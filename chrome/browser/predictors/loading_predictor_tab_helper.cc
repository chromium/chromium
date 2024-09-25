// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor_tab_helper.h"

#include <memory>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_util.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/predictors_enums.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/predictors/predictors_switches.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/google/core/common/google_util.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/lcp_critical_path_predictor_util.h"
#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

using content::BrowserThread;

namespace predictors {

namespace {

constexpr char kLoadingPredictorOptimizationHintsReceiveStatusHistogram[] =
    "LoadingPredictor.OptimizationHintsReceiveStatus";

// Called only for subresources.
// platform/loader/fetch/README.md in blink contains more details on
// prioritization as well as links to all of the relevant places in the code
// where priority is determined. If the priority logic is updated here, be sure
// to update the other code as needed.
net::RequestPriority GetRequestPriority(
    network::mojom::RequestDestination request_destination) {
  switch (request_destination) {
    case network::mojom::RequestDestination::kStyle:
      return net::HIGHEST;

    case network::mojom::RequestDestination::kFont:
    case network::mojom::RequestDestination::kScript:
    case network::mojom::RequestDestination::kJson:
      return net::MEDIUM;

    case network::mojom::RequestDestination::kEmpty:
    case network::mojom::RequestDestination::kAudio:
    case network::mojom::RequestDestination::kAudioWorklet:
    case network::mojom::RequestDestination::kDocument:
    case network::mojom::RequestDestination::kEmbed:
    case network::mojom::RequestDestination::kFrame:
    case network::mojom::RequestDestination::kIframe:
    case network::mojom::RequestDestination::kImage:
    case network::mojom::RequestDestination::kManifest:
    case network::mojom::RequestDestination::kObject:
    case network::mojom::RequestDestination::kPaintWorklet:
    case network::mojom::RequestDestination::kReport:
    case network::mojom::RequestDestination::kServiceWorker:
    case network::mojom::RequestDestination::kSharedWorker:
    case network::mojom::RequestDestination::kTrack:
    case network::mojom::RequestDestination::kVideo:
    case network::mojom::RequestDestination::kWebBundle:
    case network::mojom::RequestDestination::kWorker:
    case network::mojom::RequestDestination::kXslt:
    case network::mojom::RequestDestination::kFencedframe:
    case network::mojom::RequestDestination::kWebIdentity:
    case network::mojom::RequestDestination::kDictionary:
    case network::mojom::RequestDestination::kSpeculationRules:
    case network::mojom::RequestDestination::kSharedStorageWorklet:
      return net::LOWEST;
  }
}

bool IsHandledNavigation(content::NavigationHandle* navigation_handle) {
  content::WebContents* web_contents = navigation_handle->GetWebContents();

  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (no_state_prefetch_manager &&
      no_state_prefetch_manager->IsWebContentsPrefetching(web_contents)) {
    return false;
  }

  return navigation_handle->IsInPrimaryMainFrame() &&
         !navigation_handle->IsSameDocument() &&
         !navigation_handle->IsPageActivation() &&
         navigation_handle->GetURL().SchemeIsHTTPOrHTTPS();
}

network::mojom::RequestDestination GetDestination(
    optimization_guide::proto::ResourceType type) {
  switch (type) {
    case optimization_guide::proto::RESOURCE_TYPE_UNKNOWN:
      return network::mojom::RequestDestination::kEmpty;
    case optimization_guide::proto::RESOURCE_TYPE_CSS:
      return network::mojom::RequestDestination::kStyle;
    case optimization_guide::proto::RESOURCE_TYPE_SCRIPT:
      return network::mojom::RequestDestination::kScript;
  }
}

bool ShouldPrefetchDestination(network::mojom::RequestDestination destination) {
  switch (features::kLoadingPredictorPrefetchSubresourceType.Get()) {
    case features::PrefetchSubresourceType::kAll:
      return true;
    case features::PrefetchSubresourceType::kCss:
      return destination == network::mojom::RequestDestination::kStyle;
    case features::PrefetchSubresourceType::kJsAndCss:
      return destination == network::mojom::RequestDestination::kScript ||
             destination == network::mojom::RequestDestination::kStyle;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// Util class for recording the status for when we received optimization hints
// for navigations that we requested them for.
class ScopedOptimizationHintsReceiveStatusRecorder {
 public:
  ScopedOptimizationHintsReceiveStatusRecorder()
      : status_(OptimizationHintsReceiveStatus::kUnknown) {}
  ~ScopedOptimizationHintsReceiveStatusRecorder() {
    DCHECK_NE(status_, OptimizationHintsReceiveStatus::kUnknown);
    UMA_HISTOGRAM_ENUMERATION(
        kLoadingPredictorOptimizationHintsReceiveStatusHistogram, status_);
  }

  void set_status(OptimizationHintsReceiveStatus status) { status_ = status; }

 private:
  OptimizationHintsReceiveStatus status_;
};

bool ShouldConsultOptimizationGuide(const GURL& current_main_frame_url,
                                    content::WebContents* web_contents) {
  GURL previous_main_frame_url = web_contents->GetLastCommittedURL();

  // Consult the Optimization Guide on all cross-origin page loads.
  return url::Origin::Create(current_main_frame_url) !=
         url::Origin::Create(previous_main_frame_url);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LcppHintStatus {
  kSucceedToSet = 0,
  kNoLcppData = 1,
  kInvalidLcppStat = 2,
  kConversionFailure = 3,
  kMaxValue = kConversionFailure,
};

// Attach LCP Critical Path Predictor hint to NavigationHandle, so that it
// would be sent to the renderer process upon navigation commit.
void MaybeSetLCPPNavigationHint(content::NavigationHandle& navigation_handle,
                                LoadingPredictor& predictor) {
  base::ElapsedTimer timer;
  if (!blink::LcppEnabled() || !navigation_handle.IsInOutermostMainFrame() ||
      navigation_handle.IsSameDocument()) {
    return;
  }
  const GURL& navigation_url = navigation_handle.GetURL();
  if (!navigation_url.is_valid() || !navigation_url.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  std::optional<LcppStat> lcpp_stat =
      predictor.resource_prefetch_predictor()->GetLcppStat(
          navigation_handle.GetInitiatorOrigin(), navigation_url);
  if (!lcpp_stat) {
    base::UmaHistogramEnumeration(
        "LoadingPredictor.SetLCPPNavigationHint.Status",
        LcppHintStatus::kNoLcppData);
    return;
  }
  if (!IsValidLcppStat(*lcpp_stat)) {
    base::UmaHistogramEnumeration(
        "LoadingPredictor.SetLCPPNavigationHint.Status",
        LcppHintStatus::kInvalidLcppStat);
    return;
  }
  std::optional<blink::mojom::LCPCriticalPathPredictorNavigationTimeHint> hint =
      ConvertLcppStatToLCPCriticalPathPredictorNavigationTimeHint(*lcpp_stat);
  if (hint) {
    navigation_handle.SetLCPPNavigationHint(*hint);
    base::UmaHistogramEnumeration(
        "LoadingPredictor.SetLCPPNavigationHint.Status",
        LcppHintStatus::kSucceedToSet);
    base::UmaHistogramTimes("LoadingPredictor.SetLCPPNavigationHint.Time",
                            timer.Elapsed());
  } else {
    base::UmaHistogramEnumeration(
        "LoadingPredictor.SetLCPPNavigationHint.Status",
        LcppHintStatus::kConversionFailure);
  }
}

void MaybePrewarmMainResourceAndSubresourcesOnNavigation(
    content::NavigationHandle& navigation_handle,
    LoadingPredictor& predictor) {
  static const bool enabled =
      base::FeatureList::IsEnabled(blink::features::kHttpDiskCachePrewarming) &&
      blink::features::kHttpDiskCachePrewarmingTriggerOnNavigation.Get();
  if (!enabled || !navigation_handle.IsInOutermostMainFrame() ||
      navigation_handle.IsSameDocument()) {
    return;
  }
  predictor.MaybePrewarmResources(navigation_handle.GetInitiatorOrigin(),
                                  navigation_handle.GetURL());
}

NavigationId GetNextId() {
  static NavigationId::Generator generator;
  return generator.GenerateNextId();
}

}  // namespace

LoadingPredictorTabHelper::PageData::PageData() : navigation_id_(GetNextId()) {}
LoadingPredictorTabHelper::PageData::~PageData() = default;

LoadingPredictorTabHelper::PageData*
LoadingPredictorTabHelper::PageData::GetForNavigationHandle(
    content::NavigationHandle& navigation_handle) {
  auto* navigation_holder =
      NavigationPageDataHolder::GetForNavigationHandle(navigation_handle);
  if (!navigation_holder)
    return nullptr;

  return navigation_holder->page_data_.get();
}

LoadingPredictorTabHelper::PageData&
LoadingPredictorTabHelper::PageData::CreateForNavigationHandle(
    content::NavigationHandle& navigation_handle) {
  NavigationPageDataHolder* navigation_holder =
      NavigationPageDataHolder::GetOrCreateForNavigationHandle(
          navigation_handle);
  navigation_holder->page_data_->navigation_page_data_holder_ =
      navigation_holder->weak_factory_.GetWeakPtr();
  return *navigation_holder->page_data_;
}

LoadingPredictorTabHelper::PageData*
LoadingPredictorTabHelper::PageData::GetForDocument(
    content::RenderFrameHost& render_frame_host) {
  DocumentPageDataHolder* document_holder =
      DocumentPageDataHolder::GetForCurrentDocument(&render_frame_host);
  if (!document_holder)
    return nullptr;

  return document_holder->page_data_.get();
}

void LoadingPredictorTabHelper::PageData::
    TransferFromNavigationHandleToDocument(
        content::NavigationHandle& navigation_handle,
        content::RenderFrameHost& render_frame_host) {
  auto* navigation_holder =
      NavigationPageDataHolder::GetForNavigationHandle(navigation_handle);
  DCHECK(navigation_holder);

  auto* document_holder =
      DocumentPageDataHolder::GetOrCreateForCurrentDocument(&render_frame_host);
  document_holder->page_data_ = std::move(navigation_holder->page_data_);
  document_holder->page_data_->document_page_data_holder_ =
      document_holder->weak_factory_.GetWeakPtr();

  NavigationPageDataHolder::DeleteForNavigationHandle(navigation_handle);
}

LoadingPredictorTabHelper::DocumentPageDataHolder::DocumentPageDataHolder(
    content::RenderFrameHost* rfh)
    : content::DocumentUserData<DocumentPageDataHolder>(rfh),
      page_data_(base::MakeRefCounted<PageData>()) {}

LoadingPredictorTabHelper::DocumentPageDataHolder::~DocumentPageDataHolder() {
  if (page_data_->predictor_) {
    page_data_->predictor_->loading_data_collector()->RecordPageDestroyed(
        page_data_->navigation_id_,
        page_data_->last_optimization_guide_prediction_);
  }
  page_data_->last_optimization_guide_prediction_ = std::nullopt;
}

LoadingPredictorTabHelper::NavigationPageDataHolder::NavigationPageDataHolder(
    content::NavigationHandle& navigation_handle)
    : page_data_(base::MakeRefCounted<PageData>()),
      navigation_handle_(navigation_handle.GetSafeRef()) {}
LoadingPredictorTabHelper::NavigationPageDataHolder::
    ~NavigationPageDataHolder() = default;

LoadingPredictorTabHelper::LoadingPredictorTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<LoadingPredictorTabHelper>(*web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* predictor = LoadingPredictorFactory::GetForProfile(profile);
  if (predictor)
    predictor_ = predictor->GetWeakPtr();
  if (base::FeatureList::IsEnabled(
          features::kLoadingPredictorUseOptimizationGuide)) {
    optimization_guide_decider_ =
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
    if (optimization_guide_decider_) {
      optimization_guide_decider_->RegisterOptimizationTypes(
          {optimization_guide::proto::LOADING_PREDICTOR});
    }
  }
}

LoadingPredictorTabHelper::~LoadingPredictorTabHelper() = default;

void LoadingPredictorTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!predictor_)
    return;

  MaybeSetLCPPNavigationHint(*navigation_handle, *predictor_);

  MaybePrewarmMainResourceAndSubresourcesOnNavigation(*navigation_handle,
                                                      *predictor_);

  if (!IsHandledNavigation(navigation_handle)) {
    return;
  }

  PageData& page_data = PageData::CreateForNavigationHandle(*navigation_handle);
  page_data.predictor_ = predictor_;

  page_data.has_local_preconnect_predictions_for_current_navigation_ =
      predictor_->OnNavigationStarted(
          page_data.navigation_id_,
          ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                                 ukm::SourceIdType::NAVIGATION_ID),
          navigation_handle->GetInitiatorOrigin(), navigation_handle->GetURL(),
          navigation_handle->NavigationStart());
  if (page_data.has_local_preconnect_predictions_for_current_navigation_ &&
      !features::ShouldAlwaysRetrieveOptimizationGuidePredictions()) {
    return;
  }

  if (!optimization_guide_decider_)
    return;

  if (!ShouldConsultOptimizationGuide(navigation_handle->GetURL(),
                                      web_contents())) {
    return;
  }

  page_data.last_optimization_guide_prediction_ = OptimizationGuidePrediction();
  page_data.last_optimization_guide_prediction_->decision =
      optimization_guide::OptimizationGuideDecision::kUnknown;

  optimization_guide_decider_->CanApplyOptimization(
      navigation_handle->GetURL(), optimization_guide::proto::LOADING_PREDICTOR,
      base::BindOnce(
          &LoadingPredictorTabHelper::OnOptimizationGuideDecision,
          weak_ptr_factory_.GetWeakPtr(), base::WrapRefCounted(&page_data),
          navigation_handle->GetInitiatorOrigin(), navigation_handle->GetURL(),
          !page_data.has_local_preconnect_predictions_for_current_navigation_));
}

void LoadingPredictorTabHelper::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!predictor_)
    return;

  MaybeSetLCPPNavigationHint(*navigation_handle, *predictor_);

  MaybePrewarmMainResourceAndSubresourcesOnNavigation(*navigation_handle,
                                                      *predictor_);

  if (!IsHandledNavigation(navigation_handle)) {
    return;
  }

  auto* page_data = PageData::GetForNavigationHandle(*navigation_handle);
  // PageData may not be created in DidStartNavigation if IsHandledNavigation()
  // changes after the start of the navigation.
  if (!page_data)
    return;

  const auto& redirect_chain = navigation_handle->GetRedirectChain();
  auto redirect_size = redirect_chain.size();
  CHECK_GE(redirect_size, 2U);
  bool is_same_origin_redirect =
      url::Origin::Create(redirect_chain[redirect_size - 2]) ==
      url::Origin::Create(navigation_handle->GetURL());

  // If we are not trying to get an optimization guide prediction for this page
  // load, just return.
  if (!optimization_guide_decider_ ||
      !page_data->last_optimization_guide_prediction_)
    return;

  // Get an updated prediction for the navigation.
  optimization_guide_decider_->CanApplyOptimization(
      navigation_handle->GetURL(), optimization_guide::proto::LOADING_PREDICTOR,
      base::BindOnce(
          &LoadingPredictorTabHelper::OnOptimizationGuideDecision,
          weak_ptr_factory_.GetWeakPtr(), base::WrapRefCounted(page_data),
          navigation_handle->GetInitiatorOrigin(), navigation_handle->GetURL(),
          !(page_data
                ->has_local_preconnect_predictions_for_current_navigation_ &&
            is_same_origin_redirect)));
}

void LoadingPredictorTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!predictor_)
    return;

  if (!IsHandledNavigation(navigation_handle))
    return;

  auto* page_data = PageData::GetForNavigationHandle(*navigation_handle);
  // PageData may not be created in DidStartNavigation if IsHandledNavigation()
  // changes after the start of the navigation.
  if (!page_data)
    return;

  predictor_->OnNavigationFinished(
      page_data->navigation_id_, navigation_handle->GetRedirectChain().front(),
      navigation_handle->GetURL(), navigation_handle->IsErrorPage());

  // Transfer the state from the NavigationHandle to the (committed) main
  // document.
  if (!navigation_handle->HasCommitted())
    return;
  page_data->has_committed_ = true;
  PageData::TransferFromNavigationHandleToDocument(
      *navigation_handle, *navigation_handle->GetRenderFrameHost());
}

void LoadingPredictorTabHelper::ResourceLoadComplete(
    content::RenderFrameHost* render_frame_host,
    const content::GlobalRequestID& request_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!predictor_)
    return;

  bool is_main_frame = render_frame_host->GetParent() == nullptr;
  if (!is_main_frame)
    return;

  auto* page_data = PageData::GetForDocument(*render_frame_host);
  if (!page_data)
    return;

  predictor_->loading_data_collector()->RecordResourceLoadComplete(
      page_data->navigation_id_, resource_load_info);
}

void LoadingPredictorTabHelper::DidLoadResourceFromMemoryCache(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    const std::string& mime_type,
    network::mojom::RequestDestination request_destination) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!predictor_)
    return;

  auto* page_data = PageData::GetForDocument(*render_frame_host);
  if (!page_data)
    return;

  blink::mojom::ResourceLoadInfo resource_load_info;
  resource_load_info.original_url = url;
  resource_load_info.final_url = url;
  resource_load_info.mime_type = mime_type;
  resource_load_info.request_destination = request_destination;
  resource_load_info.method = "GET";
  resource_load_info.request_priority =
      GetRequestPriority(resource_load_info.request_destination);
  resource_load_info.network_info =
      blink::mojom::CommonNetworkInfo::New(false, false, std::nullopt);
  predictor_->loading_data_collector()->RecordResourceLoadComplete(
      page_data->navigation_id_, resource_load_info);
}

void LoadingPredictorTabHelper::DocumentOnLoadCompletedInPrimaryMainFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!predictor_)
    return;

  auto* page_data =
      PageData::GetForDocument(*web_contents()->GetPrimaryMainFrame());
  if (!page_data)
    return;

  predictor_->loading_data_collector()->RecordMainFrameLoadComplete(
      page_data->navigation_id_);
}

void LoadingPredictorTabHelper::OnOptimizationGuideDecision(
    scoped_refptr<PageData> page_data,
    const std::optional<url::Origin>& initiator_origin,
    const GURL& main_frame_url,
    bool should_add_preconnects_to_prediction,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!predictor_)
    return;

  ScopedOptimizationHintsReceiveStatusRecorder recorder;

  if (!page_data->has_committed_) {
    if (!page_data->navigation_page_data_holder_ ||
        page_data->navigation_page_data_holder_->navigation_handle_->GetURL() !=
            main_frame_url) {
      // The current navigation has either redirected or a new one has started,
      // so return.
      recorder.set_status(
          OptimizationHintsReceiveStatus::kAfterRedirectOrNextNavigationStart);
      return;
    }
  }
  if (page_data->has_committed_) {
    // There is not a pending navigation.
    recorder.set_status(OptimizationHintsReceiveStatus::kAfterNavigationFinish);

    // This hint is no longer relevant, so return.
    if (!page_data->document_page_data_holder_)
      return;

    // If we get here, we have not navigated away from the navigation we
    // received hints for. Proceed to get the preconnect prediction so we can
    // see how the predicted resources match what was actually fetched for
    // counterfactual logging purposes.
  } else {
    recorder.set_status(
        OptimizationHintsReceiveStatus::kBeforeNavigationFinish);
  }

  if (!page_data->last_optimization_guide_prediction_) {
    // Data for the navigation has already been recorded, do not proceed any
    // further, even for counterfactual logging.
    return;
  }

  page_data->last_optimization_guide_prediction_->decision = decision;
  page_data->last_optimization_guide_prediction_
      ->optimization_guide_prediction_arrived = base::TimeTicks::Now();

  if (decision != optimization_guide::OptimizationGuideDecision::kTrue)
    return;

  if (!metadata.loading_predictor_metadata()) {
    // Metadata is not applicable, so just log an unknown decision.
    page_data->last_optimization_guide_prediction_->decision =
        optimization_guide::OptimizationGuideDecision::kUnknown;
    return;
  }

  PreconnectPrediction prediction;
  url::Origin main_frame_origin = url::Origin::Create(main_frame_url);
  net::SchemefulSite main_frame_site = net::SchemefulSite(main_frame_url);
  auto network_anonymization_key =
      net::NetworkAnonymizationKey::CreateSameSite(main_frame_site);

  std::set<url::Origin> predicted_origins;
  std::vector<GURL> predicted_subresources;
  const auto lp_metadata = metadata.loading_predictor_metadata();
  for (const auto& subresource : lp_metadata->subresources()) {
    GURL subresource_url(subresource.url());
    if (!subresource_url.is_valid())
      continue;
    predicted_subresources.push_back(subresource_url);
    if (!subresource.preconnect_only() &&
        base::FeatureList::IsEnabled(features::kLoadingPredictorPrefetch)) {
      network::mojom::RequestDestination destination =
          GetDestination(subresource.resource_type());
      if (ShouldPrefetchDestination(destination)) {
        // TODO(falken): Detect duplicates.
        prediction.prefetch_requests.emplace_back(
            subresource_url, network_anonymization_key, destination);
      }
    } else if (should_add_preconnects_to_prediction) {
      url::Origin subresource_origin = url::Origin::Create(subresource_url);
      if (subresource_origin == main_frame_origin) {
        // We are already connecting to the main frame origin by default, so
        // don't include this in the prediction.
        continue;
      }
      if (predicted_origins.find(subresource_origin) != predicted_origins.end())
        continue;
      predicted_origins.insert(subresource_origin);
      prediction.requests.emplace_back(subresource_origin, 1,
                                       network_anonymization_key);
    }
  }

  page_data->last_optimization_guide_prediction_->preconnect_prediction =
      prediction;
  page_data->last_optimization_guide_prediction_->predicted_subresources =
      predicted_subresources;

  // Only prepare page load if the navigation is still pending and we want to
  // use the predictions to pre* subresources.
  if (!page_data->document_page_data_holder_ &&
      features::ShouldUseOptimizationGuidePredictions()) {
    predictor_->PrepareForPageLoad(initiator_origin, main_frame_url,
                                   HintOrigin::OPTIMIZATION_GUIDE,
                                   /*preconnectable=*/false, prediction);
  }
}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(
    LoadingPredictorTabHelper::NavigationPageDataHolder);
DOCUMENT_USER_DATA_KEY_IMPL(LoadingPredictorTabHelper::DocumentPageDataHolder);
WEB_CONTENTS_USER_DATA_KEY_IMPL(LoadingPredictorTabHelper);

}  // namespace predictors
