// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notimplemented.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_enums.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_menu_model.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_metrics.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/contextual_cueing/cueing_log.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_provider.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/google/core/common/google_util.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/optimization_guide_common.mojom.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/actions/actions.h"
#include "ui/menus/simple_menu_model.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_observer.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#endif

namespace contextual_cueing {

namespace {

const char kHomepagePathRegex[] =
    "(?i)(/(en\\/)?((index|default|home|homepage|main|welcome)(\\.[^/"
    "?;]+)?)?)?";

void RecordContextualCueingDecision(
    ContextualCueingDecision contextual_cueing_decision) {
  base::UmaHistogramEnumeration("ContextualCueing.V2.Decision",
                                contextual_cueing_decision);
}

std::optional<CueTargetType> GetTargetType(
    optimization_guide::proto::ContextualCueingResponse::FulfillmentSurfaceCase
        fulfillment_surface_case) {
  using enum optimization_guide::proto::ContextualCueingResponse::
      FulfillmentSurfaceCase;
  switch (fulfillment_surface_case) {
    case kGeminiInChromeSurface:
      return CueTargetType::kGlic;
    default:
      return std::nullopt;
  }
}

optimization_guide::proto::Tab GetTabProtoFromWebContents(
    content::WebContents* web_contents) {
  optimization_guide::proto::Tab tab;
  tab.set_url(web_contents->GetLastCommittedURL().spec());
  tab.set_title(base::UTF16ToUTF8(web_contents->GetTitle()));
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
  if (tab_id.is_valid()) {
    tab.set_tab_id(tab_id.id());
  }
  return tab;
}

bool AreTabsEqual(optimization_guide::proto::Tab tab1,
                  optimization_guide::proto::Tab tab2) {
  return tab1.tab_id() == tab2.tab_id() && tab1.url() == tab2.url() &&
         tab1.title() == tab2.title();
}

#if !BUILDFLAG(IS_ANDROID)
class ContextualCueingPageActionObserver
    : public page_actions::PageActionObserver {
 public:
  explicit ContextualCueingPageActionObserver(
      base::RepeatingClosure hidden_callback)
      : page_actions::PageActionObserver(kActionAnchoredContextualCue),
        hidden_callback_(hidden_callback) {}

  void OnPageActionIconHidden(
      const page_actions::PageActionState& page_action) override {
    hidden_callback_.Run();
  }

 private:
  base::RepeatingClosure hidden_callback_;
};
#endif

}  // namespace

ContextualCueingController::ContextualCueingController(
    BrowserWindowInterface* browser_window_interface,
    TabListInterface* tab_list_interface)
    : browser_window_interface_(browser_window_interface),
      tab_list_interface_(tab_list_interface),
      contextual_cueing_service_(ContextualCueingServiceFactory::GetForProfile(
          browser_window_interface_->GetProfile())),
      page_content_annotations_service_(
          PageContentAnnotationsServiceFactory::GetForProfile(
              browser_window_interface_->GetProfile())),
      optimization_guide_keyed_service_(
          OptimizationGuideKeyedServiceFactory::GetForProfile(
              browser_window_interface_->GetProfile())),
      sync_service_(SyncServiceFactory::GetForProfile(
          browser_window_interface_->GetProfile())),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(
          browser_window_interface_->GetProfile())),
      identity_manager_(IdentityManagerFactory::GetForProfile(
          browser_window_interface_->GetProfile())) {
#if !BUILDFLAG(IS_ANDROID)
  page_action_observer_ = std::make_unique<ContextualCueingPageActionObserver>(
      base::BindRepeating(&ContextualCueingController::OnCueHidden,
                          weak_ptr_factory_.GetWeakPtr()));
#endif
  if (page_content_annotations_service_) {
    page_content_annotations_service_->AddObserver(
        page_content_annotations::AnnotationType::kCategoryClassifier, this);
  }
}

ContextualCueingController::~ContextualCueingController() {
  if (page_content_annotations_service_) {
    page_content_annotations_service_->RemoveObserver(
        page_content_annotations::AnnotationType::kCategoryClassifier, this);
  }
}

// static
ContextualCueingController* ContextualCueingController::GetForWebContents(
    content::WebContents& contents) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
#else
  if (auto* tab = tabs::TabInterface::GetFromContents(&contents)) {
    if (auto* browser_window_interface = tab->GetBrowserWindowInterface()) {
      return browser_window_interface->GetFeatures()
          .contextual_cueing_controller();
    }
  }
#endif
  return nullptr;
}

void ContextualCueingController::RegisterCueTarget(
    CueTargetType type,
    std::unique_ptr<CueTarget> target) {
  cue_targets_.insert_or_assign(type, std::move(target));
}

void ContextualCueingController::OnPageContentAnnotated(
    const page_content_annotations::HistoryVisit& visit,
    const page_content_annotations::PageContentAnnotationsResult& result) {
  content::WebContents* active_web_contents =
      tab_list_interface_->GetActiveTab()
          ? tab_list_interface_->GetActiveTab()->GetContents()
          : nullptr;
  if (!active_web_contents ||
      visit.url != active_web_contents->GetLastCommittedURL()) {
    CUEING_LOG(
        base::StringPrintf("%s ineligible for cue: No longer active tab after "
                           "category classification.",
                           active_web_contents->GetLastCommittedURL().spec()));
    RecordContextualCueingDecision(
        ContextualCueingDecision::
            kNoLongerActiveTabAfterCategoryClassification);
    return;
  }

  if (!IsUrlEligibleForCue(active_web_contents->GetLastCommittedURL())) {
    CUEING_LOG(
        base::StringPrintf("%s ineligible for cue: URL is ineligible.",
                           active_web_contents->GetLastCommittedURL().spec()));
    RecordContextualCueingDecision(ContextualCueingDecision::kUrlNotEligible);
    return;
  }

  if (GetEligibleCueSurfaces().empty()) {
    CUEING_LOG(
        base::StringPrintf("%s ineligible for cue: No eligible cue surfaces.",
                           active_web_contents->GetLastCommittedURL().spec()));
    RecordContextualCueingDecision(
        ContextualCueingDecision::kNoEligibleCueSurfaces);
    return;
  }

  if (!IsAllowedToShowCue()) {
    return;
  }

  // Check classification to see if we should proceed to next step.
  bool is_supported_category = false;
  for (const page_content_annotations::Category& category :
       result.GetCategoryResults()) {
    if (category.category_type ==
            page_content_annotations::CategoryType::kEducation &&
        category.score > kEduClassifierThreshold.Get()) {
      is_supported_category = true;
      break;
    }
    if (category.category_type ==
            page_content_annotations::CategoryType::kShopping &&
        category.score > kShoppingClassifierThreshold.Get()) {
      is_supported_category = true;
      break;
    }
  }

  if (!is_supported_category) {
    CUEING_LOG(base::StringPrintf(
        "%s ineligible for cue: Failed category classification.",
        active_web_contents->GetLastCommittedURL().spec()));
    RecordContextualCueingDecision(
        ContextualCueingDecision::kFailedCategoryClassification);
    return;
  }

  if (auto decision = contextual_cueing_service_->CanShowCue(
          active_web_contents->GetLastCommittedURL());
      decision != ContextualCueingDecision::kSuccess) {
    CUEING_LOG(
        base::StringPrintf("%s ineligible for cue with reason: %d.",
                           active_web_contents->GetLastCommittedURL().spec(),
                           static_cast<int>(decision)));
    RecordContextualCueingDecision(decision);
    return;
  }

  CUEING_LOG(
      base::StringPrintf("%s eligible for cue: Category classification "
                         "succeeded. Initiating model execution request.",
                         active_web_contents->GetLastCommittedURL().spec()));
  InitiateModelExecutionRequest();
}

void ContextualCueingController::InitiateModelExecutionRequest() {
  if (!optimization_guide_keyed_service_) {
    RecordContextualCueingDecision(
        ContextualCueingDecision::kModelExecutionUnavailable);
    return;
  }

  optimization_guide::proto::ContextualCueingRequest request;
  content::WebContents* active_web_contents =
      tab_list_interface_->GetActiveTab()
          ? tab_list_interface_->GetActiveTab()->GetContents()
          : nullptr;
  CHECK(active_web_contents);
  request.mutable_active_tab_page_context()->set_url(
      active_web_contents->GetLastCommittedURL().spec());
  request.mutable_active_tab_page_context()->set_title(
      base::UTF16ToUTF8(active_web_contents->GetTitle()));

  struct BackgroundTabInfo {
    base::Time last_active_time;
    raw_ptr<content::WebContents> contents;
  };
  std::vector<BackgroundTabInfo> background_tabs;
  for (int i = 0; i < tab_list_interface_->GetTabCount(); ++i) {
    tabs::TabInterface* tab = tab_list_interface_->GetTab(i);
    if (tab == tab_list_interface_->GetActiveTab()) {
      // Active tab already added to the request.
      continue;
    }
    content::WebContents* tab_contents = tab ? tab->GetContents() : nullptr;
    if (!tab_contents) {
      continue;
    }
    if (!IsUrlEligibleForCue(tab_contents->GetLastCommittedURL())) {
      continue;
    }
    background_tabs.push_back(
        {.last_active_time = tab_contents->GetLastActiveTime(),
         .contents = tab_contents});
  }

  std::sort(background_tabs.begin(), background_tabs.end(),
            [](const BackgroundTabInfo& a, const BackgroundTabInfo& b) {
              return a.last_active_time > b.last_active_time;
            });

  for (size_t i = 0; i < std::min<size_t>(background_tabs.size(),
                                          kMaxNumBackgroundTabs.Get());
       ++i) {
    *request.add_background_tabs() =
        GetTabProtoFromWebContents(background_tabs[i].contents);
  }
  CUEING_LOG(base::StringPrintf("Requesting %d background tabs.",
                                request.background_tabs_size()));

  auto eligible_cue_surfaces = GetEligibleCueSurfaces();
  *request.mutable_supported_surfaces() = {eligible_cue_surfaces.begin(),
                                           eligible_cue_surfaces.end()};

  LOCAL_HISTOGRAM_COUNTS_100("ContextualCueing.V2.NumRequestedBackgroundTabs",
                             request.background_tabs_size());
  optimization_guide_keyed_service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kContextualCueing, request,
      {.service_type =
           kUsePrivateAi.Get()
               ? optimization_guide::ModelExecutionServiceType::kPrivateAi
               : optimization_guide::ModelExecutionServiceType::kDefault},
      base::BindOnce(
          &ContextualCueingController::OnModelExecutionResponseReceived,
          weak_ptr_factory_.GetWeakPtr(),
          GetTabProtoFromWebContents(active_web_contents)));
}

void ContextualCueingController::OnModelExecutionResponseReceived(
    optimization_guide::proto::Tab active_tab,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  tabs::TabInterface* current_active_tab = tab_list_interface_->GetActiveTab();
  if (!current_active_tab || !current_active_tab->GetContents() ||
      !AreTabsEqual(active_tab, GetTabProtoFromWebContents(
                                    current_active_tab->GetContents()))) {
    CUEING_LOG(
        "Model execution returned but tab for generated cue is no longer "
        "active.");
    RecordContextualCueingDecision(
        ContextualCueingDecision::kNoLongerActiveTabAfterModelExecution);
    return;
  }

  if (!result.response.has_value()) {
    CUEING_LOG("Model execution to generate cue failed.");
    RecordContextualCueingDecision(
        ContextualCueingDecision::kModelExecutionFailed);
    return;
  }

  std::optional<optimization_guide::proto::ContextualCueingResponse> response =
      optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::ContextualCueingResponse>(
          *result.response);
  if (!response) {
    CUEING_LOG("Model execution to generate cue failed: couldn't parse proto.");
    RecordContextualCueingDecision(
        ContextualCueingDecision::kModelExecutionResponseFailedToParse);
    return;
  }

  if (!response->has_anchored_message_cue() ||
      response->anchored_message_cue().anchored_message_text().empty() ||
      response->anchored_message_cue().action_text().empty()) {
    CUEING_LOG(
        "Model execution to generate cue failed: missing anchored message "
        "text.");
    RecordContextualCueingDecision(
        ContextualCueingDecision::kMissingAnchoredMessageText);
    return;
  }

  std::optional<CueTargetType> target_type =
      GetTargetType(response->fulfillment_surface_case());
  if (!target_type) {
    CUEING_LOG("Unknown fulfillment surface");
    RecordContextualCueingDecision(
        ContextualCueingDecision::kUnknownFulfillmentSurface);
    return;
  }

  CueTarget* target = GetTarget(*target_type);
  if (!target) {
    CUEING_LOG(base::StringPrintf("No CueTarget registered for '%s'",
                                  GetName(*target_type)));
    RecordContextualCueingDecision(
        ContextualCueingDecision::kTargetFeatureNotRegistered);
    return;
  }

  if (IsUserSubjectToAgeRestrictions()) {
    RecordContextualCueingDecision(
        ContextualCueingDecision::kAgeRestrictionEnforced);
    return;
  }

  if (!target->IsEligible()) {
    CUEING_LOG(base::StringPrintf("Not eligible for '%s' cues",
                                  GetName(*target_type)));
    RecordContextualCueingDecision(
        ContextualCueingDecision::kTargetFeatureNotEligible);
    return;
  }

  if (IsAllowedToShowCue()) {
    ShowCue(*target_type, *target, std::move(*response));
  }
}

bool ContextualCueingController::IsUserSubjectToAgeRestrictions() {
  if (!base::FeatureList::IsEnabled(kContextualCueingV2EnforceAgeRestriction)) {
    return false;
  }

  // If the user is not signed in, we cannot check their age. Say that they are
  // subject to age restrictions.
  if (!identity_manager_) {
    return true;
  }

  // If the user is signed in, check if they are subject to age restrictions.
  AccountCapabilities capabilities =
      identity_manager_
          ->FindExtendedAccountInfo(identity_manager_->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin))
          .capabilities;

  return capabilities.can_use_model_execution_features() !=
         signin::Tribool::kTrue;
}

bool ContextualCueingController::IsUrlEligibleForCue(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  if (google_util::IsGoogleSearchUrl(url)) {
    return false;
  }
  if (template_url_service_ &&
      template_url_service_->ExtractSearchMetadata(url)) {
    return false;
  }
  if (RE2::FullMatch(url.path(), kHomepagePathRegex)) {
    return false;
  }
  return true;
}

bool ContextualCueingController::IsAllowedToShowCue() {
  if (!sync_service_ ||
      !sync_service_->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kHistory)) {
    // If history sync is off, we cannot proceed to generate or show the cue.
    RecordContextualCueingDecision(ContextualCueingDecision::kHistorySyncOff);
    return false;
  }

  // Check if the user has opted out of contextual cues.
  PrefService* pref_service =
      browser_window_interface_->GetProfile()->GetPrefs();
  optimization_guide::prefs::FeatureOptInState opt_in_state = static_cast<
      optimization_guide::prefs::FeatureOptInState>(pref_service->GetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kContextualCueing)));
  if (opt_in_state == optimization_guide::prefs::FeatureOptInState::kDisabled) {
    RecordContextualCueingDecision(ContextualCueingDecision::kUserOptedOut);
    return false;
  }

  // Check enterprise policy.
  if (pref_service->GetInteger(optimization_guide::prefs::
                                   kContextualCueingEnterprisePolicyAllowed) ==
      static_cast<int>(optimization_guide::model_execution::prefs::
                           ModelExecutionEnterprisePolicyValue::kDisable)) {
    RecordContextualCueingDecision(
        ContextualCueingDecision::kDisabledByEnterprisePolicy);
    return false;
  }

#if !BUILDFLAG(IS_ANDROID)
  auto* browser_user_education_interface =
      BrowserUserEducationInterface::From(browser_window_interface_);
  if (browser_user_education_interface &&
      browser_user_education_interface->IsAnyFeaturePromoActive()) {
    RecordContextualCueingDecision(
        ContextualCueingDecision::kFeaturePromoActive);
    return false;
  }
#endif

  auto* infobar_manager = infobars::ContentInfoBarManager::FromWebContents(
      tab_list_interface_->GetActiveTab()->GetContents());
  if (infobar_manager && !infobar_manager->infobars().empty()) {
    RecordContextualCueingDecision(ContextualCueingDecision::kInfobarVisible);
    return false;
  }

  if (auto* side_panel_ui =
          SidePanelUIProvider::From(browser_window_interface_);
      side_panel_ui && side_panel_ui->IsSidePanelShowing()) {
    RecordContextualCueingDecision(ContextualCueingDecision::kSidePanelShowing);
    return false;
  }

  return true;
}

void ContextualCueingController::ShowCue(
    CueTargetType cue_type,
    const CueTarget& target,
    optimization_guide::proto::ContextualCueingResponse response) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED()
      << "Contextual cueing anchored message UI is not implemented for Android";
#else
  tabs::TabInterface* tab = tab_list_interface_->GetActiveTab();
  CHECK(tab);

  auto* action = actions::ActionManager::Get().FindAction(
      kActionAnchoredContextualCue, browser_window_interface_->GetFeatures()
                                        .browser_actions()
                                        ->root_action_item());
  CHECK(action);

  const auto& strings = response.anchored_message_cue();
  action->SetText(base::UTF8ToUTF16(strings.action_text()));
  action->SetImage(target.GetOmniboxChipIcon());
  action->SetInvokeActionCallback(base::BindRepeating(
      &ContextualCueingController::OnCueClicked, weak_ptr_factory_.GetWeakPtr(),
      cue_type, target.CueActionDataFromResponse(response)));

  page_actions::PageActionController* page_action_controller =
      tab->GetTabFeatures()->page_action_controller();
  if (!page_action_controller) {
    RecordContextualCueingDecision(ContextualCueingDecision::kNoActiveTab);
    return;
  }

  page_action_controller->Show(kActionAnchoredContextualCue);
  page_action_controller->SetAnchoredMessageIcon(
      kActionAnchoredContextualCue, target.GetAnchoredMessageIcon());
  page_action_controller->SetAnchoredMessageText(
      kActionAnchoredContextualCue,
      base::UTF8ToUTF16(strings.anchored_message_text()));
  page_action_controller->OverrideText(
      kActionAnchoredContextualCue, base::UTF8ToUTF16(strings.action_text()));

  auto menu_model = std::make_unique<ContextualCueingMenuModel>(
      browser_window_interface_->GetProfile(), weak_ptr_factory_.GetWeakPtr(),
      cue_type, target.CueActionDataFromResponse(response));
  page_action_controller->SetAnchoredMessageAction(
      kActionAnchoredContextualCue,
      page_actions::AnchoredMessageActionIconType::kMenu,
      std::move(menu_model));
  page_action_controller->ShowAnchoredMessage(
      kActionAnchoredContextualCue,
      {.priority = page_actions::PageActionPriorityCategory::kContextualCue});

  CUEING_LOG(base::StringPrintf(
      "Showing cue for CUJ %s: %s [%s]", response.suggested_cuj(),
      strings.anchored_message_text(), strings.action_text()));

  page_action_observer_->RegisterAsPageActionObserver(*page_action_controller);

  current_cuj_ = response.suggested_cuj();

  contextual_cueing_service_->OnCueShown(
      tab->GetContents()->GetLastCommittedURL());
#endif

  base::UmaHistogramSparse("ContextualCueing.ShownCueCUJ",
                           base::HashMetricName(response.suggested_cuj()));

  RecordContextualCueingDecision(ContextualCueingDecision::kSuccess);
}

void ContextualCueingController::OnCueHidden() {
  current_cuj_.clear();
}

void ContextualCueingController::OnCueClicked(
    CueTargetType cue_type,
    CueActionData data,
    actions::ActionItem*,
    actions::ActionInvocationContext) {
  CUEING_LOG(
      base::StringPrintf("Cue type '%s' was clicked", GetName(cue_type)));
  if (CueTarget* target = GetTarget(cue_type)) {
    target->OnClick(std::move(data));
  }
  contextual_cueing_service_->OnCueClicked(cue_type);

  RecordContextualCueingInteraction(ContextualCueingInteraction::kCueClicked,
                                    current_cuj_);

  HideCue();
}

void ContextualCueingController::HideCue() {
#if !BUILDFLAG(IS_ANDROID)
  tabs::TabInterface* active_tab = tab_list_interface_->GetActiveTab();
  if (!active_tab) {
    return;
  }
  page_actions::PageActionController* page_action_controller =
      active_tab->GetTabFeatures()->page_action_controller();
  if (!page_action_controller) {
    return;
  }
  page_action_controller->Hide(kActionAnchoredContextualCue);
#endif
}

CueTarget* ContextualCueingController::GetTarget(CueTargetType type) {
  auto iter = cue_targets_.find(type);
  return iter != cue_targets_.end() ? iter->second.get() : nullptr;
}

absl::flat_hash_set<optimization_guide::proto::ContextualCueingSurface>
ContextualCueingController::GetEligibleCueSurfaces() {
  absl::flat_hash_set<optimization_guide::proto::ContextualCueingSurface>
      eligible_cue_surfaces;
  for (const auto& [cue_type, target] : cue_targets_) {
    if (target->IsEligible() &&
        target->GetSurface() !=
            optimization_guide::proto::CONTEXTUAL_CUEING_SURFACE_UNSPECIFIED) {
      eligible_cue_surfaces.insert(target->GetSurface());
    }
  }
  return eligible_cue_surfaces;
}

}  // namespace contextual_cueing
