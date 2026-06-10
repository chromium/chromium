// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_controller.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "base/i18n/message_formatter.h"
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
#include "chrome/browser/contextual_cueing/prefs.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
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
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/side_panel/side_panel_ui_provider.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/favicon/core/favicon_service.h"
#include "components/google/core/common/google_util.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/optimization_guide_common.mojom.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/pdf/common/constants.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/tabs/public/tab_handle_factory.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
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

std::optional<CueTargetType> GetTargetType(
    optimization_guide::proto::ContextualCue::FulfillmentSurfaceCase
        fulfillment_surface_case) {
  using enum optimization_guide::proto::ContextualCue::FulfillmentSurfaceCase;
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
  ContextualCueingPageActionObserver(
      base::RepeatingCallback<void(CueFormFactor)> shown_callback,
      base::RepeatingCallback<void(CueFormFactor)> hidden_callback)
      : page_actions::PageActionObserver(kActionAnchoredContextualCue),
        shown_callback_(std::move(shown_callback)),
        hidden_callback_(std::move(hidden_callback)) {}

  void OnPageActionIconShown(const page_actions::PageActionState&) override {
    shown_callback_.Run(CueFormFactor::kIcon);
  }

  void OnPageActionIconHidden(const page_actions::PageActionState&) override {
    hidden_callback_.Run(CueFormFactor::kIcon);
  }

  void OnPageActionChipShown(const page_actions::PageActionState&) override {
    shown_callback_.Run(CueFormFactor::kChip);
  }

  void OnPageActionChipHidden(const page_actions::PageActionState&) override {
    hidden_callback_.Run(CueFormFactor::kChip);
  }

  void OnPageActionAnchoredMessageShown(
      const page_actions::PageActionState&) override {
    shown_callback_.Run(CueFormFactor::kAnchoredMessage);
  }

  void OnPageActionAnchoredMessageHidden(
      const page_actions::PageActionState&) override {
    hidden_callback_.Run(CueFormFactor::kAnchoredMessage);
  }

 private:
  base::RepeatingCallback<void(CueFormFactor)> shown_callback_;
  base::RepeatingCallback<void(CueFormFactor)> hidden_callback_;
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
          browser_window_interface_->GetProfile())),
      favicon_service_(FaviconServiceFactory::GetForProfile(
          browser_window_interface_->GetProfile(),
          ServiceAccessType::EXPLICIT_ACCESS)) {
#if !BUILDFLAG(IS_ANDROID)
  page_action_observer_ = std::make_unique<ContextualCueingPageActionObserver>(
      base::BindRepeating(&ContextualCueingController::OnCueFormFactorShown,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&ContextualCueingController::OnCueFormFactorHidden,
                          weak_ptr_factory_.GetWeakPtr()));
#endif
  if (page_content_annotations_service_) {
    page_content_annotations_service_->AddObserver(
        page_content_annotations::AnnotationType::kCategoryClassifier, this);
  }
  if (tab_list_interface_) {
    tab_list_interface_->AddTabListInterfaceObserver(this);
  }
}

ContextualCueingController::~ContextualCueingController() {
  if (page_content_annotations_service_) {
    page_content_annotations_service_->RemoveObserver(
        page_content_annotations::AnnotationType::kCategoryClassifier, this);
  }
  if (tab_list_interface_) {
    tab_list_interface_->RemoveTabListInterfaceObserver(this);
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
    CUEING_LOG(base::StringPrintf(
        "Ignoring category classification - received category classification "
        "not for current active tab. Received URL: %s Current URL: %s",
        visit.url.spec(),
        active_web_contents ? active_web_contents->GetLastCommittedURL().spec()
                            : "unknown"));
    return;
  }

  ukm::SourceId source_id = GetActiveTabSourceId();
  if (!IsUrlEligibleForCue(active_web_contents->GetLastCommittedURL())) {
    CUEING_LOG(
        base::StringPrintf("%s ineligible for cue: URL is ineligible.",
                           active_web_contents->GetLastCommittedURL().spec()));
    RecordContextualCueingDecision(source_id,
                                   ContextualCueingDecision::kUrlNotEligible);
    return;
  }

  if (GetEligibleCueSurfaces().empty()) {
    CUEING_LOG(
        base::StringPrintf("%s ineligible for cue: No eligible cue surfaces.",
                           active_web_contents->GetLastCommittedURL().spec()));
    RecordContextualCueingDecision(
        source_id, ContextualCueingDecision::kNoEligibleCueSurfaces);
    return;
  }

  if (auto decision = IsAllowedToShowCue();
      decision != ContextualCueingDecision::kUnspecified) {
    CUEING_LOG(
        base::StringPrintf("%s ineligible for cue with reason: %d.",
                           active_web_contents->GetLastCommittedURL().spec(),
                           static_cast<int>(decision)));
    // Cueing decision already recorded in IsAllowedToShowCue().
    return;
  }

  // Check classification to see if we should proceed to next step.
  bool passes_edu = false;
  bool passes_shopping = false;
  for (const page_content_annotations::Category& category :
       result.GetCategoryResults()) {
    if (category.category_type ==
            page_content_annotations::CategoryType::kEducation &&
        category.score > kEduClassifierThreshold.Get()) {
      passes_edu = true;
    }
    if (category.category_type ==
            page_content_annotations::CategoryType::kShopping &&
        category.score > kShoppingClassifierThreshold.Get()) {
      passes_shopping = true;
    }
  }

  bool is_supported_category = false;
  if (kDiscardShoppingPdfs.Get() &&
      active_web_contents->GetContentsMimeType() == pdf::kPDFMimeType) {
    is_supported_category = passes_edu && !passes_shopping;
  } else {
    is_supported_category = passes_edu || passes_shopping;
  }

  if (!is_supported_category) {
    CUEING_LOG(base::StringPrintf(
        "%s ineligible for cue: Failed category classification.",
        active_web_contents->GetLastCommittedURL().spec()));
    RecordContextualCueingDecision(
        source_id, ContextualCueingDecision::kFailedCategoryClassification);
    return;
  }

  if (auto decision = contextual_cueing_service_->CanShowCue(
          active_web_contents->GetLastCommittedURL());
      decision != ContextualCueingDecision::kSuccess) {
    CUEING_LOG(
        base::StringPrintf("%s ineligible for cue with reason: %d.",
                           active_web_contents->GetLastCommittedURL().spec(),
                           static_cast<int>(decision)));
    RecordContextualCueingDecision(source_id, decision);
    return;
  }

  CUEING_LOG(
      base::StringPrintf("%s eligible for cue: Category classification "
                         "succeeded. Initiating model execution request.",
                         active_web_contents->GetLastCommittedURL().spec()));
  InitiateModelExecutionRequest();
}

void ContextualCueingController::OnActiveTabChanged(TabListInterface& tab_list,
                                                    tabs::TabInterface* tab) {
  if (tab) {
    ActiveTabUrlChanged(tab->GetURL());
  }
}

void ContextualCueingController::ActiveTabUrlChanged(const GURL& url) {
  if (url == last_logged_active_url_) {
    return;
  }
  last_logged_active_url_ = url;
  CUEING_LOG(
      base::StringPrintf("Active tab URL changed to %s", url.spec().c_str()));
}

void ContextualCueingController::InitiateModelExecutionRequest() {
  tabs::TabInterface* active_tab = tab_list_interface_->GetActiveTab();
  CHECK(active_tab);
  content::WebContents* active_web_contents = active_tab->GetContents();
  CHECK(active_web_contents);

  tab_favicons_.clear();
  FetchFavicon(active_tab, active_web_contents);

  ukm::SourceId source_id = GetActiveTabSourceId();

  if (!optimization_guide_keyed_service_) {
    RecordContextualCueingDecision(
        source_id, ContextualCueingDecision::kModelExecutionUnavailable);
    return;
  }

  optimization_guide::proto::ContextualCueingRequest request;
  request.mutable_active_tab_page_context()->set_url(
      active_web_contents->GetLastCommittedURL().spec());
  request.mutable_active_tab_page_context()->set_title(
      base::UTF16ToUTF8(active_web_contents->GetTitle()));

  struct BackgroundTabInfo {
    base::Time last_active_time;
    raw_ptr<tabs::TabInterface> tab;
    raw_ptr<content::WebContents> contents;
  };
  std::vector<BackgroundTabInfo> background_tabs;
  for (int i = 0; i < tab_list_interface_->GetTabCount(); ++i) {
    tabs::TabInterface* tab = tab_list_interface_->GetTab(i);
    if (tab == active_tab) {
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
         .tab = tab,
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
    FetchFavicon(background_tabs[i].tab, background_tabs[i].contents);
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

void ContextualCueingController::FetchFavicon(
    tabs::TabInterface* tab,
    content::WebContents* web_contents) {
  if (!favicon_service_ || !web_contents || !tab) {
    return;
  }

  favicon_service_->GetFaviconImageForPageURL(
      web_contents->GetLastCommittedURL(),
      base::BindOnce(&ContextualCueingController::OnFaviconAvailable,
                     weak_ptr_factory_.GetWeakPtr(), tab->GetHandle()),
      &cancelable_task_tracker_);
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
    OnShowCueFailed(
        ContextualCueingDecision::kNoLongerActiveTabAfterModelExecution);
    return;
  }

  if (!result.response.has_value()) {
    CUEING_LOG("Model execution to generate cue failed.");
    OnShowCueFailed(ContextualCueingDecision::kModelExecutionFailed);
    return;
  }

  std::optional<optimization_guide::proto::ContextualCueingResponse> response =
      optimization_guide::ParsedAnyMetadata<
          optimization_guide::proto::ContextualCueingResponse>(
          *result.response);
  if (!response) {
    CUEING_LOG("Model execution to generate cue failed: couldn't parse proto.");
    OnShowCueFailed(
        ContextualCueingDecision::kModelExecutionResponseFailedToParse);
    return;
  }

  if (response->contextual_cues_size() == 0) {
    CUEING_LOG("Model execution to generate cue failed: no cues returned.");
    OnShowCueFailed(ContextualCueingDecision::kNoCues);
    return;
  }

  // TODO(crbug.com/515865902): Handle multiple cues. For now only use the first
  // one.
  const auto& cue = response->contextual_cues(0);

  if (!cue.has_anchored_message_cue() ||
      cue.anchored_message_cue().anchored_message_text().empty() ||
      cue.anchored_message_cue().action_text().empty()) {
    CUEING_LOG(
        "Model execution to generate cue failed: missing anchored message "
        "text.");
    OnShowCueFailed(ContextualCueingDecision::kMissingAnchoredMessageText);
    return;
  }

  std::optional<CueTargetType> target_type =
      GetTargetType(cue.fulfillment_surface_case());
  if (!target_type) {
    CUEING_LOG("Unknown fulfillment surface");
    OnShowCueFailed(ContextualCueingDecision::kUnknownFulfillmentSurface);
    return;
  }

  CueTarget* target = GetTarget(*target_type);
  if (!target) {
    CUEING_LOG(base::StringPrintf("No CueTarget registered for '%s'",
                                  GetName(*target_type)));
    OnShowCueFailed(ContextualCueingDecision::kTargetFeatureNotRegistered);
    return;
  }

  if (IsUserSubjectToAgeRestrictions()) {
    OnShowCueFailed(ContextualCueingDecision::kAgeRestrictionEnforced);
    return;
  }

  if (!target->IsEligible()) {
    CUEING_LOG(base::StringPrintf("Not eligible for '%s' cues",
                                  GetName(*target_type)));
    OnShowCueFailed(ContextualCueingDecision::kTargetFeatureNotEligible);
    return;
  }

  if (IsAllowedToShowCue() == ContextualCueingDecision::kUnspecified) {
    ShowCue(*target_type, *target, cue);
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

ukm::SourceId ContextualCueingController::GetActiveTabSourceId() const {
  tabs::TabInterface* active_tab = tab_list_interface_->GetActiveTab();
  return active_tab ? active_tab->GetContents()
                          ->GetPrimaryMainFrame()
                          ->GetPageUkmSourceId()
                    : ukm::kInvalidSourceId;
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

ContextualCueingDecision ContextualCueingController::IsAllowedToShowCue() {
  ukm::SourceId source_id = GetActiveTabSourceId();

  if (!sync_service_ ||
      !sync_service_->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kHistory)) {
    CUEING_LOG("History sync is off.");
    // If history sync is off, we cannot proceed to generate or show the cue.
    RecordContextualCueingDecision(source_id,
                                   ContextualCueingDecision::kHistorySyncOff);
    return ContextualCueingDecision::kHistorySyncOff;
  }

  // Check if the user has opted out of contextual cues.
  PrefService* pref_service =
      browser_window_interface_->GetProfile()->GetPrefs();
  optimization_guide::prefs::FeatureOptInState opt_in_state = static_cast<
      optimization_guide::prefs::FeatureOptInState>(pref_service->GetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kContextualCueing)));
  if (opt_in_state == optimization_guide::prefs::FeatureOptInState::kDisabled) {
    CUEING_LOG(
        "Not attempting to show/generate cue because user has opted out of "
        "contextual cues.");
    RecordContextualCueingDecision(source_id,
                                   ContextualCueingDecision::kUserOptedOut);
    return ContextualCueingDecision::kUserOptedOut;
  }

  // Check enterprise policy.
  if (pref_service->GetInteger(
          optimization_guide::prefs::kChromeSuggestionsSettings) ==
      static_cast<int>(
          contextual_cueing::ChromeSuggestionsSettingsValue::kDisabled)) {
    CUEING_LOG(
        "Not attempting to show/generate cue because enterprise policy has "
        "disabled contextual cues.");
    RecordContextualCueingDecision(
        source_id, ContextualCueingDecision::kDisabledByEnterprisePolicy);
    return ContextualCueingDecision::kDisabledByEnterprisePolicy;
  }

#if !BUILDFLAG(IS_ANDROID)
  auto* browser_user_education_interface =
      BrowserUserEducationInterface::From(browser_window_interface_);
  if (browser_user_education_interface &&
      browser_user_education_interface->IsAnyFeaturePromoActive()) {
    CUEING_LOG(
        "Not attempting to show/generate cue because a feature promo is "
        "active.");
    RecordContextualCueingDecision(
        source_id, ContextualCueingDecision::kFeaturePromoActive);
    return ContextualCueingDecision::kFeaturePromoActive;
  }
#endif

  auto* infobar_manager = infobars::ContentInfoBarManager::FromWebContents(
      tab_list_interface_->GetActiveTab()->GetContents());
  if (infobar_manager && !infobar_manager->infobars().empty()) {
    CUEING_LOG(
        "Not attempting to show/generate cue because infobar is visible.");
    RecordContextualCueingDecision(source_id,
                                   ContextualCueingDecision::kInfobarVisible);
    return ContextualCueingDecision::kInfobarVisible;
  }

  if (auto* side_panel_ui =
          SidePanelUIProvider::From(browser_window_interface_);
      side_panel_ui && side_panel_ui->IsSidePanelShowing()) {
    CUEING_LOG(
        "Not attempting to show/generate cue because side panel is visible.");
    RecordContextualCueingDecision(source_id,
                                   ContextualCueingDecision::kSidePanelShowing);
    return ContextualCueingDecision::kSidePanelShowing;
  }

#if !BUILDFLAG(IS_ANDROID)
  if (tabs::TabInterface* active_tab = tab_list_interface_->GetActiveTab()) {
    if (page_actions::PageActionController* page_action_controller =
            active_tab->GetTabFeatures()->page_action_controller()) {
      if (page_action_controller->GetActiveAnchoredMessage().has_value()) {
        CUEING_LOG(
            "Not attempting to show/generate cue because another anchored "
            "message is currently showing.");
        RecordContextualCueingDecision(
            source_id,
            ContextualCueingDecision::kAnchoredMessageAlreadyShowing);
        return ContextualCueingDecision::kAnchoredMessageAlreadyShowing;
      }
    }
  }
#endif

  return ContextualCueingDecision::kUnspecified;
}

std::pair<std::vector<tabs::TabHandle>, CueTabMetrics>
ContextualCueingController::GetTabsToShow(
    const optimization_guide::proto::ContextualCue& cue) {
  std::vector<tabs::TabHandle> tabs_to_show;
  CueTabMetrics tab_metrics;
  auto& tab_handle_factory = tabs::SessionMappedTabHandleFactory::GetInstance();
  for (auto& tab : cue.anchored_message_cue().tabs_to_show()) {
    SessionID session_id = SessionID::FromSerializedValue(
        static_cast<SessionID::id_type>(tab.tab_id()));
    if (!session_id.is_valid()) {
      ++tab_metrics.missing_count;
      continue;
    }

    tabs::TabHandle handle(
        tab_handle_factory.GetHandleForSessionId(session_id.id()));
    // Ensure tab is valid and belongs to the current browser window.
    if (handle.Get() && handle.Get()->GetContents() &&
        handle.Get()->GetBrowserWindowInterface() ==
            browser_window_interface_) {
      // Check whether the tab's current URL still matches the URL from the
      // response. If the tab has navigated away, skip it.
      GURL response_url(tab.url());
      const GURL& live_url = handle.Get()->GetContents()->GetLastCommittedURL();
      if (!response_url.EqualsIgnoringRef(live_url)) {
        ++tab_metrics.navigated_away_count;
        continue;
      }

      tabs_to_show.push_back(handle);
      ++tab_metrics.matched_count;
    } else {
      ++tab_metrics.missing_count;
    }
  }

  // Also show the active tab if it isn't already shown.
  tabs::TabHandle active_handle =
      tab_list_interface_->GetActiveTab()->GetHandle();
  if (std::find(tabs_to_show.begin(), tabs_to_show.end(), active_handle) ==
      tabs_to_show.end()) {
    tabs_to_show.push_back(active_handle);
  }

  CUEING_LOG(base::StringPrintf("%d tabs to show.", tabs_to_show.size()));
  return {tabs_to_show, tab_metrics};
}

void ContextualCueingController::ShowCue(
    CueTargetType cue_type,
    const CueTarget& target,
    const optimization_guide::proto::ContextualCue& cue) {
  auto [tabs_to_show, tab_metrics] = GetTabsToShow(cue);
  CueActionData action_data =
      target.CueActionDataFromResponse(cue, tabs_to_show);

  tabs::TabInterface* active_tab = tab_list_interface_->GetActiveTab();
  CHECK(active_tab);

  base::TimeDelta show_latency;
  base::Time page_load_time = active_tab->GetContents()
                                  ->GetController()
                                  .GetLastCommittedEntry()
                                  ->GetTimestamp();
  if (!page_load_time.is_null()) {
    show_latency = base::Time::Now() - page_load_time;
  }

  RecordCueShownMetrics(GetActiveTabSourceId(), cue.suggested_cuj(),
                        tab_metrics, show_latency);
  cue_hidden_time_ = base::TimeTicks();
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED()
      << "Contextual cueing anchored message UI is not implemented for Android";
#else
  auto* action = actions::ActionManager::Get().FindAction(
      kActionAnchoredContextualCue, browser_window_interface_->GetFeatures()
                                        .browser_actions()
                                        ->root_action_item());
  CHECK(action);

  const auto& strings = cue.anchored_message_cue();
  action->SetText(base::UTF8ToUTF16(strings.action_text()));
  action->SetImage(target.GetOmniboxChipIcon());
  action->SetInvokeActionCallback(base::BindRepeating(
      &ContextualCueingController::OnCueClicked, weak_ptr_factory_.GetWeakPtr(),
      cue_type, cue.suggested_cuj(), action_data));

  page_actions::PageActionController* page_action_controller =
      active_tab->GetTabFeatures()->page_action_controller();
  if (!page_action_controller) {
    RecordContextualCueingDecision(GetActiveTabSourceId(),
                                   ContextualCueingDecision::kNoActiveTab);
    return;
  }

  ObserveSidePanel();

  page_action_controller->Show(kActionAnchoredContextualCue);
  page_action_controller->SetAnchoredMessageIcon(
      kActionAnchoredContextualCue, target.GetAnchoredMessageIcon());
  page_action_controller->SetAnchoredMessageText(
      kActionAnchoredContextualCue,
      base::UTF8ToUTF16(strings.anchored_message_text()));
  page_action_controller->OverrideText(
      kActionAnchoredContextualCue, base::UTF8ToUTF16(strings.action_text()));
  page_action_controller->OverrideImage(kActionAnchoredContextualCue,
                                        target.GetOmniboxChipIcon());

  auto menu_model = std::make_unique<ContextualCueingMenuModel>(
      browser_window_interface_->GetProfile(), weak_ptr_factory_.GetWeakPtr(),
      cue_type, cue.suggested_cuj(), std::move(action_data));
  page_action_controller->SetAnchoredMessageAction(
      kActionAnchoredContextualCue,
      page_actions::AnchoredMessageActionIconType::kMenu,
      std::move(menu_model));

  MaybeShowTabList(page_action_controller, tabs_to_show);

  page_action_controller->ShowAnchoredMessage(
      kActionAnchoredContextualCue,
      {.priority = page_actions::PageActionPriorityCategory::kContextualCue});

  CUEING_LOG(base::StringPrintf(
      "Showing cue for CUJ %s: %s [%s]", cue.suggested_cuj(),
      strings.anchored_message_text(), strings.action_text()));

  page_action_observer_->RegisterAsPageActionObserver(*page_action_controller);

  contextual_cueing_service_->OnCueShown(
      active_tab->GetContents()->GetLastCommittedURL());
#endif

  base::UmaHistogramSparse("ContextualCueing.ShownCueCUJ",
                           base::HashMetricName(cue.suggested_cuj()));

  RecordContextualCueingDecision(GetActiveTabSourceId(),
                                 ContextualCueingDecision::kSuccess);
}

#if !BUILDFLAG(IS_ANDROID)
void ContextualCueingController::MaybeShowTabList(
    page_actions::PageActionController* page_action_controller,
    const std::vector<tabs::TabHandle>& tabs_to_show) {
  page_action_controller->SetAnchoredMessageExpandableContent(
      kActionAnchoredContextualCue, std::nullopt);

  const TabListVisibility visibility_mode = kTabListVisibility.Get();
  if (visibility_mode == TabListVisibility::kNever) {
    return;
  }

  const size_t min_tab_count =
      visibility_mode == TabListVisibility::kOnlyIfMultiple ? 2ul : 1ul;
  if (tabs_to_show.size() < min_tab_count) {
    return;
  }

  int missing_favicon_count = 0;
  std::vector<page_actions::AnchoredMessageExpandableItem> tab_items;
  tab_items.reserve(tabs_to_show.size());
  base::flat_set<std::string> domains;
  for (tabs::TabHandle handle : tabs_to_show) {
    const tabs::TabInterface* tab = handle.Get();
    if (!tab) {
      continue;
    }

    // TODO(crbug.com/507551989): Display "Current tab" on the active tab's
    // entry.
    std::u16string title = tab->GetTitle();
    CUEING_LOG(base::StringPrintf("title: %s", base::UTF16ToUTF8(title)));

    ui::ImageModel favicon;
    auto it = tab_favicons_.find(handle);
    if (it != tab_favicons_.end()) {
      favicon = it->second;
    } else {
      favicon = favicon::GetDefaultFaviconModel();
      ++missing_favicon_count;
    }

    tab_items.emplace_back(std::move(favicon), std::move(title));
    domains.insert(tab->GetURL().GetHost());
  }

  if (tab_items.size() < min_tab_count) {
    return;
  }

  base::UmaHistogramExactLinear(
      "ContextualCueing.V2.MissingFaviconCount", missing_favicon_count,
      // Exclusive max of background tabs plus active tab.
      kMaxNumBackgroundTabs.Get() + 2);

  // Tab list heading.
  std::u16string heading = l10n_util::GetPluralStringFUTF16(
      IDS_CONTEXTUAL_CUEING_TAB_SHARING_HEADING, tab_items.size());
  CUEING_LOG(base::StringPrintf("heading: %s", base::UTF16ToUTF8(heading)));

  std::u16string expand_announcement =
      base::i18n::MessageFormatter::FormatWithNamedArgs(
          l10n_util::GetStringUTF16(
              IDS_CONTEXTUAL_CUEING_TAB_SHARING_EXPAND_BUTTON_ANNOUNCEMENT),
          "NUM_TABS", static_cast<int>(tab_items.size()), "WEBSITE_LIST_STR",
          base::UTF8ToUTF16(base::JoinString(domains, ", ")));
  CUEING_LOG(base::StringPrintf("expand button a11y string: %s",
                                base::UTF16ToUTF8(expand_announcement)));

  page_action_controller->SetAnchoredMessageExpandableContent(
      kActionAnchoredContextualCue,
      std::make_optional<page_actions::AnchoredMessageExpandableContent>(
          {.heading = heading,
           .items = std::move(tab_items),
           .expand_button_tooltip = std::move(expand_announcement)}));
}
#endif

void ContextualCueingController::OnCueHidden() {
  cue_hidden_time_ = base::TimeTicks();
  cue_shown_time_ = base::TimeTicks();
}

void ContextualCueingController::OnCueFormFactorShown(
    CueFormFactor form_factor) {
  RecordCueFormFactorShown(form_factor);
  if (form_factor == CueFormFactor::kAnchoredMessage) {
    cue_shown_time_ = base::TimeTicks::Now();
  }
}

void ContextualCueingController::OnCueFormFactorHidden(
    CueFormFactor form_factor) {
  RecordCueFormFactorHidden(form_factor);
  if (form_factor == CueFormFactor::kAnchoredMessage) {
    cue_hidden_time_ = base::TimeTicks::Now();
  }
  // Resets the cue state and shown timer only when the entire page action
  // icon is hidden, preserving the original contextual cue lifecycle behavior.
  if (form_factor == CueFormFactor::kIcon) {
    OnCueHidden();
  }
}

void ContextualCueingController::OnFaviconAvailable(
    tabs::TabHandle handle,
    const favicon_base::FaviconImageResult& image_result) {
  if (!image_result.image.IsEmpty()) {
    tab_favicons_[handle] = ui::ImageModel::FromImage(image_result.image);
  }
}

void ContextualCueingController::OnShowCueFailed(
    ContextualCueingDecision decision) {
  RecordContextualCueingDecision(GetActiveTabSourceId(), decision);
  cancelable_task_tracker_.TryCancelAll();
}

void ContextualCueingController::OnSidePanelShown() {
  HideCue();
}

void ContextualCueingController::OnCueClicked(
    CueTargetType cue_type,
    std::string cuj,
    CueActionData action,
    actions::ActionItem*,
    actions::ActionInvocationContext) {
  CUEING_LOG(
      base::StringPrintf("Cue type '%s' was clicked", GetName(cue_type)));
#if !BUILDFLAG(IS_ANDROID)
  CHECK(page_action_observer_);
  const page_actions::PageActionState& state =
      page_action_observer_->GetCurrentPageActionState();
  if (!state.anchored_message_showing) {
    tabs::TabInterface* active_tab = tab_list_interface_->GetActiveTab();
    if (active_tab) {
      // Re-show the anchored message to allow for the user to see the tab
      // sharing UI before invoking the cue target's click action
      if (page_actions::PageActionController* page_action_controller =
              active_tab->GetTabFeatures()->page_action_controller()) {
        page_action_controller->ShowAnchoredMessage(
            kActionAnchoredContextualCue,
            {.priority =
                 page_actions::PageActionPriorityCategory::kUserInteraction});
        if (!cue_hidden_time_.is_null()) {
          base::TimeDelta collapsed_duration =
              base::TimeTicks::Now() - cue_hidden_time_;
          RecordChipClickedCollapsedDuration(collapsed_duration);
          cue_hidden_time_ = base::TimeTicks();
        }
      }
    }
    return;
  }
#endif

  OnCueInteraction(ContextualCueingInteraction::kCueClicked, cue_type, cuj,
                   std::move(action));
}

void ContextualCueingController::OnCueInteraction(
    ContextualCueingInteraction interaction_type,
    CueTargetType cue_type,
    const std::string& cuj,
    CueActionData action) {
  base::TimeDelta shown_duration = ExtractCueShownDuration();
  ukm::SourceId source_id = GetActiveTabSourceId();

  RecordContextualCueingInteraction(interaction_type, cuj, source_id,
                                    shown_duration);

  HideCue();

  switch (interaction_type) {
    case ContextualCueingInteraction::kCueDismissed:
      contextual_cueing_service_->OnCueDismissed(cue_type);
      break;
    case ContextualCueingInteraction::kCueEditPrompt:
      if (CueTarget* target = GetTarget(cue_type)) {
        target->OnEditPrompt(std::move(action));
      }
      break;
    case ContextualCueingInteraction::kCueSuggestionsSettings:
      chrome::ShowSettingsSubPageForProfile(
          browser_window_interface_->GetProfile(), chrome::kSuggestionsSubPage);
      break;
    case ContextualCueingInteraction::kCueClicked:
      if (CueTarget* target = GetTarget(cue_type)) {
        target->OnClick(std::move(action));
      }
      contextual_cueing_service_->OnCueClicked(cue_type);
      break;
  }
}

base::TimeDelta ContextualCueingController::ExtractCueShownDuration() {
  if (cue_shown_time_.is_null()) {
    return base::TimeDelta();
  }
  base::TimeDelta duration = base::TimeTicks::Now() - cue_shown_time_;
  cue_shown_time_ = base::TimeTicks();
  return duration;
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
  page_action_controller->HideAnchoredMessage(kActionAnchoredContextualCue);
  page_action_controller->Hide(kActionAnchoredContextualCue);
#endif
}

void ContextualCueingController::ObserveSidePanel() {
  if (side_panel_shown_subscription_) {
    return;
  }
  if (auto* side_panel_ui =
          SidePanelUIProvider::From(browser_window_interface_)) {
    side_panel_shown_subscription_ = side_panel_ui->RegisterSidePanelShown(
        base::BindRepeating(&ContextualCueingController::OnSidePanelShown,
                            weak_ptr_factory_.GetWeakPtr()));
  }
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
