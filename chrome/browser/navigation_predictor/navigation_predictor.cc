// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor.h"

#include <memory>

#include "base/check_op.h"
#include "base/hash/hash.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
#include "base/system/sys_info.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/page_load_metrics/observers/page_anchors_metrics_observer.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace {

// The maximum number of clicks to track in a single navigation.
size_t kMaxClicksTracked = 10;

bool IsPrerendering(content::RenderFrameHost& render_frame_host) {
  return render_frame_host.GetLifecycleState() ==
         content::RenderFrameHost::LifecycleState::kPrerendering;
}

}  // namespace

NavigationPredictor::NavigationPredictor(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<AnchorElementMetricsHost> receiver)
    : content::DocumentService<blink::mojom::AnchorElementMetricsHost>(
          render_frame_host,
          std::move(receiver)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // When using content::Page::IsPrimary, bfcache can cause returning a false in
  // the back/forward navigation. So, DCHECK only checks if current page is
  // prerendering until deciding how to handle bfcache navigations. See also
  // https://crbug.com/1239310.
  DCHECK(!IsPrerendering(render_frame_host));

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
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));
  DCHECK(!IsPrerendering(*render_frame_host));

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

int NavigationPredictor::GetBucketMinForPageMetrics(int value) const {
  return ukm::GetExponentialBucketMin(value, 1.3);
}

int NavigationPredictor::GetLinearBucketForLinkLocation(int value) const {
  return ukm::GetLinearBucketMin(static_cast<int64_t>(value), 10);
}

int NavigationPredictor::GetLinearBucketForRatioArea(int value) const {
  return ukm::GetLinearBucketMin(static_cast<int64_t>(value), 5);
}

void NavigationPredictor::ReportNewAnchorElements(
    std::vector<blink::mojom::AnchorElementMetricsPtr> elements) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));
  DCHECK(!IsPrerendering(render_frame_host()));

  // Create the AnchorsData object for this WebContents if it doesn't already
  // exist. Note that NavigationPredictor only runs on the main frame, but get
  // reports for links from all same-process iframes.
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  PageAnchorsMetricsObserver::AnchorsData::CreateForWebContents(web_contents);
  PageAnchorsMetricsObserver::AnchorsData* data =
      PageAnchorsMetricsObserver::AnchorsData::FromWebContents(web_contents);
  DCHECK(data);
  GURL document_url;
  std::vector<GURL> new_predictions;
  for (auto& element : elements) {
    uint32_t anchor_id = element->anchor_id;
    if (anchors_.find(anchor_id) != anchors_.end()) {
      continue;
    }

    data->number_of_anchors_++;
    if (element->contains_image) {
      data->number_of_anchors_contains_image_++;
    }
    if (element->is_url_incremented_by_one) {
      data->number_of_anchors_url_incremented_++;
    }
    if (element->is_in_iframe) {
      data->number_of_anchors_in_iframe_++;
    }
    if (element->is_same_host) {
      data->number_of_anchors_same_host_++;
    }
    data->viewport_height_ = element->viewport_size.height();
    data->viewport_width_ = element->viewport_size.width();
    data->total_clickable_space_ += element->ratio_area * 100;
    data->link_locations_.push_back(element->ratio_distance_top_to_visible_top);

    // Collect the target URL if it is new, without ref (# fragment).
    GURL::Replacements replacements;
    replacements.ClearRef();
    document_url = element->source_url.ReplaceComponents(replacements);
    GURL target_url = element->target_url.ReplaceComponents(replacements);
    if (target_url != document_url &&
        predicted_urls_.find(target_url) == predicted_urls_.end()) {
      predicted_urls_.insert(target_url);
      new_predictions.push_back(target_url);
    }

    anchors_.emplace(anchor_id, std::move(element));
    tracked_anchor_id_to_index_[anchor_id] = tracked_anchor_id_to_index_.size();
  }

  if (!new_predictions.empty()) {
    NavigationPredictorKeyedService* service =
        NavigationPredictorKeyedServiceFactory::GetForProfile(
            Profile::FromBrowserContext(
                render_frame_host().GetBrowserContext()));
    DCHECK(service);
    service->OnPredictionUpdated(
        web_contents, document_url,
        NavigationPredictorKeyedService::PredictionSource::
            kAnchorElementsParsedFromWebPage,
        new_predictions);
  }
}

void NavigationPredictor::ReportAnchorElementClick(
    blink::mojom::AnchorElementClickPtr click) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));
  DCHECK(!IsPrerendering(render_frame_host()));

  clicked_count_++;
  if (clicked_count_ > kMaxClicksTracked)
    return;

  if (!ukm_recorder_) {
    return;
  }

  // An anchor index of -1 indicates that we are not going to log details about
  // the anchor that was clicked.
  int anchor_index = -1;
  auto index_it = tracked_anchor_id_to_index_.find(click->anchor_id);
  if (index_it != tracked_anchor_id_to_index_.end()) {
    anchor_index = index_it->second;
  }

  ukm::builders::NavigationPredictorPageLinkClick builder(ukm_source_id_);
  builder.SetAnchorElementIndex(anchor_index);
  auto it = anchors_.find(click->anchor_id);
  if (it != anchors_.end()) {
    builder.SetHrefUnchanged(it->second->target_url == click->target_url);
  }
  builder.Record(ukm_recorder_);
}

void NavigationPredictor::ReportAnchorElementsEnteredViewport(
    std::vector<blink::mojom::AnchorElementEnteredViewportPtr> elements) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNavigationPredictor));
  DCHECK(!IsPrerendering(render_frame_host()));

  if (elements.empty()) {
    return;
  }

  for (const auto& element : elements) {
    if (anchors_.find(element->anchor_id) == anchors_.end()) {
      // We don't know about this anchor, likely because at its first paint,
      // AnchorElementMetricsSender didn't send it to NavigationPredictor.
      // Reasons could be that the link had non-HTTP scheme, the anchor had
      // zero width/height, etc.
      continue;
    }
    const auto& anchor = anchors_[element->anchor_id];
    // Collect the target URL if it is new, without ref (# fragment).
    GURL::Replacements replacements;
    replacements.ClearRef();
    GURL document_url = anchor->source_url.ReplaceComponents(replacements);
    GURL target_url = anchor->target_url.ReplaceComponents(replacements);
    if (target_url == document_url) {
      // Ignore anchors pointing to the same document.
      continue;
    }

    if (!ukm_recorder_) {
      continue;
    }

    auto index_it = tracked_anchor_id_to_index_.find(element->anchor_id);
    if (index_it == tracked_anchor_id_to_index_.end()) {
      // We're not tracking this element, no need to generate a
      // NavigationPredictorAnchorElementMetrics record.
      continue;
    }
    ukm::builders::NavigationPredictorAnchorElementMetrics
        anchor_element_builder(ukm_source_id_);

    anchor_element_builder.SetAnchorIndex(index_it->second);
    anchor_element_builder.SetIsInIframe(anchor->is_in_iframe);
    anchor_element_builder.SetIsURLIncrementedByOne(
        anchor->is_url_incremented_by_one);
    anchor_element_builder.SetContainsImage(anchor->contains_image);
    anchor_element_builder.SetSameOrigin(anchor->is_same_host);
    anchor_element_builder.SetHasTextSibling(anchor->has_text_sibling ? 1 : 0);
    anchor_element_builder.SetIsBold(anchor->font_weight > 500 ? 1 : 0);
    anchor_element_builder.SetNavigationStartToLinkLoggedMs(
        ukm::GetExponentialBucketMin(
            element->navigation_start_to_entered_viewport_ms, 1.3));

    uint32_t font_size_bucket;
    if (anchor->font_size_px < 10) {
      font_size_bucket = 1;
    } else if (anchor->font_size_px < 18) {
      font_size_bucket = 2;
    } else {
      font_size_bucket = 3;
    }
    anchor_element_builder.SetFontSize(font_size_bucket);

    base::StringPiece path = anchor->target_url.path_piece();
    int64_t path_length = path.length();
    path_length = ukm::GetLinearBucketMin(path_length, 10);
    // Truncate at 100 characters.
    path_length = std::min(path_length, static_cast<int64_t>(100));
    anchor_element_builder.SetPathLength(path_length);

    int64_t num_slashes = base::ranges::count(path, '/');
    // Truncate at 5.
    num_slashes = std::min(num_slashes, static_cast<int64_t>(5));
    anchor_element_builder.SetPathDepth(num_slashes);

    // 10-bucket hash of the URL's path.
    uint32_t hash = base::PersistentHash(path.data(), path.length());
    anchor_element_builder.SetBucketedPathHash(hash % 10);

    // Convert the ratio area and ratio distance from [0,1] to [0,100].
    int percent_ratio_area = static_cast<int>(anchor->ratio_area * 100);
    int percent_ratio_distance_root_top =
        static_cast<int>(anchor->ratio_distance_root_top * 100);

    anchor_element_builder.SetPercentClickableArea(
        GetLinearBucketForRatioArea(percent_ratio_area));
    anchor_element_builder.SetPercentVerticalDistance(
        GetLinearBucketForLinkLocation(percent_ratio_distance_root_top));

    anchor_element_builder.Record(ukm_recorder_);
  }
}
