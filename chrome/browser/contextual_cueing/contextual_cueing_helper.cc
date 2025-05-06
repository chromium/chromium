// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_helper.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_page_data.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/contextual_cueing/zero_state_suggestions_page_data.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "components/optimization_guide/core/hints_processing_util.h"
#include "components/optimization_guide/core/model_execution/model_execution_features_controller.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/contextual_cueing_metadata.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#endif

namespace contextual_cueing {

class ScopedNudgeDecisionRecorder {
 public:
  ScopedNudgeDecisionRecorder(
      optimization_guide::proto::OptimizationType optimization_type,
      ukm::SourceId source_id)
      : optimization_type_(optimization_type), source_id_(source_id) {}
  ~ScopedNudgeDecisionRecorder() {
    base::UmaHistogramEnumeration(
        "ContextualCueing.NudgeDecision." +
            optimization_guide::GetStringNameForOptimizationType(
                optimization_type_),
        nudge_decision_);

    auto* ukm_recorder = ukm::UkmRecorder::Get();
    ukm::builders::ContextualCueing_NudgeDecision(source_id_)
        .SetNudgeDecision(static_cast<int64_t>(nudge_decision_))
        .SetOptimizationType(optimization_type_)
        .Record(ukm_recorder->Get());
  }

  void set_nudge_decision(NudgeDecision nudge_decision) {
    nudge_decision_ = nudge_decision;
  }

  NudgeDecision nudge_decision() const { return nudge_decision_; }

 private:
  optimization_guide::proto::OptimizationType optimization_type_;
  ukm::SourceId source_id_;
  NudgeDecision nudge_decision_ = NudgeDecision::kUnknown;
};

ContextualCueingHelper::ContextualCueingHelper(
    content::WebContents* web_contents,
    OptimizationGuideKeyedService* ogks,
    ContextualCueingService* ccs)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<ContextualCueingHelper>(*web_contents),
      optimization_guide_keyed_service_(ogks),
      contextual_cueing_service_(ccs) {
  if (base::FeatureList::IsEnabled(kContextualCueing)) {
    // LINT.IfChange(OptType)
    optimization_guide_keyed_service_->RegisterOptimizationTypes(
        {optimization_guide::proto::GLIC_CONTEXTUAL_CUEING});
    // LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_cueing/histograms.xml:OptType)
  }
}

ContextualCueingHelper::~ContextualCueingHelper() = default;

tabs::GlicNudgeController* ContextualCueingHelper::GetGlicNudgeController() {
  if (!base::FeatureList::IsEnabled(kContextualCueing)) {
    return nullptr;
  }

  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  if (!browser) {
    return nullptr;
  }
  return browser->browser_window_features()->glic_nudge_controller();
}

void ContextualCueingHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Ignore sub-frame and uncommitted navigations.
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  if (!navigation_handle->HasCommitted()) {
    return;
  }

  last_same_doc_navigation_committed_ =
      navigation_handle->IsSameDocument()
          ? std::make_optional(base::TimeTicks::Now())
          : std::nullopt;

  // Ignore reloads.
  if (PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                               ui::PAGE_TRANSITION_RELOAD)) {
    return;
  }

  // Ignore fragment changes.
  if (navigation_handle->GetPreviousPrimaryMainFrameURL().GetWithoutRef() ==
      navigation_handle->GetURL().GetWithoutRef()) {
    return;
  }

  // Reset FCP state.
  has_first_contentful_paint_ = false;

  // Clear zero state suggestions if needed.
  if (base::FeatureList::IsEnabled(kGlicZeroStateSuggestions) &&
      ZeroStateSuggestionsPageData::GetForPage(
          web_contents()->GetPrimaryPage())) {
    ZeroStateSuggestionsPageData::DeleteForPage(
        web_contents()->GetPrimaryPage());
  }

  if (!base::FeatureList::IsEnabled(kContextualCueing)) {
    return;
  }

  // Make sure we always clear the nudge label anyway despite operating on
  // pages.
  auto* glic_nudge_controller = GetGlicNudgeController();
  if (glic_nudge_controller) {
    glic_nudge_controller->UpdateNudgeLabel(
        web_contents(), std::string(),
        tabs::GlicNudgeActivity::kNudgeIgnoredNavigation, base::DoNothing());
  }

  // Do not report page loads for these types of navigations.
  if (navigation_handle->IsErrorPage() ||
      !navigation_handle->ShouldUpdateHistory()) {
    return;
  }

  // We have already initiated nudging sequence for the page. Do not report page
  // load.
  if (ContextualCueingPageData::GetForPage(web_contents()->GetPrimaryPage())) {
    return;
  }

  contextual_cueing_service_->ReportPageLoad();
}

void ContextualCueingHelper::PrimaryMainDocumentElementAvailable() {
  if (!base::FeatureList::IsEnabled(kContextualCueing)) {
    return;
  }

  // We have already initiated nudging sequence for the page. Do not see if we
  // should nudge.
  if (ContextualCueingPageData::GetForPage(web_contents()->GetPrimaryPage())) {
    return;
  }

  auto* glic_nudge_controller = GetGlicNudgeController();
  if (!glic_nudge_controller ||
      !web_contents()->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    return;
  }
  // Determine if server data indicates a nudge should be shown.
  optimization_guide_keyed_service_->CanApplyOptimization(
      web_contents()->GetLastCommittedURL(),
      optimization_guide::proto::GLIC_CONTEXTUAL_CUEING,
      base::BindOnce(&ContextualCueingHelper::OnOptimizationGuideCueingMetadata,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void ContextualCueingHelper::OnFirstContentfulPaintInPrimaryMainFrame() {
  if (!base::FeatureList::IsEnabled(kGlicZeroStateSuggestions)) {
    return;
  }

  has_first_contentful_paint_ = true;

  ZeroStateSuggestionsPageData* page_data =
      ZeroStateSuggestionsPageData::GetForPage(
          web_contents()->GetPrimaryPage());
  if (page_data) {
    page_data->InitiatePageContentExtraction();
  }
}

void ContextualCueingHelper::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (!base::FeatureList::IsEnabled(kGlicZeroStateSuggestions)) {
    return;
  }

  ZeroStateSuggestionsPageData* page_data =
      ZeroStateSuggestionsPageData::GetForPage(
          web_contents()->GetPrimaryPage());
  if (page_data) {
    page_data->InitiatePageContentExtraction();
  }
}

void ContextualCueingHelper::OnOptimizationGuideCueingMetadata(
    base::TimeTicks document_available_time,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  std::unique_ptr<ScopedNudgeDecisionRecorder> recorder =
      std::make_unique<ScopedNudgeDecisionRecorder>(
          optimization_guide::proto::GLIC_CONTEXTUAL_CUEING,
          web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId());

  if (decision != optimization_guide::OptimizationGuideDecision::kTrue ||
      metadata.empty()) {
    recorder->set_nudge_decision(NudgeDecision::kServerDataUnavailable);
    return;
  }
  auto parsed = metadata.ParsedMetadata<
      optimization_guide::proto::GlicContextualCueingMetadata>();
  if (!parsed) {
    recorder->set_nudge_decision(NudgeDecision::kServerDataMalformed);
    return;
  }

  ContextualCueingPageData::CreateForPage(
      web_contents()->GetPrimaryPage(), std::move(*parsed),
      base::BindOnce(&ContextualCueingHelper::OnCueingDecision,
                     weak_ptr_factory_.GetWeakPtr(), std::move(recorder),
                     document_available_time));
}

bool ContextualCueingHelper::IsBrowserBlockingNudges(
    ScopedNudgeDecisionRecorder* recorder) {
  // Determine if the Browser is available for nudging.
  if (!web_contents()) {
    return false;
  }

  auto* tab_interface = tabs::TabInterface::GetFromContents(web_contents());
  if (!tab_interface) {
    return false;
  }

  auto* browser_window_interface = tab_interface->GetBrowserWindowInterface();
  if (!browser_window_interface) {
    return false;
  }

  auto* user_education_interface =
      browser_window_interface->GetUserEducationInterface();
  if (!user_education_interface) {
    return false;
  }

  if (user_education_interface->IsFeaturePromoActive(
          feature_engagement::kIPHGlicPromoFeature)) {
    recorder->set_nudge_decision(NudgeDecision::kNudgeNotShownIPH);
    return true;
  }

#if BUILDFLAG(ENABLE_GLIC)
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());

  if (!glic::GlicEnabling::IsEnabledForProfile(profile)) {
    return true;
  }

  auto* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile);

  if (glic_service->IsWindowShowing()) {
    recorder->set_nudge_decision(NudgeDecision::kNudgeNotShownWindowShowing);
    return true;
  }
#endif  // BUILDFLAG(ENABLE_GLIC)

  return false;
}

void ContextualCueingHelper::OnCueingDecision(
    std::unique_ptr<ScopedNudgeDecisionRecorder> decision_recorder,
    base::TimeTicks document_available_time,
    base::expected<std::string, NudgeDecision> decision_result) {
  CHECK_EQ(NudgeDecision::kUnknown, decision_recorder->nudge_decision());
  if (ContextualCueingPageData::GetForPage(web_contents()->GetPrimaryPage())) {
    ContextualCueingPageData::DeleteForPage(web_contents()->GetPrimaryPage());
  }

  if (!decision_result.has_value()) {
    decision_recorder->set_nudge_decision(decision_result.error());
    return;
  }

  std::string cue_label = decision_result.value();
  if (IsBrowserBlockingNudges(decision_recorder.get())) {
    return;
  }

  const GURL& url = web_contents()->GetLastCommittedURL();
  auto can_show_decision = contextual_cueing_service_->CanShowNudge(url);
  decision_recorder->set_nudge_decision(can_show_decision);
  if (can_show_decision != NudgeDecision::kSuccess) {
    return;
  }

  GetGlicNudgeController()->UpdateNudgeLabel(
      web_contents(), cue_label, /*activity=*/std::nullopt,
      base::BindRepeating(&ContextualCueingService::OnNudgeActivity,
                          contextual_cueing_service_->GetWeakPtr(),
                          web_contents(), document_available_time));
}

// static
void ContextualCueingHelper::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(contextual_cueing::kContextualCueing) &&
      !base::FeatureList::IsEnabled(
          contextual_cueing::kGlicZeroStateSuggestions)) {
    return;
  }

#if BUILDFLAG(ENABLE_GLIC)
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!glic::GlicEnabling::IsProfileEligible(profile)) {
    return;
  }

  auto* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide_keyed_service) {
    return;
  }

  auto* contextual_cueing_service =
      ContextualCueingServiceFactory::GetForProfile(profile);
  if (!contextual_cueing_service) {
    return;
  }

  ContextualCueingHelper::CreateForWebContents(web_contents,
                                               optimization_guide_keyed_service,
                                               contextual_cueing_service);
#endif  // BUILDFLAG(ENABLE_GLIC)
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualCueingHelper);

}  // namespace contextual_cueing
