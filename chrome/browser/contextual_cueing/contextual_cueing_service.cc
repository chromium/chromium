// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include <cmath>

#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_page_data.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_prefs.h"
#include "chrome/browser/contextual_cueing/zero_state_suggestions_page_data.h"
#include "chrome/browser/contextual_cueing/zero_state_suggestions_request.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "net/base/network_anonymization_key.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_pref_names.h"
#endif

namespace contextual_cueing {
namespace {

void LogNudgeInteractionHistogram(NudgeInteraction interaction,
                                  bool is_dynamic) {
  base::UmaHistogramEnumeration("ContextualCueing.NudgeInteraction",
                                interaction);
  std::string cue_type = is_dynamic ? "Dynamic" : "Static";
  base::UmaHistogramEnumeration("ContextualCueing.NudgeInteraction." + cue_type,
                                interaction);
}

void LogNudgeInteractionUKM(ukm::SourceId source_id,
                            NudgeInteraction interaction,
                            base::TimeTicks document_available_time,
                            base::TimeTicks nudge_shown_time) {
  auto* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::ContextualCueing_NudgeInteraction(source_id)
      .SetNudgeInteraction(static_cast<int64_t>(interaction))
      .SetNudgeShownDuration(ukm::GetExponentialBucketMinForUserTiming(
          (base::TimeTicks::Now() - nudge_shown_time).InMilliseconds()))
      .SetNudgeLatencyAfterPageLoad(
          (nudge_shown_time - document_available_time).InMilliseconds())
      .Record(ukm_recorder->Get());
}

#if BUILDFLAG(ENABLE_GLIC)
bool IsGlicTabContextEnabled(PrefService* pref_service) {
  if (base::FeatureList::IsEnabled(features::kGlicDefaultTabContextSetting)) {
    return true;
  }
  return pref_service->GetBoolean(glic::prefs::kGlicTabContextEnabled);
}

void OnSuggestionsReceived(bool is_fre,
                           base::TimeTicks fetch_begin_time,
                           GlicSuggestionsCallback callback,
                           std::vector<std::string> suggestions) {
  base::TimeDelta suggestion_latency =
      base::TimeTicks::Now() - fetch_begin_time;
  std::string result_type =
      suggestions.empty() ? "EmptySuggestions" : "ValidSuggestions";
  std::string engagement_type = is_fre ? "FRE" : "Reengagement";
  // Continue logging the original histogram.
  base::UmaHistogramTimes(
      "ContextualCueing.GlicSuggestions.SuggestionsFetchLatency." + result_type,
      suggestion_latency);
  // Add another split by engagement type.
  base::UmaHistogramTimes(
      "ContextualCueing.GlicSuggestions.SuggestionsFetchLatency." +
          result_type + "." + engagement_type,
      suggestion_latency);

  std::move(callback).Run(suggestions);
}
#endif

base::Value::List ConvertSupportedToolsToPrefValue(
    const std::vector<std::string>& supported_tools) {
  base::Value::List pref_tools;
  for (const auto& tool : supported_tools) {
    pref_tools.Append(tool);
  }
  return pref_tools;
}

std::vector<std::string> GetSupportedToolsFromPref(
    const base::Value::List& pref_value) {
  std::vector<std::string> supported_tools;
  for (const base::Value& value : pref_value) {
    supported_tools.push_back(value.GetString());
  }
  return supported_tools;
}

// Populates the tools to be sent in the request for zero state suggestions.
// Will cache tools from request if present. Otherwise, gets the tools cached
// from pref.
void PopulateSupportedToolsForRequest(
    const std::optional<std::vector<std::string>>& tools_from_request,
    PrefService* pref_service,
    optimization_guide::proto::ZeroStateSuggestionsRequest* out_request) {
  std::vector<std::string> req_supported_tools;
  if (tools_from_request) {
    req_supported_tools = *tools_from_request;
    pref_service->SetList(
        prefs::kZeroStateSuggestionsSupportedTools,
        ConvertSupportedToolsToPrefValue(*tools_from_request));
  } else {
    req_supported_tools = GetSupportedToolsFromPref(
        pref_service->GetList(prefs::kZeroStateSuggestionsSupportedTools));
  }
  *out_request->mutable_supported_tools() = {req_supported_tools.begin(),
                                             req_supported_tools.end()};
}

}  // namespace

ContextualCueingService::ContextualCueingService(
    page_content_annotations::PageContentExtractionService*
        page_content_extraction_service,
    OptimizationGuideKeyedService* optimization_guide_keyed_service,
    predictors::LoadingPredictor* loading_predictor,
    PrefService* pref_service,
    TemplateURLService* template_url_service)
    : recent_nudge_tracker_(kNudgeCapCount.Get(), kNudgeCapTime.Get()),
      recent_visited_origins_(kVisitedDomainsLimit.Get()),
      page_content_extraction_service_(page_content_extraction_service),
      optimization_guide_keyed_service_(optimization_guide_keyed_service),
      loading_predictor_(loading_predictor),
      pref_service_(pref_service),
      template_url_service_(template_url_service),
      mes_url_(optimization_guide::switches::GetModelExecutionServiceURL()) {
  if (optimization_guide_keyed_service_ && IsZeroStateSuggestionsEnabled()) {
    optimization_guide_keyed_service_->RegisterOptimizationTypes(
        {optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS});
  }

  if (kEnablePageContentExtraction.Get() && page_content_extraction_service_) {
    page_content_extraction_service_->AddObserver(this);
  }
}

ContextualCueingService::~ContextualCueingService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (kEnablePageContentExtraction.Get() && page_content_extraction_service_) {
    page_content_extraction_service_->RemoveObserver(this);
  }
}

void ContextualCueingService::ReportPageLoad() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (remaining_quiet_loads_) {
    remaining_quiet_loads_--;
  }
}

void ContextualCueingService::CueingNudgeShown(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  recent_nudge_tracker_.CueingNudgeShown();
  shown_backoff_end_time_ =
      base::TimeTicks::Now() + kMinTimeBetweenNudges.Get();

  if (kMinPageCountBetweenNudges.Get()) {
    // Let the cue logic be performed the next page after quiet count pages.
    remaining_quiet_loads_ = kMinPageCountBetweenNudges.Get() + 1;
  }

  auto origin = url::Origin::Create(url);
  auto iter = recent_visited_origins_.Get(origin);
  if (iter == recent_visited_origins_.end()) {
    iter = recent_visited_origins_.Put(
        origin, NudgeCapTracker(kNudgeCapCountPerDomain.Get(),
                                kNudgeCapTimePerDomain.Get()));
  }
  iter->second.CueingNudgeShown();
}

void ContextualCueingService::CueingNudgeDismissed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::TimeDelta backoff_duration =
      kBackoffTime.Get() * pow(kBackoffMultiplierBase.Get(), dismiss_count_);

  dismiss_backoff_end_time_ = base::TimeTicks::Now() + backoff_duration;
  ++dismiss_count_;
}

void ContextualCueingService::CueingNudgeClicked() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dismiss_count_ = 0;
}

NudgeDecision ContextualCueingService::CanShowNudge(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (remaining_quiet_loads_ > 0) {
    return NudgeDecision::kNotEnoughPageLoadsSinceLastNudge;
  }
  if (shown_backoff_end_time_ &&
      base::TimeTicks::Now() < shown_backoff_end_time_) {
    return NudgeDecision::kNotEnoughTimeSinceLastNudgeShown;
  }
  if (IsNudgeBlockedByBackoffRule()) {
    return NudgeDecision::kNotEnoughTimeSinceLastNudgeDismissed;
  }
  if (!recent_nudge_tracker_.CanShowNudge()) {
    return NudgeDecision::kTooManyNudgesShownToTheUser;
  }
  auto iter = recent_visited_origins_.Peek(url::Origin::Create(url));
  if (iter != recent_visited_origins_.end() && !iter->second.CanShowNudge()) {
    return NudgeDecision::kTooManyNudgesShownToTheUserForDomain;
  }
  return NudgeDecision::kSuccess;
}

bool ContextualCueingService::IsNudgeBlockedByBackoffRule() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return dismiss_backoff_end_time_ &&
         (base::TimeTicks::Now() < dismiss_backoff_end_time_);
}

bool ContextualCueingService::IsPageTypeEligibleForContextualSuggestions(
    GURL url) const {
  // Non-HTTP/HTTPS pages are not eligible.
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  // Search results pages are not eligible.
  if (!kAllowContextualSuggestionsForSearchResultsPages.Get() &&
      (template_url_service_ &&
       template_url_service_->ExtractSearchMetadata(url))) {
    return false;
  }

  return true;
}

void ContextualCueingService::OnNudgeActivity(
    content::WebContents* web_contents,
    base::TimeTicks document_available_time,
    bool is_dynamic,
    tabs::GlicNudgeActivity activity) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::optional<base::TimeTicks> nudge_time =
      recent_nudge_tracker_.GetMostRecentNudgeTime();
  const GURL& url = web_contents->GetLastCommittedURL();
  NudgeInteraction interaction;
  bool log_ukm = false;
  switch (activity) {
    case tabs::GlicNudgeActivity::kNudgeShown:
      interaction = NudgeInteraction::kShown;
      CueingNudgeShown(url);
      break;
    case tabs::GlicNudgeActivity::kNudgeClicked:
      CueingNudgeClicked();
      interaction = NudgeInteraction::kClicked;
      log_ukm = true;
      break;
    case tabs::GlicNudgeActivity::kNudgeDismissed:
      interaction = NudgeInteraction::kDismissed;
      CueingNudgeDismissed();
      log_ukm = true;
      break;
    case tabs::GlicNudgeActivity::kNudgeNotShownWebContents:
      interaction = NudgeInteraction::kNudgeNotShownWebContents;
      break;
    case tabs::GlicNudgeActivity::kNudgeNotShownWindowCallToActionUI:
      interaction = NudgeInteraction::kNudgeNotShownWindowCallToActionUI;
      break;
    case tabs::GlicNudgeActivity::kNudgeIgnoredActiveTabChanged:
      interaction = NudgeInteraction::kIgnoredTabChange;
      // The ActiveTabChanged activity is called very aggresivly and there may
      // not be an actively shown nudge. We should only log this as an action if
      // there is a shown nudge is dismissed
      if (!nudge_time) {
        return;
      }
      log_ukm = true;
      break;
    case tabs::GlicNudgeActivity::kNudgeIgnoredNavigation:
      interaction = NudgeInteraction::kIgnoredNavigation;
      log_ukm = true;
      break;
  }
  LogNudgeInteractionHistogram(interaction, is_dynamic);
  // As this function is called multiple times per nudge only some of the
  // activities result in a UKM call.
  if (log_ukm) {
    CHECK(nudge_time);
    LogNudgeInteractionUKM(
        web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId(), interaction,
        document_available_time, *nudge_time);
  }
}

void ContextualCueingService::PrepareToFetchContextualGlicZeroStateSuggestions(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsZeroStateSuggestionsEnabled()) {
    return;
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (!IsPageTypeEligibleForContextualSuggestions(
          web_contents->GetLastCommittedURL())) {
    return;
  }

  if (!IsGlicTabContextEnabled(pref_service_)) {
    return;
  }

  // This call preflights grabbing the page content.
  ZeroStateSuggestionsPageData::CreateForPage(web_contents->GetPrimaryPage());

  if (loading_predictor_) {
    net::NetworkAnonymizationKey anonymization_key =
        net::NetworkAnonymizationKey::CreateSameSite(
            net::SchemefulSite(mes_url_));
    loading_predictor_->PreconnectURLIfAllowed(
        mes_url_, /*allow_credentials=*/true, anonymization_key);
  }
#endif
}

std::unique_ptr<ZeroStateSuggestionsRequest>
ContextualCueingService::MakeZeroStateSuggestionsRequest(
    const std::vector<content::WebContents*>& web_contents_list,
    bool is_fre,
    std::optional<std::vector<std::string>> supported_tools,
    const content::WebContents* focused_tab) {
  // Construct base request proto.
  optimization_guide::proto::ZeroStateSuggestionsRequest request_proto;
  request_proto.set_is_fre(is_fre);
  if (g_browser_process) {
    request_proto.set_locale(g_browser_process->GetApplicationLocale());
  }
  PopulateSupportedToolsForRequest(supported_tools, pref_service_,
                                   &request_proto);
  // Instantiate the one-of to indicate the request type.
  if (focused_tab && web_contents_list.size() == 1) {
    request_proto.mutable_page_context();
  } else {
    request_proto.mutable_page_context_list();
  }

  return std::make_unique<ZeroStateSuggestionsRequest>(
      optimization_guide_keyed_service_, request_proto, web_contents_list,
      focused_tab);
}

void ContextualCueingService::
    GetContextualGlicZeroStateSuggestionsForFocusedTab(
        content::WebContents* web_contents,
        bool is_fre,
        std::optional<std::vector<std::string>> supported_tools,
        GlicSuggestionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsZeroStateSuggestionsEnabled()) {
    std::move(callback).Run({});
    return;
  }

  bool page_type_eligible = IsPageTypeEligibleForContextualSuggestions(
      web_contents->GetLastCommittedURL());
  base::UmaHistogramBoolean(
      "ContextualCueing.GlicSuggestions.FocusedTabEligibleForSuggestions",
      page_type_eligible);
  if (!page_type_eligible) {
    std::move(callback).Run({});
    return;
  }

#if BUILDFLAG(ENABLE_GLIC)
  if (!IsGlicTabContextEnabled(pref_service_)) {
    std::move(callback).Run({});
    return;
  }

  // Add callback to new request or existing one if already have one for
  // the page associated with `web_contents`.
  auto* zss_data = ZeroStateSuggestionsPageData::GetOrCreateForPage(
      web_contents->GetPrimaryPage());
  auto* zss_request_ptr = zss_data->focused_tab_request();
  if (!zss_request_ptr) {
    auto zss_request = MakeZeroStateSuggestionsRequest(
        {web_contents}, is_fre, supported_tools, web_contents);
    zss_request_ptr = zss_request.get();
    zss_data->set_focused_tab_request(std::move(zss_request));
  }
  zss_request_ptr->AddCallback(base::BindOnce(&OnSuggestionsReceived, is_fre,
                                              base::TimeTicks::Now(),
                                              std::move(callback)));
#else
  std::move(callback).Run({});
#endif
}

std::optional<std::vector<content::WebContents*>>
ContextualCueingService::GetOutstandingPinnedTabsContents() {
  if (!pinned_tabs_zero_state_suggestions_request_) {
    return std::nullopt;
  }
  return pinned_tabs_zero_state_suggestions_request_->GetRequestedTabs();
}

bool ContextualCueingService::
    GetContextualGlicZeroStateSuggestionsForPinnedTabs(
        std::vector<content::WebContents*> pinned_web_contents,
        bool is_fre,
        std::optional<std::vector<std::string>> supported_tools,
        const content::WebContents* focused_tab,
        GlicSuggestionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsZeroStateSuggestionsEnabled()) {
    std::move(callback).Run({});
    return false;
  }

  // Remove all ineligible pages from list.
  std::erase_if(pinned_web_contents, [&](const auto* web_contents) {
    return !IsPageTypeEligibleForContextualSuggestions(
        web_contents->GetLastCommittedURL());
  });
  base::UmaHistogramBoolean(
      "ContextualCueing.GlicSuggestions.PinnedTabsEligibleForSuggestions",
      !pinned_web_contents.empty());
  if (pinned_web_contents.empty()) {
    std::move(callback).Run({});
    return false;
  }

#if BUILDFLAG(ENABLE_GLIC)
  // Initiate request for suggestions for pinned tabs.
  pinned_tabs_zero_state_suggestions_request_ = MakeZeroStateSuggestionsRequest(
      pinned_web_contents, is_fre, supported_tools, focused_tab);
  pinned_tabs_zero_state_suggestions_request_->AddCallback(base::BindOnce(
      &ContextualCueingService::OnPinnedTabsSuggestionsReceived,
      weak_ptr_factory_.GetWeakPtr(), is_fre, base::TimeTicks::Now(),
      pinned_tabs_zero_state_suggestions_request_->AsWeakPtr(),
      std::move(callback)));
  return true;
#else
  std::move(callback).Run({});
  return false;
#endif
}

void ContextualCueingService::OnPinnedTabsSuggestionsReceived(
    bool is_fre,
    base::TimeTicks fetch_begin_time,
    base::WeakPtr<ZeroStateSuggestionsRequest> pinned_tabs_request,
    GlicSuggestionsCallback callback,
    std::vector<std::string> suggestions) {
#if BUILDFLAG(ENABLE_GLIC)
  OnSuggestionsReceived(is_fre, fetch_begin_time, std::move(callback),
                        std::move(suggestions));

  // Only destroy the outstanding pinned tabs request if it is the same.
  if (pinned_tabs_request &&
      pinned_tabs_request.get() ==
          pinned_tabs_zero_state_suggestions_request_.get()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostNonNestableTask(
        FROM_HERE,
        base::BindOnce(&ZeroStateSuggestionsRequest::Destroy,
                       std::move(pinned_tabs_zero_state_suggestions_request_)));
  }
#endif
}

void ContextualCueingService::OnPageContentExtracted(
    content::Page& page,
    const optimization_guide::proto::AnnotatedPageContent& page_content) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* cueing_page_data = ContextualCueingPageData::GetForPage(page);
  if (!cueing_page_data) {
    return;
  }
  cueing_page_data->OnPageContentExtracted(page_content);
}

}  // namespace contextual_cueing
