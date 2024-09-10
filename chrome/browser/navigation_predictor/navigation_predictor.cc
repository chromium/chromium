// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/hash/hash.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/system/sys_info.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/navigation_predictor/preloading_model_keyed_service.h"
#include "chrome/browser/navigation_predictor/preloading_model_keyed_service_factory.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace {

// The maximum number of clicks to track in a single navigation.
constexpr size_t kMaxClicksTracked = 10;

bool IsPrerendering(content::RenderFrameHost& render_frame_host) {
  return render_frame_host.GetLifecycleState() ==
         content::RenderFrameHost::LifecycleState::kPrerendering;
}

NavigationPredictor::FontSizeBucket GetFontSizeFromPx(uint32_t font_size_px) {
  if (font_size_px < 10) {
    return NavigationPredictor::kLessThanTen;
  } else if (font_size_px < 18) {
    return NavigationPredictor::kTenToSeventeen;
  } else {
    return NavigationPredictor::kEighteenOrGreater;
  }
}

bool IsBoldFont(uint32_t font_weight) {
  return font_weight > 500;
}

struct PathLengthDepthAndHash {
  // `path_length` caps at 100.
  uint8_t path_length;
  // `path_depth` caps at 5.
  uint8_t path_depth;
  // 10-bucket hash.
  uint8_t hash_bucket;
};

PathLengthDepthAndHash GetUrlPathLengthDepthAndHash(const GURL& target_url) {
  std::string_view path = target_url.path_piece();
  int64_t path_length = path.length();
  path_length = ukm::GetLinearBucketMin(path_length, 10);
  // Truncate at 100 characters.
  path_length = std::min(path_length, static_cast<int64_t>(100));

  int num_slashes = base::ranges::count(path, '/');
  // Truncate at 5.
  int path_depth = std::min(num_slashes, 5);

  // 10-bucket hash of the URL's path.
  uint32_t hash = base::PersistentHash(path);
  uint8_t hash_bucket = hash % 10;

  return {static_cast<uint8_t>(path_length), static_cast<uint8_t>(path_depth),
          hash_bucket};
}

// Returns the minimum of the bucket that |value| belongs in, used for
// |ratio_distance_root_top|.
int GetLinearBucketForLinkLocation(int value) {
  return ukm::GetLinearBucketMin(static_cast<int64_t>(value), 10);
}

// Returns the minimum of the bucket that |value| belongs in, used for
// |ratio_area|.
int GetLinearBucketForRatioArea(int value) {
  return ukm::GetLinearBucketMin(static_cast<int64_t>(value), 5);
}

base::TimeDelta MLModelExecutionTimerStartDelay() {
  static int timer_start_delay =
      blink::features::kPreloadingModelTimerStartDelay.Get();
  return base::Milliseconds(timer_start_delay);
}

base::TimeDelta MLModelExecutionTimerInterval() {
  static int timer_interval =
      blink::features::kPreloadingModelTimerInterval.Get();
  return base::Milliseconds(timer_interval);
}

bool MLModelOneExecutionPerHover() {
  static bool one_execution_per_hover =
      blink::features::kPreloadingModelOneExecutionPerHover.Get();
  return one_execution_per_hover;
}

base::TimeDelta MLModelMaxHoverTime() {
  static const base::TimeDelta max_hover_time =
      blink::features::kPreloadingModelMaxHoverTime.Get();
  return max_hover_time;
}

void RecordMetricsForModelTraining(
    const PreloadingModelKeyedService::Inputs& inputs,
    ukm::SourceId ukm_source,
    std::optional<double> sampling_likelihood,
    bool is_accurate) {
  constexpr double kBucketSpacing = 1.3;

  const int sampling_likelihood_per_million =
      static_cast<int>(1'000'000 * sampling_likelihood.value_or(1.0));
  const int sampling_amount_bucket = ukm::GetExponentialBucketMin(
      1'000'000 - sampling_likelihood_per_million, kBucketSpacing);

  ukm::builders::Preloading_NavigationPredictorModelTrainingData builder(
      ukm_source);

  builder.SetSamplingAmount(sampling_amount_bucket);
  builder.SetIsAccurate(is_accurate);
  builder.SetContainsImage(inputs.contains_image);
  // Font size is already bucketed. See `FontSizeBucket`.
  builder.SetFontSize(inputs.font_size);
  builder.SetHasTextSibling(inputs.has_text_sibling);
  builder.SetIsBold(inputs.is_bold);
  builder.SetIsInIframe(inputs.is_in_iframe);
  builder.SetIsURLIncrementedByOne(inputs.is_url_incremented_by_one);
  builder.SetNavigationStartToLinkLoggedMs(ukm::GetExponentialBucketMin(
      inputs.navigation_start_to_link_logged.InMilliseconds(), kBucketSpacing));
  builder.SetPathDepth(inputs.path_depth);
  // Path length is already bucketed.
  DCHECK_EQ(
      inputs.path_length,
      ukm::GetLinearBucketMin(static_cast<int64_t>(inputs.path_length), 10));
  builder.SetPathLength(inputs.path_length);
  builder.SetPercentClickableArea(
      GetLinearBucketForRatioArea(inputs.percent_clickable_area));
  builder.SetPercentVerticalDistance(
      GetLinearBucketForLinkLocation(inputs.percent_vertical_distance));
  builder.SetSameHost(inputs.is_same_host);
  builder.SetHoverDwellTimeMs(ukm::GetExponentialBucketMin(
      inputs.hover_dwell_time.InMilliseconds(), kBucketSpacing));
  builder.SetPointerHoveringOverCount(ukm::GetExponentialBucketMin(
      inputs.pointer_hovering_over_count, kBucketSpacing));

  builder.Record(ukm::UkmRecorder::Get());
}

bool MaySendTraffic() {
  // TODO(b/290223353): Due to concerns about the amount of traffic this feature
  // would create on desktop, we'll just enable for a random sample of clients.
  // We should scale up the percentage of enabled clients.
  // Note that NavigationPredictor has functionality, unrelated to sending
  // requests, which continues to run regardless of this parameter.
  static const bool may_send_traffic = [] {
    // Use a fixed state for benchmarking.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            variations::switches::kEnableBenchmarking)) {
#if BUILDFLAG(IS_ANDROID)
      return true;
#else
      return false;
#endif
    }

    int enabled_percent =
        blink::features::kPredictorTrafficClientEnabledPercent.Get();

    // This isn't user facing, so we'll just re-roll for each session.
    return base::RandInt(0, 99) < enabled_percent;
  }();

  return may_send_traffic;
}

}  // namespace

NavigationPredictor::AnchorElementData::AnchorElementData(
    blink::mojom::AnchorElementMetricsPtr metrics,
    base::TimeTicks first_report_timestamp)
    : ratio_distance_root_top(metrics->ratio_distance_root_top),
      ratio_area(static_cast<uint8_t>(metrics->ratio_area * 100)),
      is_in_iframe(metrics->is_in_iframe),
      contains_image(metrics->contains_image),
      is_same_host(metrics->is_same_host),
      is_url_incremented_by_one(metrics->is_url_incremented_by_one),
      has_text_sibling(metrics->has_text_sibling),
      is_bold_font(IsBoldFont(metrics->font_weight)),
      font_size(GetFontSizeFromPx(metrics->font_size_px)),
      target_url(metrics->target_url),
      first_report_timestamp(first_report_timestamp) {}

NavigationPredictor::AnchorElementData::~AnchorElementData() = default;

NavigationPredictor::NavigationPredictor(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<AnchorElementMetricsHost> receiver)
    : content::DocumentService<blink::mojom::AnchorElementMetricsHost>(
          render_frame_host,
          std::move(receiver)),
      clock_(base::DefaultTickClock::GetInstance()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // When using content::Page::IsPrimary, bfcache can cause returning a false in
  // the back/forward navigation. So, DCHECK only checks if current page is
  // prerendering until deciding how to handle bfcache navigations. See also
  // https://crbug.com/1239310.
  DCHECK(!IsPrerendering(render_frame_host));

  navigation_start_ = NowTicks();
  ukm_recorder_ = ukm::UkmRecorder::Get();
  ukm_source_id_ = render_frame_host.GetMainFrame()->GetPageUkmSourceId();
}

NavigationPredictor::~NavigationPredictor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NavigationPredictor::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::AnchorElementMetricsHost> receiver) {
  CHECK(render_frame_host);
  CHECK(!IsPrerendering(*render_frame_host));

  if (!base::FeatureList::IsEnabled(blink::features::kNavigationPredictor)) {
    return;
  }

  // Only valid for the main frame.
  if (render_frame_host->GetParentOrOuterDocument())
    return;

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;

  DCHECK(web_contents->GetBrowserContext());
  if (web_contents->GetBrowserContext()->IsOffTheRecord()) {
    return;
  }

  // The object is bound to the lifetime of the |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new NavigationPredictor(*render_frame_host, std::move(receiver));
}

NavigationPredictorMetricsDocumentData&
NavigationPredictor::GetNavigationPredictorMetricsDocumentData() const {
  // Create the `NavigationPredictorMetricsDocumentData` object for this
  // document if it doesn't already exist.
  NavigationPredictorMetricsDocumentData* data =
      NavigationPredictorMetricsDocumentData::GetOrCreateForCurrentDocument(
          &render_frame_host());
  DCHECK(data);
  return *data;
}

void NavigationPredictor::ReportNewAnchorElements(
    std::vector<blink::mojom::AnchorElementMetricsPtr> elements,
    const std::vector<uint32_t>& removed_elements) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));
  DCHECK(!IsPrerendering(render_frame_host()));

  // Create the AnchorsData object for this WebContents if it doesn't already
  // exist. Note that NavigationPredictor only runs on the main frame, but get
  // reports for links from all same-process iframes.
  NavigationPredictorMetricsDocumentData::AnchorsData& data =
      GetNavigationPredictorMetricsDocumentData().GetAnchorsData();
  const GURL document_url =
      render_frame_host().GetLastCommittedURL().GetWithoutRef();
  if (!document_url.is_valid()) {
    return;
  }
  std::vector<GURL> new_predictions;
  const base::TimeTicks now = NowTicks();
  for (auto& element : elements) {
    AnchorId anchor_id(element->anchor_id);
    if (anchors_.find(anchor_id) != anchors_.end()) {
      continue;
    }

    auto [id_it, id_inserted] = tracked_anchor_id_to_index_.insert(
        {anchor_id, tracked_anchor_id_to_index_.size()});

    // We may have seen this anchor before, but it was removed from the page, so
    // we stopped tracking it. We'll start tracking it again, but not treat it
    // as a new anchor.
    if (id_inserted) {
      data.number_of_anchors_++;
      if (element->contains_image) {
        data.number_of_anchors_contains_image_++;
      }
      if (element->is_url_incremented_by_one) {
        data.number_of_anchors_url_incremented_++;
      }
      if (element->is_in_iframe) {
        data.number_of_anchors_in_iframe_++;
      }
      if (element->is_same_host) {
        data.number_of_anchors_same_host_++;
      }
      data.viewport_height_ = element->viewport_size.height();
      data.viewport_width_ = element->viewport_size.width();
      data.total_clickable_space_ += element->ratio_area * 100;
      data.link_locations_.push_back(
          element->ratio_distance_top_to_visible_top);

      // Collect the target URL if it is new, without ref (# fragment).
      GURL target_url = element->target_url.GetWithoutRef();
      if (target_url != document_url) {
        auto [url_it, url_inserted] =
            predicted_urls_.insert(base::FastHash(target_url.spec()));
        if (url_inserted) {
          new_predictions.push_back(std::move(target_url));
        }
      }
    }

    anchors_.emplace(std::piecewise_construct, std::forward_as_tuple(anchor_id),
                     std::forward_as_tuple(std::move(element), now));
  }

  for (uint32_t removed_element : removed_elements) {
    AnchorId anchor_id(removed_element);
    // Stop tracking removed elements to conserve memory. We leave an entry in
    // `tracked_anchor_id_to_index_` to detect if a removed element is re-added
    // to the page.
    anchors_.erase(anchor_id);
  }

  if (!new_predictions.empty() && MaySendTraffic()) {
    NavigationPredictorKeyedService* service =
        NavigationPredictorKeyedServiceFactory::GetForProfile(
            Profile::FromBrowserContext(
                render_frame_host().GetBrowserContext()));
    DCHECK(service);
    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(&render_frame_host());

    service->OnPredictionUpdated(
        web_contents, document_url,
        NavigationPredictorKeyedService::PredictionSource::
            kAnchorElementsParsedFromWebPage,
        new_predictions);
  }
}

void NavigationPredictor::OnPreloadingHeuristicsModelDone(
    GURL url,
    PreloadingModelKeyedService::Result result) {
  if (!result.has_value()) {
    return;
  }
  render_frame_host().OnPreloadingHeuristicsModelDone(url, result.value());
}

void NavigationPredictor::ProcessPointerEventUsingMLModel(
    blink::mojom::AnchorElementPointerEventForMLModelPtr pointer_event) {
  // Find anchor elements data.
  AnchorId anchor_id(pointer_event->anchor_id);
  auto it = anchors_.find(anchor_id);
  if (it == anchors_.end()) {
    return;
  }

  AnchorElementData& anchor = it->second;
  switch (pointer_event->user_interaction_event_type) {
    case blink::mojom::AnchorElementUserInteractionEventForMLModelType::
        kPointerOut: {
      anchor.pointer_over_timestamp.reset();
      ml_model_candidate_.reset();
      break;
    }
    case blink::mojom::AnchorElementUserInteractionEventForMLModelType::
        kPointerOver: {
      // Currently we only process mouse based events.
      if (!pointer_event->is_mouse) {
        return;
      }
      // Ignore anchors pointing to the same document.
      if (IsTargetURLTheSameAsDocument(anchor)) {
        return;
      }

      anchor.pointer_over_timestamp = NowTicks();
      anchor.pointer_hovering_over_count++;
      ml_model_candidate_ = anchor_id;
      if (!ml_model_execution_timer_.IsRunning()) {
        ml_model_execution_timer_.Start(
            FROM_HERE, MLModelExecutionTimerStartDelay(),
            base::BindOnce(&NavigationPredictor::OnMLModelExecutionTimerFired,
                           base::Unretained(this)));
      }
      break;
    }
    default:
      break;
  }
}

void NavigationPredictor::OnMLModelExecutionTimerFired() {
  // Check whether preloading is enabled or not.
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  if (prefetch::IsSomePreloadingEnabled(*profile->GetPrefs()) !=
      content::PreloadingEligibility::kEligible) {
    return;
  }

  // Execute the model.
  PreloadingModelKeyedService* model_service =
      PreloadingModelKeyedServiceFactory::GetForProfile(profile);
  if (!model_service) {
    return;
  }

  if (!ml_model_candidate_.has_value()) {
    return;
  }
  auto it = anchors_.find(ml_model_candidate_.value());
  if (it == anchors_.end()) {
    return;
  }

  AnchorElementData& anchor = it->second;

  PreloadingModelKeyedService::Inputs inputs;
  inputs.contains_image = anchor.contains_image;
  inputs.font_size = anchor.font_size;
  inputs.has_text_sibling = anchor.has_text_sibling;
  inputs.is_bold = anchor.is_bold_font;
  inputs.is_in_iframe = anchor.is_in_iframe;
  inputs.is_url_incremented_by_one = anchor.is_url_incremented_by_one;
  inputs.navigation_start_to_link_logged =
      anchor.first_report_timestamp - navigation_start_;
  auto path_info = GetUrlPathLengthDepthAndHash(anchor.target_url);
  inputs.path_length = path_info.path_length;
  inputs.path_depth = path_info.path_depth;
  inputs.percent_clickable_area = anchor.ratio_area;
  inputs.percent_vertical_distance =
      static_cast<int>(anchor.ratio_distance_root_top * 100);

  inputs.is_same_host = anchor.is_same_host;
  auto to_timedelta = [this](std::optional<base::TimeTicks> ts) {
    return ts.has_value() ? NowTicks() - ts.value() : base::TimeDelta();
  };
  // TODO(329691634): Using the real viewport entry time for
  // `entered_viewport_to_left_viewport` produces low quality results.
  // We could remove it from the model, if we can't get this to be useful.
  inputs.entered_viewport_to_left_viewport = base::TimeDelta();
  inputs.hover_dwell_time = to_timedelta(anchor.pointer_over_timestamp);
  inputs.pointer_hovering_over_count = anchor.pointer_hovering_over_count;
  if (model_score_callback_) {
    std::move(model_score_callback_).Run(inputs);
  }

  content::PreloadingData* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(
          content::WebContents::FromRenderFrameHost(&render_frame_host()));
  preloading_data->OnPreloadingHeuristicsModelInput(
      anchor.target_url,
      base::BindOnce(&RecordMetricsForModelTraining, inputs,
                     render_frame_host().GetPageUkmSourceId()));
  model_service->Score(
      &scoring_model_task_tracker_, inputs,
      base::BindOnce(&NavigationPredictor::OnPreloadingHeuristicsModelDone,
                     weak_ptr_factory_.GetWeakPtr(), anchor.target_url));

  // TODO(crbug.com/40278151): In its current form, the model does not seem to
  // ever increase in confidence when dwelling on an anchor, which makes
  // repeated executions wasteful. So we only do one execution per mouse over.
  // As we iterate on the model, multiple executions may become useful, but we
  // need to take care to not produce a large amount of redundant predictions
  // (as seen in crbug.com/338200075 ).
  if (!MLModelOneExecutionPerHover() &&
      inputs.hover_dwell_time < MLModelMaxHoverTime() &&
      !ml_model_execution_timer_.IsRunning()) {
    ml_model_execution_timer_.Start(
        FROM_HERE, MLModelExecutionTimerInterval(),
        base::BindOnce(&NavigationPredictor::OnMLModelExecutionTimerFired,
                       base::Unretained(this)));
  }
}

void NavigationPredictor::SetModelScoreCallbackForTesting(
    ModelScoreCallbackForTesting callback) {
  model_score_callback_ = std::move(callback);
}

void NavigationPredictor::SetTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* clock) {
  ml_model_execution_timer_.SetTaskRunner(task_runner);
  clock_ = clock;
  navigation_start_ = NowTicks();
}

// static
bool NavigationPredictor::disable_renderer_metric_sending_delay_for_testing_ =
    false;

// static
void NavigationPredictor::DisableRendererMetricSendingDelayForTesting() {
  disable_renderer_metric_sending_delay_for_testing_ = true;
}

void NavigationPredictor::ShouldSkipUpdateDelays(
    ShouldSkipUpdateDelaysCallback callback) {
  std::move(callback).Run(disable_renderer_metric_sending_delay_for_testing_);
}

void NavigationPredictor::ReportAnchorElementClick(
    blink::mojom::AnchorElementClickPtr click) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));
  DCHECK(!IsPrerendering(render_frame_host()));

  navigation_start_to_click_ = click->navigation_start_to_click;

  clicked_count_++;
  if (clicked_count_ > kMaxClicksTracked)
    return;

  if (!ukm_recorder_) {
    return;
  }

  auto& navigation_predictor_metrics_data =
      GetNavigationPredictorMetricsDocumentData();
  // An anchor index of -1 indicates that we are not going to log details about
  // the anchor that was clicked.
  int anchor_index = -1;
  AnchorId anchor_id(click->anchor_id);
  auto index_it = tracked_anchor_id_to_index_.find(anchor_id);
  if (index_it != tracked_anchor_id_to_index_.end()) {
    anchor_index = index_it->second;

    // Record PreloadOnHover.HoverTakenMs and PreloadOnHover.PointerDownTakenMs
    // to UKM. We should make sure that we only process the `sampled` anchor
    // elements here, as `AnchorElementMetricsSender` reports all new anchor
    // elements to `NavigationPredictor`, but only reports user interactions
    // events for the  `sampled` anchors. Otherwise, we will end up creating
    // empty `UserInteractionsData` UKM records.
    auto& user_interactions =
        navigation_predictor_metrics_data.GetUserInteractionsData();
    auto user_interaction_it = user_interactions.find(index_it->second);
    if (user_interaction_it != user_interactions.end()) {
      auto& user_interaction = user_interaction_it->second;

      // navigation_start_to_click_ is set to click->navigation_start_to_click
      // and should always have a value.
      CHECK(navigation_start_to_click_.has_value());
      if (user_interaction.last_navigation_start_to_pointer_over.has_value() ||
          user_interaction.last_navigation_start_to_last_pointer_down
              .has_value()) {
        NavigationPredictorMetricsDocumentData::PreloadOnHoverData
            preload_on_hover;
        preload_on_hover.taken = true;
        if (user_interaction.last_navigation_start_to_pointer_over
                .has_value()) {
          // `hover_dwell_time` measures the time delta from the last mouse over
          // event to the last mouse click event.
          preload_on_hover.hover_dwell_time =
              navigation_start_to_click_.value() -
              user_interaction.last_navigation_start_to_pointer_over.value();
        }
        if (user_interaction.last_navigation_start_to_last_pointer_down
                .has_value()) {
          // `pointer_down_duration` measures the time delta from the last mouse
          // down event to the last mouse click event.
          preload_on_hover.pointer_down_duration =
              navigation_start_to_click_.value() -
              user_interaction.last_navigation_start_to_last_pointer_down
                  .value();
          user_interaction.last_navigation_start_to_last_pointer_down.reset();
        }
        navigation_predictor_metrics_data.AddPreloadOnHoverData(
            std::move(preload_on_hover));
      }
    }
  }

  NavigationPredictorMetricsDocumentData::PageLinkClickData page_link_click;
  page_link_click.anchor_element_index_ = anchor_index;
  auto it = anchors_.find(anchor_id);
  if (it != anchors_.end()) {
    page_link_click.href_unchanged_ =
        (it->second.target_url == click->target_url);
  }
  navigation_start_to_click_ = click->navigation_start_to_click;
  // navigation_start_to_click_ is set to click->navigation_start_to_click and
  // should always have a value.
  CHECK(navigation_start_to_click_.has_value());

  navigation_predictor_metrics_data.SetNavigationStartToClick(
      navigation_start_to_click_.value());

  page_link_click.navigation_start_to_link_clicked_ =
      navigation_start_to_click_.value();
  navigation_predictor_metrics_data.AddPageLinkClickData(
      std::move(page_link_click));
}

void NavigationPredictor::ReportAnchorElementsLeftViewport(
    std::vector<blink::mojom::AnchorElementLeftViewportPtr> elements) {
  auto& user_interactions =
      GetNavigationPredictorMetricsDocumentData().GetUserInteractionsData();
  for (const auto& element : elements) {
    auto index_it =
        tracked_anchor_id_to_index_.find(AnchorId(element->anchor_id));
    if (index_it == tracked_anchor_id_to_index_.end()) {
      continue;
    }
    auto& user_interaction = user_interactions[index_it->second];
    user_interaction.is_in_viewport = false;
    user_interaction.last_navigation_start_to_entered_viewport.reset();
    user_interaction.max_time_in_viewport = std::max(
        user_interaction.max_time_in_viewport.value_or(base::TimeDelta()),
        element->time_in_viewport);
    user_interaction.percent_vertical_position.reset();
    user_interaction.percent_distance_from_pointer_down.reset();
  }
}

void NavigationPredictor::ReportAnchorElementsPositionUpdate(
    std::vector<blink::mojom::AnchorElementPositionUpdatePtr> elements) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kNavigationPredictorNewViewportFeatures)) {
    ReportBadMessageAndDeleteThis(
        "ReportAnchorElementsPositionUpdate should only be called with "
        "kNavigationPredictorNewViewportFeatures enabled.");
    return;
  }

  auto& user_interactions =
      GetNavigationPredictorMetricsDocumentData().GetUserInteractionsData();
  for (const auto& element : elements) {
    auto index_it =
        tracked_anchor_id_to_index_.find(AnchorId(element->anchor_id));
    if (index_it == tracked_anchor_id_to_index_.end()) {
      continue;
    }
    auto& user_interaction = user_interactions[index_it->second];
    user_interaction.percent_vertical_position =
        base::saturated_cast<int>(element->vertical_position_ratio * 100);
    if (element->distance_from_pointer_down_ratio.has_value()) {
      user_interaction.percent_distance_from_pointer_down =
          base::saturated_cast<int>(
              element->distance_from_pointer_down_ratio.value() * 100);
    }
  }
}

void NavigationPredictor::ReportAnchorElementPointerDataOnHoverTimerFired(
    blink::mojom::AnchorElementPointerDataOnHoverTimerFiredPtr msg) {
  if (!msg->pointer_data || !msg->pointer_data->is_mouse_pointer) {
    return;
  }

  auto& user_interactions =
      GetNavigationPredictorMetricsDocumentData().GetUserInteractionsData();
  auto index_it = tracked_anchor_id_to_index_.find(AnchorId(msg->anchor_id));
  if (index_it == tracked_anchor_id_to_index_.end()) {
    return;
  }

  auto& user_interaction = user_interactions[index_it->second];
  user_interaction.mouse_velocity = msg->pointer_data->mouse_velocity;
  user_interaction.mouse_acceleration = msg->pointer_data->mouse_acceleration;
}

void NavigationPredictor::ReportAnchorElementPointerOver(
    blink::mojom::AnchorElementPointerOverPtr pointer_over_event) {
  auto& user_interactions =
      GetNavigationPredictorMetricsDocumentData().GetUserInteractionsData();
  auto index_it =
      tracked_anchor_id_to_index_.find(AnchorId(pointer_over_event->anchor_id));
  if (index_it == tracked_anchor_id_to_index_.end()) {
    return;
  }

  auto& user_interaction = user_interactions[index_it->second];
  if (!user_interaction.is_hovered) {
    user_interaction.pointer_hovering_over_count++;
  }
  user_interaction.is_hovered = true;
  user_interaction.last_navigation_start_to_pointer_over =
      pointer_over_event->navigation_start_to_pointer_over;
}

void NavigationPredictor::ReportAnchorElementPointerOut(
    blink::mojom::AnchorElementPointerOutPtr hover_event) {
  auto& navigation_predictor_metrics_data =
      GetNavigationPredictorMetricsDocumentData();
  auto& user_interactions =
      navigation_predictor_metrics_data.GetUserInteractionsData();
  auto index_it =
      tracked_anchor_id_to_index_.find(AnchorId(hover_event->anchor_id));
  if (index_it == tracked_anchor_id_to_index_.end()) {
    return;
  }

  auto& user_interaction = user_interactions[index_it->second];
  // Record PreloadOnHover.HoverNotTakenMs and
  // PreloadOnHover.MouseDownNotTakenMs to UKM.
  NavigationPredictorMetricsDocumentData::PreloadOnHoverData preload_on_hover;
  preload_on_hover.taken = false;
  preload_on_hover.hover_dwell_time = hover_event->hover_dwell_time;
  if (user_interaction.last_navigation_start_to_last_pointer_down.has_value() &&
      user_interaction.last_navigation_start_to_pointer_over.has_value()) {
    preload_on_hover.pointer_down_duration =
        user_interaction.last_navigation_start_to_pointer_over.value() +
        hover_event->hover_dwell_time -
        user_interaction.last_navigation_start_to_last_pointer_down.value();
    user_interaction.last_navigation_start_to_last_pointer_down.reset();
  }
  navigation_predictor_metrics_data.AddPreloadOnHoverData(
      std::move(preload_on_hover));

  // Update user interactions.
  user_interaction.is_hovered = false;
  user_interaction.last_navigation_start_to_pointer_over.reset();
  user_interaction.max_hover_dwell_time = std::max(
      hover_event->hover_dwell_time,
      user_interaction.max_hover_dwell_time.value_or(base::TimeDelta()));
}

void NavigationPredictor::ReportAnchorElementPointerDown(
    blink::mojom::AnchorElementPointerDownPtr pointer_down_event) {
  auto index_it =
      tracked_anchor_id_to_index_.find(AnchorId(pointer_down_event->anchor_id));
  if (index_it == tracked_anchor_id_to_index_.end()) {
    return;
  }

  auto& user_interactions =
      GetNavigationPredictorMetricsDocumentData().GetUserInteractionsData();
  auto& user_interaction = user_interactions[index_it->second];
  user_interaction.last_navigation_start_to_last_pointer_down =
      pointer_down_event->navigation_start_to_pointer_down;
}

void NavigationPredictor::ReportAnchorElementsEnteredViewport(
    std::vector<blink::mojom::AnchorElementEnteredViewportPtr> elements) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));
  DCHECK(!IsPrerendering(render_frame_host()));

  if (elements.empty()) {
    return;
  }
  auto& navigation_predictor_metrics_data =
      GetNavigationPredictorMetricsDocumentData();
  auto& user_interactions =
      navigation_predictor_metrics_data.GetUserInteractionsData();
  for (const auto& element : elements) {
    AnchorId anchor_id(element->anchor_id);
    auto index_it = tracked_anchor_id_to_index_.find(anchor_id);
    if (index_it == tracked_anchor_id_to_index_.end()) {
      // We're not tracking this element, no need to generate a
      // NavigationPredictorAnchorElementMetrics record.
      continue;
    }
    auto& user_interaction = user_interactions[index_it->second];
    if (!user_interaction.is_in_viewport) {
      user_interaction.entered_viewport_count++;
    }
    user_interaction.is_in_viewport = true;
    user_interaction.last_navigation_start_to_entered_viewport =
        element->navigation_start_to_entered_viewport;

    auto anchor_it = anchors_.find(anchor_id);
    if (anchor_it == anchors_.end()) {
      // We don't know about this anchor, likely because at its first paint,
      // AnchorElementMetricsSender didn't send it to NavigationPredictor.
      // Reasons could be that the link had non-HTTP scheme, the anchor had
      // zero width/height, etc.
      continue;
    }
    const AnchorElementData& anchor = anchor_it->second;
    // Collect the target URL if it is new, without ref (# fragment).
    if (IsTargetURLTheSameAsDocument(anchor)) {
      // Ignore anchors pointing to the same document.
      continue;
    }

    if (!ukm_recorder_) {
      continue;
    }

    NavigationPredictorMetricsDocumentData::AnchorElementMetricsData metrics;

    metrics.is_in_iframe_ = anchor.is_in_iframe;
    metrics.is_url_incremented_by_one_ = anchor.is_url_incremented_by_one;
    metrics.contains_image_ = anchor.contains_image;
    metrics.is_same_host_ = anchor.is_same_host;
    metrics.has_text_sibling_ = anchor.has_text_sibling;
    metrics.is_bold_ = anchor.is_bold_font;
    metrics.navigation_start_to_link_logged =
        element->navigation_start_to_entered_viewport;

    metrics.font_size_bucket_ = anchor.font_size;
    auto path_info = GetUrlPathLengthDepthAndHash(anchor.target_url);
    metrics.path_length_ = path_info.path_length;
    metrics.path_depth_ = path_info.path_depth;
    metrics.bucketed_path_hash_ = path_info.hash_bucket;

    int percent_ratio_area = anchor.ratio_area;
    metrics.percent_clickable_area_ =
        GetLinearBucketForRatioArea(percent_ratio_area);

    int percent_ratio_distance_root_top =
        static_cast<int>(anchor.ratio_distance_root_top * 100);
    metrics.percent_vertical_distance_ =
        GetLinearBucketForLinkLocation(percent_ratio_distance_root_top);

    navigation_predictor_metrics_data.AddAnchorElementMetricsData(
        index_it->second, std::move(metrics));
  }
}

bool NavigationPredictor::IsTargetURLTheSameAsDocument(
    const AnchorElementData& anchor) {
  return render_frame_host().GetLastCommittedURL().EqualsIgnoringRef(
      anchor.target_url);
}
