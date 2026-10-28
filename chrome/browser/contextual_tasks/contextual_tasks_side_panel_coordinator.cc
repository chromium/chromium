// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_search_session_finder.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_auto_suggestion_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_thread_observer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_permission_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_tab_restore_data.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_interface.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/sessions/core/session_id.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/url_util.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_tasks/contextual_tasks_onboarding_tooltip_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_specification.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/contextual_tasks/contextual_tasks_extensions_container.h"
#endif
#include "ui/base/interaction/element_tracker.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_handler_document_singleton.h"
#endif
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "ui/actions/actions.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/tab_helper.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/view_type_utils.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

#if !BUILDFLAG(IS_ANDROID)
std::string GetContextualTasksArmShortName() {
  using contextual_tasks::ExpandButtonOption;
  ExpandButtonOption expand_button = contextual_tasks::GetExpandButtonOption();
  bool lens_enabled = contextual_tasks::GetEnableLensInContextualTasks();
  bool tab_auto_chip = contextual_tasks::GetIsTabAutoSuggestionChipEnabled();
  const auto entrypoint = contextual_tasks::kShowEntryPoint.Get();

  if (expand_button == ExpandButtonOption::kSidePanelExpandButton) {
    if (lens_enabled && tab_auto_chip) {
      return "Arm 1";
    }
    if (!lens_enabled && tab_auto_chip) {
      return "Arm 3";
    }
    if (lens_enabled && !tab_auto_chip) {
      return "Arm 4";
    }
  } else if (expand_button == ExpandButtonOption::kToolbarCloseButton) {
    if (lens_enabled && tab_auto_chip &&
        entrypoint !=
            contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
      return "Arm 5";
    }
    if (!lens_enabled && tab_auto_chip) {
      return "Arm 6";
    }
    if (lens_enabled && !tab_auto_chip) {
      return "Arm 7";
    }
    if (lens_enabled && tab_auto_chip &&
        entrypoint ==
            contextual_tasks::EntryPointOption::kToolbarEphemeralBranded) {
      return "Arm 8";
    }
  }

  return "Default";
}
#endif  // !BUILDFLAG(IS_ANDROID)

std::unique_ptr<content::WebContents> CreateWebContents(
    BrowserWindowInterface* browser_window) {
  content::WebContents::CreateParams create_params(
      browser_window->GetProfile());
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);
  webui::SetBrowserWindowInterface(web_contents.get(), browser_window);

#if !BUILDFLAG(IS_ANDROID)
  if (contextual_tasks::IsContextualTasksSidePanelRearchitectureEnabled()) {
    contextual_tasks::ContextualTasksWebView::ConfigureWebContentsBackground(
        web_contents.get(), browser_window->GetProfile());
  }
#endif

  // Create PermissionRequestManager and PermissionRecoverySuccessRateTracker
  // explicitly for this WebContents. The permission bubble will anchor to
  // the browser window via BrowserWindowInterface.
  permissions::PermissionRequestManager::CreateForWebContents(
      web_contents.get());
  permissions::PermissionRecoverySuccessRateTracker::CreateForWebContents(
      web_contents.get());

  // Create PageSpecificContentSettings for chip indicating which
  // permissions are being used currently.
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      web_contents.get(), std::make_unique<PageSpecificContentSettingsDelegate>(
                              web_contents.get()));
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // WebContents created here never go through TabHelpers::AttachTabHelpers(),
  // unlike the ones the panel adopts from a real tab via
  // TransferWebContentsFromTab(). The extensions menu, which can be opened
  // from the side panel's page info bubble, reads extension state for the
  // active WebContents through extensions::TabHelper and dereferences it
  // without a null check, so attach it explicitly.
  extensions::TabHelper::CreateForWebContents(web_contents.get());

#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

  // Register the task `WebContents` with the side panel's single location bar.
  if (auto* controller = contextual_tasks::ContextualTasksPanelController::From(
          browser_window);
      controller && controller->permission_controller()) {
    controller->permission_controller()->RegisterWebContents(
        web_contents.get());
  }

  return web_contents;
}

// Take a detached web contents and unset the embedder context data.
// Detached web contents will no longer have a TabInterface, but the
// embedder context data will still maintain a tab tracker that needs to
// be unset.
void SetBrowserWindowInterface(content::WebContents* web_contents,
                               BrowserWindowInterface* browser_window) {
  webui::SetTabInterface(web_contents, nullptr);
  webui::SetBrowserWindowInterface(web_contents, browser_window);
}

std::set<SessionID> GetAllTabIdsInTabList(TabListInterface* tab_list) {
  std::set<SessionID> tab_ids;
  for (tabs::TabInterface* tab : tab_list->GetAllTabs()) {
    tab_ids.insert(sessions::SessionTabHelper::IdForTab(tab->GetContents()));
  }
  return tab_ids;
}

void RecordUserActionAndHistogram(const std::string& metric_name) {
  base::RecordAction(base::UserMetricsAction(metric_name.c_str()));
  base::UmaHistogramBoolean(metric_name, true);
}

std::vector<tabs::TabHandle> FilterValidTabHandlesForSharing(
    const std::vector<tabs::TabHandle>& tab_handles) {
  std::vector<tabs::TabHandle> valid_tab_handles;
  for (tabs::TabHandle tab_handle : tab_handles) {
    tabs::TabInterface* tab = tab_handle.Get();
    if (!tab || !tab->GetContents()) {
      continue;
    }
    if (contextual_tasks::IsUrlValidForContextSharing(
            tab->GetContents()->GetLastCommittedURL()) &&
        !std::ranges::contains(valid_tab_handles, tab_handle)) {
      valid_tab_handles.push_back(tab_handle);
    }
  }
  return valid_tab_handles;
}

std::vector<tabs::TabHandle> BuildTabsToAttachWithActiveTab(
    BrowserWindowInterface* browser_window,
    const std::vector<tabs::TabHandle>& valid_tab_handles) {
  std::vector<tabs::TabHandle> tabs_to_attach;
  if (TabListInterface* tab_list = TabListInterface::From(browser_window)) {
    if (tabs::TabInterface* active_tab = tab_list->GetActiveTab();
        active_tab && active_tab->GetContents() &&
        contextual_tasks::IsUrlValidForContextSharing(
            active_tab->GetContents()->GetLastCommittedURL())) {
      tabs_to_attach.push_back(active_tab->GetHandle());
    }
  }
  for (tabs::TabHandle tab_handle : valid_tab_handles) {
    if (!std::ranges::contains(tabs_to_attach, tab_handle)) {
      tabs_to_attach.push_back(tab_handle);
    }
  }
  return tabs_to_attach;
}

}  // namespace

namespace contextual_tasks {

class ContextualTasksSidePanelCoordinator::SidePanelWebContentsObserver
    : public content::WebContentsObserver {
 public:
  SidePanelWebContentsObserver(content::WebContents* web_contents,
                               PaintCallback on_fcp_callback,
                               base::RepeatingClosure on_navigation_callback)
      : content::WebContentsObserver(web_contents),
        on_fcp_callback_(std::move(on_fcp_callback)),
        on_navigation_callback_(std::move(on_navigation_callback)) {}
  ~SidePanelWebContentsObserver() override = default;

  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override {
    if (on_fcp_callback_ && web_contents()) {
      std::move(on_fcp_callback_).Run(web_contents());
    }
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle || !navigation_handle->IsInPrimaryMainFrame()) {
      return;
    }
    if (navigation_handle->HasCommitted() && on_navigation_callback_) {
      on_navigation_callback_.Run();
    }
    if (!IsContextualTasksSidePanelRearchitectureEnabled()) {
      return;
    }
    auto* ui_service =
        ContextualTasksUiServiceFactory::GetForBrowserContextIfExists(
            web_contents()->GetBrowserContext());
    const bool is_zero_state =
        ui_service &&
        ContextualTasksUI::IsZeroState(navigation_handle->GetURL(), ui_service);
    RecordContextualTasksNavigationMetrics(navigation_handle, is_zero_state);
  }

 private:
  PaintCallback on_fcp_callback_;
  base::RepeatingClosure on_navigation_callback_;
};

ContextualTasksSidePanelCoordinator::WebContentsCacheItem::WebContentsCacheItem(
    std::unique_ptr<content::WebContents> wc,
    bool open,
    PaintCallback on_fcp_callback,
    base::RepeatingClosure on_navigation_callback)
    : web_contents(std::move(wc)),
      is_open(open),
      last_active_time_ticks(base::TimeTicks::Now()),
      open_time_ticks(base::TimeTicks::Now()),
      web_contents_observer(
          web_contents ? std::make_unique<SidePanelWebContentsObserver>(
                             web_contents.get(),
                             std::move(on_fcp_callback),
                             std::move(on_navigation_callback))
                       : nullptr) {}
ContextualTasksSidePanelCoordinator::WebContentsCacheItem::
    ~WebContentsCacheItem() = default;

DEFINE_USER_DATA(ContextualTasksPanelController);

ContextualTasksSidePanelCoordinator::ContextualTasksSidePanelCoordinator(
    BrowserWindowInterface* browser_window,
    ActiveTaskContextProvider* active_task_context_provider,
    EntryPointEligibilityManager* eligibility_manager)
    : ContextualTasksSidePanelCoordinator(
          browser_window,
          ContextualTasksPanelHost::Create(browser_window),
          active_task_context_provider,
          eligibility_manager) {}

ContextualTasksSidePanelCoordinator::ContextualTasksSidePanelCoordinator(
    BrowserWindowInterface* browser_window,
    std::unique_ptr<ContextualTasksPanelHost> contextual_tasks_panel_host,
    ActiveTaskContextProvider* active_task_context_provider,
    EntryPointEligibilityManager* eligibility_manager)
    : browser_window_(browser_window),
      permission_controller_(
          std::make_unique<ContextualTasksPermissionController>(
              browser_window)),
      contextual_tasks_panel_host_(std::move(contextual_tasks_panel_host)),
      contextual_tasks_service_(ContextualTasksServiceFactory::GetForProfile(
          browser_window->GetProfile())),
      contextual_search_service_(ContextualSearchServiceFactory::GetForProfile(
          browser_window->GetProfile())),
      pref_service_(browser_window->GetProfile()->GetPrefs()),
      active_task_context_provider_(active_task_context_provider),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
#if !BUILDFLAG(IS_ANDROID)
  onboarding_controller_ =
      std::make_unique<ContextualTasksOnboardingTooltipController>(
          browser_window, browser_window->GetProfile());
#endif
  active_task_context_provider_->SetContextualTasksPanelController(this);
  TabListInterface::From(browser_window_)->AddTabListInterfaceObserver(this);
  contextual_tasks_panel_host_->AddObserver(this);

#if !BUILDFLAG(IS_ANDROID)
  if (Profile* profile = browser_window_->GetProfile()) {
    is_dark_mode_ = contextual_tasks::ShouldUseDarkMode(profile);
    if (ThemeService* theme_service =
            ThemeServiceFactory::GetForProfile(profile)) {
      theme_service_observation_.Observe(theme_service);
    }
  }
  if (ui::NativeTheme* native_theme =
          ui::NativeTheme::GetInstanceForNativeUi()) {
    native_theme_observation_.Observe(native_theme);
  }
#endif

  if (eligibility_manager) {
    eligibility_change_subscription_ =
        eligibility_manager->RegisterOnEntryPointEligibilityChanged(
            base::BindRepeating(
                &ContextualTasksSidePanelCoordinator::OnEligibilityChange,
                weak_ptr_factory_.GetWeakPtr()));
  }
}

ContextualTasksSidePanelCoordinator::~ContextualTasksSidePanelCoordinator() {
#if !BUILDFLAG(IS_ANDROID)
  theme_service_observation_.Reset();
  native_theme_observation_.Reset();
#endif
  for (auto& observer : observers_) {
    observer.OnControllerDestroyed();
  }
  active_task_context_provider_->SetContextualTasksPanelController(nullptr);
  TabListInterface::From(browser_window_)->RemoveTabListInterfaceObserver(this);
  contextual_tasks_panel_host_->RemoveObserver(this);

  RecordSessionEndMetrics();
}

ContextualTasksUiService* ContextualTasksSidePanelCoordinator::GetUiService()
    const {
  return ContextualTasksUiServiceFactory::GetForBrowserContextIfExists(
      browser_window_->GetProfile());
}

ContextualTasksPermissionController*
ContextualTasksSidePanelCoordinator::permission_controller() {
  return permission_controller_.get();
}

// static
ContextualTasksPanelController* ContextualTasksPanelController::From(
    BrowserWindowInterface* window) {
  return window ? Get(window->GetUnownedUserDataHost()) : nullptr;
}

// static
bool ContextualTasksPanelController::IsWebContentsInPanel(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }
  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_contents);
  if (!browser) {
    return false;
  }
  auto* controller = From(browser);
  if (!controller) {
    return false;
  }
  for (auto* wc : controller->GetPanelWebContentsList()) {
    if (wc == web_contents) {
      return true;
    }
  }
  return false;
}

void ContextualTasksSidePanelCoordinator::RecordTimeToFirstContentfulPaint(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  WebContentsCacheItem* cache_item =
      GetWebContentsCacheItemForWebContents(web_contents);
  if (cache_item && !cache_item->first_contentful_paint_recorded &&
      !cache_item->open_time_ticks.is_null()) {
    base::TimeDelta elapsed =
        base::TimeTicks::Now() - cache_item->open_time_ticks;
    base::UmaHistogramMediumTimes(
        "ContextualTasks.SidePanel.TimeToFirstContentfulPaint", elapsed);
    cache_item->first_contentful_paint_recorded = true;
  }
}

void ContextualTasksSidePanelCoordinator::RecordTimeToHandshakeComplete(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  WebContentsCacheItem* cache_item =
      GetWebContentsCacheItemForWebContents(web_contents);
  if (cache_item && !cache_item->time_to_handshake_complete_recorded &&
      !cache_item->open_time_ticks.is_null()) {
    base::TimeDelta elapsed =
        base::TimeTicks::Now() - cache_item->open_time_ticks;
    base::UmaHistogramMediumTimes(
        "ContextualTasks.SidePanel.TimeToHandshakeComplete", elapsed);
    cache_item->time_to_handshake_complete_recorded = true;
  }
}

void ContextualTasksSidePanelCoordinator::Show(
    bool transition_from_tab,
    omnibox::ChromeAimEntryPoint entry_point,
    bool use_no_animation,
    std::optional<base::TimeTicks> open_time_ticks,
    std::optional<EntrySource> entry_source_override) {
  // Increment the impression count and attempt to show the HaTS survey.
  int impression_count =
      pref_service_->GetInteger(prefs::kContextualTasksNextPanelOpenCount);
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopNextPanel) &&
      impression_count >= 1) {
    HatsService* hats_service =
        HatsServiceFactory::GetForProfile(browser_window_->GetProfile(),
                                          /* create_if_necessary = */ true);
    if (hats_service) {
      std::string experiment_id = GetContextualTasksArmShortName();

      hats_service->LaunchDelayedSurvey(
          kHatsSurveyTriggerNextPanel, 90000, {},
          {{"Experiment ID", experiment_id},
           {"ContextualTasksExpandButtonOptions",
            GetExpandButtonOption() ==
                    ExpandButtonOption::kSidePanelExpandButton
                ? "side-panel-expand-button"
                : "toolbar-close-button"},
           {"ContextualTasksEnableLensInContextualTasks",
            GetEnableLensInContextualTasks() ? "true" : "false"},
           {"ContextualTasksTabAutoSuggestionChipEnabled",
            GetIsTabAutoSuggestionChipEnabled() ? "true" : "false"}});
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  pref_service_->SetInteger(prefs::kContextualTasksNextPanelOpenCount,
                            impression_count + 1);

  tabs::TabInterface* active_tab_interface =
      TabListInterface::From(browser_window_)->GetActiveTab();
  CHECK(active_tab_interface);
  std::optional<ContextualTask> existing_task = GetCurrentTask();
  const bool is_new_ui_service_task =
      existing_task.has_value() &&
      !task_id_to_web_contents_cache_.contains(existing_task->GetTaskId()) &&
      GetUiService() &&
      (GetUiService()
           ->GetCreationUrlForTask(existing_task->GetTaskId())
           .has_value() ||
       GetUiService()->IsTaskWaitingForUrl(existing_task->GetTaskId()) ||
       GetUiService()->IsPendingErrorPage(existing_task->GetTaskId()));
  const bool is_continuing_existing_thread =
      contextual_tasks::IsContextualTasksPinningV2Enabled() &&
      entry_point == omnibox::ChromeAimEntryPoint::
                         DESKTOP_CHROME_COBROWSE_PINNED_TOOLBAR_BUTTON &&
      !is_opening_in_zero_state_ && existing_task.has_value() &&
      !is_new_ui_service_task;

  ContextualTasksPanelController::EntrySource entry_source;
  if (entry_source_override.has_value()) {
    entry_source = *entry_source_override;
  } else if (entry_point ==
             omnibox::ChromeAimEntryPoint::
                 DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT) {
    entry_source = ContextualTasksPanelController::EntrySource::kLensOverlay;
  } else if (entry_point ==
             omnibox::ChromeAimEntryPoint::DESKTOP_CHROME_COBROWSE_AIO_LINK) {
    entry_source = ContextualTasksPanelController::EntrySource::kAioToCobr;
  } else if (is_continuing_existing_thread) {
    entry_source =
        ContextualTasksPanelController::EntrySource::kPermanentToolbarButton;
  } else if (entry_point == omnibox::ChromeAimEntryPoint::
                                DESKTOP_CHROME_PAGE_CONTEXT_MENU_ASK_GOOGLE ||
             entry_point == omnibox::ChromeAimEntryPoint::
                                DESKTOP_CHROME_APP_MENU_ASK_GOOGLE ||
             entry_point ==
                 omnibox::ChromeAimEntryPoint::
                     DESKTOP_CHROME_CHROMNIENT_HOMEWORK_ACTION_CHIP) {
    entry_source = ContextualTasksPanelController::EntrySource::kAskGoogle;
  } else if (entry_point == omnibox::ChromeAimEntryPoint::
                                DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON) {
    entry_source =
        ContextualTasksPanelController::EntrySource::kEphemeralToolbarButton;
  } else if (transition_from_tab) {
    entry_source =
        ContextualTasksPanelController::EntrySource::kAiModeLinkClick;
  } else {
    entry_source = ContextualTasksPanelController::EntrySource::kOther;
  }

  CloseLensSessionIfActive(active_tab_interface, entry_point, entry_source);
  if (!existing_task) {
    // If no task is found, create a new task and associate it with the active
    // tab.
    ContextualTask task = contextual_tasks_service_->CreateTask();
    if (auto* ui_service = GetUiService()) {
      ui_service->AssociateWebContentsToTask(
          active_tab_interface->GetContents(), task.GetTaskId());
    }
  }

  MaybeCreateCachedWebContents(
      is_continuing_existing_thread
          ? omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT
          : entry_point);
  if (std::optional<ContextualTask> task = GetCurrentTask()) {
    auto it = task_id_to_web_contents_cache_.find(task->GetTaskId());
    if (it != task_id_to_web_contents_cache_.end()) {
      it->second->entry_source = entry_source;
      it->second->entry_point = entry_point;
      it->second->last_active_time_ticks = base::TimeTicks::Now();
      it->second->open_time_ticks =
          open_time_ticks.value_or(base::TimeTicks::Now());
      it->second->first_contentful_paint_recorded = false;
      it->second->time_to_handshake_complete_recorded = false;
    }
  }
  UpdateWebContentsForActiveTab();
#if !BUILDFLAG(IS_ANDROID)
  onboarding_controller_->SetEntryPoint(entry_point);
#endif

  ContextualTasksPanelHost::AnimationStyle animation_style =
      ContextualTasksPanelHost::AnimationStyle::kStandard;
  if (use_no_animation) {
    animation_style = ContextualTasksPanelHost::AnimationStyle::kNoAnimation;
  } else if (transition_from_tab) {
    animation_style =
        ContextualTasksPanelHost::AnimationStyle::kTransitionFromTab;
  }

  contextual_tasks_panel_host_->Show(animation_style);

  UpdateOpenState(/*is_open=*/true);
  UpdateContextualTaskUI();
  if (content::WebContents* active_web_contents = GetActiveWebContents()) {
    if (auto* web_ui_interface = GetWebUiInterface(active_web_contents)) {
      web_ui_interface->FocusComposebox();
    }
  }
  ObserveWebContentsOnActiveTab();
  NotifyActiveTaskContextProvider();

#if !BUILDFLAG(IS_ANDROID)
  // Hide the GLIC nudge when the panel is opened.
  if (auto* glic_nudge_controller =
          glic::GlicNudgeController::From(browser_window_)) {
    glic_nudge_controller->UpdateNudgeLabel(
        active_tab_interface->GetContents(), "", std::nullopt,
        glic::GlicNudgeActivity::kNudgeIgnoredOpenedContextualTasksSidePanel,
        base::DoNothing());
  }

  // Feature engagement session notice and IPH trigger checks.
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(
          browser_window_->GetProfile());
  if (tracker) {
    tracker->NotifyEvent("contextual_tasks_session_started");

    // Increment post-onboarding session count if it has been explicitly
    // initialized (>= 0).
    int post_onboarding_sessions =
        pref_service_->GetInteger(kContextualTasksSessionCountPostOnboarding);
    if (post_onboarding_sessions >= 0) {
      post_onboarding_sessions++;
      pref_service_->SetInteger(kContextualTasksSessionCountPostOnboarding,
                                post_onboarding_sessions);
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ContextualTasksSidePanelCoordinator::Close() {
  if (content::WebContents* active_web_contents = GetActiveWebContents()) {
    if (auto* permission_request_manager =
            permissions::PermissionRequestManager::FromWebContents(
                active_web_contents);
        permission_request_manager &&
        permission_request_manager->IsRequestInProgress()) {
      permission_request_manager->Ignore(/*prompt_options=*/std::monostate());
    }
    if (WebContentsCacheItem* cache_item =
            GetWebContentsCacheItemForWebContents(active_web_contents)) {
      // Save the entry source so that it can be logged in
      // OnSurfaceStateChanged.
      closing_entry_source_ = cache_item->entry_source;
    }
  }
  UpdateOpenState(/*is_open=*/false);
  contextual_tasks_panel_host_->Close(
      ContextualTasksPanelHost::AnimationStyle::kStandard);
  MaybeRemoveImplicitTabContext();
  Observe(nullptr);

#if !BUILDFLAG(IS_ANDROID)
  onboarding_controller_->CloseBubble();
#endif

  if (active_task_context_provider_) {
    active_task_context_provider_->ClearAllLocalTabUnderlines();
  }

  NotifyActiveTaskContextProvider();

  // When scoped to the tab, the session continues while another tab of the
  // task still shows the panel.
  const bool scoped_to_tab = ShouldScopeSidePanelStateToTab();
  std::optional<ContextualTask> current_task =
      scoped_to_tab ? GetCurrentTask() : std::nullopt;
  if (!current_task ||
      !IsPanelOpenOnOtherTabOfTask(current_task->GetTaskId())) {
    RecordSessionEndMetrics();
  }

  if (scoped_to_tab) {
    // Only disassociate the active tab so that other tabs associated with the
    // current task keep their open panel and thread.
    DisassociateActiveTabOnClose();
  } else {
    // Disassociate all tabs associated with the current task on close so that
    // re-opening the panel always starts a fresh zero-state thread regardless
    // of which associated tab is active.
    DisassociateAllTabsFromCurrentTask();
  }
  CleanUpUnusedWebContents();

  // When scoped to the tab, `CleanUpUnusedWebContents()` already removed the
  // WebContents if no other tab is associated with its task.
  if (!scoped_to_tab && !ShouldPreserveTaskOnClose()) {
    if (content::WebContents* active_web_contents = GetActiveWebContents()) {
      MaybeDetachWebContents(active_web_contents);
      for (auto it = task_id_to_web_contents_cache_.begin();
           it != task_id_to_web_contents_cache_.end(); ++it) {
        if (it->second->web_contents.get() == active_web_contents) {
          task_id_to_web_contents_cache_.erase(it);
          break;
        }
      }
    }
  }
}

void ContextualTasksSidePanelCoordinator::OpenInZeroState() {
  // Disassociate only the active tab from its current task so that Show()
  // creates a fresh zero-state task for this tab, without affecting any
  // background tabs that may still be associated with the previous task.
  TabListInterface* tab_list = TabListInterface::From(browser_window_);
  if (tab_list) {
    tabs::TabInterface* active_tab_interface = tab_list->GetActiveTab();
    if (active_tab_interface && active_tab_interface->GetContents()) {
      DisassociateTabFromTask(active_tab_interface->GetContents());
    }
  }

  if (active_task_context_provider_) {
    active_task_context_provider_->ClearAllLocalTabUnderlines();
  }

  MaybeRemoveImplicitTabContext();
  if (content::WebContents* active_contents = GetActiveWebContents()) {
    MaybeDetachWebContents(active_contents);
  }

  CleanUpUnusedWebContents();
  bool is_pinned =
      contextual_tasks::IsContextualTasksPinButtonInToolbarEnabled() &&
      contextual_tasks::GetEffectivePinState(browser_window_->GetProfile());

  base::AutoReset<bool> opening_in_zero_state(&is_opening_in_zero_state_, true);
  Show(/*transition_from_tab=*/false,
       is_pinned ? omnibox::ChromeAimEntryPoint::
                       DESKTOP_CHROME_COBROWSE_PINNED_TOOLBAR_BUTTON
                 : omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
       /*use_no_animation=*/false, /*open_time_ticks=*/std::nullopt,
       is_pinned ? std::nullopt
                 : std::make_optional(
                       ContextualTasksPanelController::EntrySource::
                           kEphemeralToolbarButton));
}

bool ContextualTasksSidePanelCoordinator::MoveActiveTabToNewThreadIfShared() {
  TabListInterface* tab_list = TabListInterface::From(browser_window_);
  ContextualTasksUiService* ui_service = GetUiService();
  if (!tab_list || !contextual_tasks_service_ || !ui_service ||
      !ShouldBeOpen()) {
    return false;
  }
  tabs::TabInterface* active_tab = tab_list->GetActiveTab();
  std::optional<ContextualTask> task = GetCurrentTask();
  if (!active_tab || !active_tab->GetContents() || !task) {
    return false;
  }

  // A new thread can replace the current thread in place if no other tab in
  // this window shares the task.
  const SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  const std::set<SessionID> tab_ids = GetAllTabIdsInTabList(tab_list);
  const bool is_shared = std::ranges::any_of(
      contextual_tasks_service_->GetTabsAssociatedWithTask(task->GetTaskId()),
      [&](SessionID tab_id) {
        return tab_id != active_tab_id && tab_ids.contains(tab_id);
      });
  if (!is_shared) {
    return false;
  }

  const EntrySource entry_source = GetActiveEntrySource();
  const omnibox::ChromeAimEntryPoint entry_point =
      ui_service->GetInitialEntryPointForTask(task->GetTaskId());

  // Move only the active tab to a new task. Other tabs keep the current panel
  // `WebContents` and its thread.
  DisassociateTabFromTask(active_tab->GetContents());
  ContextualTask new_task = contextual_tasks_service_->CreateTask();
  ui_service->AssociateWebContentsToTask(active_tab->GetContents(),
                                         new_task.GetTaskId());
  MaybeCreateCachedWebContents(entry_point);
  auto it = task_id_to_web_contents_cache_.find(new_task.GetTaskId());
  if (it != task_id_to_web_contents_cache_.end()) {
    it->second->entry_source = entry_source;
    // The panel was already open, so don't record open latency or session
    // duration metrics for the new thread's `WebContents`.
    it->second->open_time_ticks = base::TimeTicks();
  }
  UpdateWebContentsForActiveTab();
  UpdateContextualTaskUI();
  NotifyActiveTaskContextProvider();
  return true;
}

std::map<std::string, std::string>
ContextualTasksSidePanelCoordinator::GetTabRestoreData(
    tabs::TabInterface* tab) const {
  std::map<std::string, std::string> data;
  if (!ShouldRestoreSidePanelOnTabRestore() || !contextual_tasks_service_ ||
      !tab || !tab->GetContents() ||
      tabs_with_closed_panel_.contains(tab->GetHandle())) {
    return data;
  }
  std::optional<ContextualTask> task =
      contextual_tasks_service_->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(tab->GetContents()));
  if (!task) {
    return data;
  }
  const base::Uuid& task_id = task->GetTaskId();
  auto it = task_id_to_web_contents_cache_.find(task_id);
  const bool has_panel_contents = it != task_id_to_web_contents_cache_.end();
  // A reopened tab's panel isn't recreated until the tab becomes active, so it
  // is still pending if the tab is closed again before that.
  const bool is_panel_open =
      has_panel_contents ? it->second->is_open
                         : tasks_pending_panel_rebuild_.contains(task_id);
  if (!is_panel_open) {
    return data;
  }
  data[ContextualTasksTabRestoreData::kTaskIdKey] = task_id.AsLowercaseString();

  // Save the URL of the page that the panel shows, like an AI thread or a Lens
  // search, to load it again in the reopened tab's panel. The legacy panel
  // embeds the page. A panel that isn't recreated yet, or hasn't loaded its
  // page yet, uses the URL that it's recreated with.
  ContextualTasksUiService* ui_service = GetUiService();
  if (!ui_service) {
    return data;
  }
  GURL thread_url;
  if (!has_panel_contents) {
    thread_url = ui_service->GetCreationUrlForTask(task_id).value_or(GURL());
  } else {
    if (IsContextualTasksSidePanelRearchitectureEnabled()) {
      thread_url = it->second->web_contents->GetLastCommittedURL();
    } else if (ContextualTasksUIInterface* web_ui_interface =
                   GetWebUiInterface(it->second->web_contents.get())) {
      thread_url = web_ui_interface->GetInnerFrameUrl();
    }
    if (!ui_service->IsRestorableSidePanelUrl(thread_url)) {
      thread_url = it->second->restored_url;
    }
  }
  if (ui_service->IsRestorableSidePanelUrl(thread_url)) {
    data[ContextualTasksTabRestoreData::kThreadUrlKey] = thread_url.spec();
  }
  return data;
}

void ContextualTasksSidePanelCoordinator::ShareTabsWithActiveOrNewThread(
    const std::vector<tabs::TabHandle>& tab_handles) {
  if (!browser_window_ || !CanShareTabContext(browser_window_->GetProfile())) {
    return;
  }

  std::vector<tabs::TabHandle> valid_tab_handles =
      FilterValidTabHandlesForSharing(tab_handles);
  if (valid_tab_handles.empty()) {
    return;
  }

  // If the side panel is already open on the active tab, attach the selected
  // tabs directly to the open task so sharing multiple tabs from the tab strip
  // stages them together without resetting previously staged tabs.
  if (IsPanelOpenForContextualTask()) {
    if (std::optional<ContextualTask> current_task = GetCurrentTask()) {
      AttachTabsToTask(current_task->GetTaskId(), valid_tab_handles);
      return;
    }
  }

  std::vector<tabs::TabHandle> tabs_to_attach =
      BuildTabsToAttachWithActiveTab(browser_window_, valid_tab_handles);

  OpenInZeroState();

  std::optional<ContextualTask> task = GetCurrentTask();
  if (!task.has_value()) {
    return;
  }
  AttachTabsToTask(task->GetTaskId(), tabs_to_attach);
  if (content::WebContents* panel_contents = GetActiveWebContents()) {
    ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
        panel_contents)
        ->PreservePreStagedContext();
  }
}

void ContextualTasksSidePanelCoordinator::ShareTabsWithExistingThread(
    const base::Uuid& task_id,
    const std::vector<tabs::TabHandle>& tab_handles,
    omnibox::ChromeAimEntryPoint entry_point) {
  if (!browser_window_ || !CanShareTabContext(browser_window_->GetProfile()) ||
      !task_id.is_valid()) {
    return;
  }
  if (const auto cache_it = task_id_to_web_contents_cache_.find(task_id);
      cache_it == task_id_to_web_contents_cache_.end() || !cache_it->second) {
    return;
  }

  std::vector<tabs::TabHandle> valid_tab_handles =
      FilterValidTabHandlesForSharing(tab_handles);
  if (valid_tab_handles.empty()) {
    return;
  }

  std::vector<tabs::TabHandle> tabs_to_attach =
      BuildTabsToAttachWithActiveTab(browser_window_, valid_tab_handles);

  std::optional<ContextualTask> current_task = GetCurrentTask();
  const bool switching_threads =
      !current_task.has_value() || current_task->GetTaskId() != task_id;
  if (switching_threads) {
    if (active_task_context_provider_) {
      active_task_context_provider_->ClearAllLocalTabUnderlines();
    }
    if (TabListInterface* tab_list = TabListInterface::From(browser_window_)) {
      if (tabs::TabInterface* active_tab = tab_list->GetActiveTab();
          active_tab && active_tab->GetContents()) {
        DisassociateTabFromTask(active_tab->GetContents());
        if (ContextualTasksUiService* ui_service = GetUiService()) {
          ui_service->AssociateWebContentsToTask(active_tab->GetContents(),
                                                 task_id);
        }
      }
    }
  }

  Show(/*transition_from_tab=*/false, entry_point);

  AttachTabsToTask(task_id, tabs_to_attach);
}

std::vector<ContextualTasksPanelController::RecentThreadInfo>
ContextualTasksSidePanelCoordinator::GetRecentThreads() const {
  constexpr size_t kMaxRecentThreads = 5;
  struct CandidateThread {
    base::Uuid task_id;
    std::u16string title;
    bool is_active;
    base::TimeTicks last_active_time_ticks;
  };
  std::vector<CandidateThread> candidates;
  content::WebContents* active_web_contents =
      IsPanelOpenForContextualTask() ? GetActiveWebContents() : nullptr;
  ContextualTasksUiService* ui_service = GetUiService();

  for (const auto& [task_id, cache_item] : task_id_to_web_contents_cache_) {
    if (!cache_item || !cache_item->web_contents) {
      continue;
    }
    content::WebContents* wc = cache_item->web_contents.get();
    ContextualTasksUIInterface* web_ui_interface = GetWebUiInterface(wc);
    GURL url = web_ui_interface ? web_ui_interface->GetInnerFrameUrl() : GURL();
    if (url.is_empty()) {
      url = wc->GetLastCommittedURL();
    }
    if (url.is_empty() || url.IsAboutBlank() || !ui_service ||
        !ui_service->IsAiUrl(url) ||
        ContextualTasksUI::IsZeroState(url, ui_service)) {
      continue;
    }

    std::u16string title;
    std::string query_value;
    if (net::GetValueForKeyInQuery(url, "q", &query_value) &&
        !query_value.empty()) {
      title = base::UTF8ToUTF16(query_value);
    }
    if (title.empty() && contextual_tasks_service_) {
      for (SessionID tab_id :
           contextual_tasks_service_->GetTabsAssociatedWithTask(task_id)) {
        if (std::optional<ContextualTask> task =
                contextual_tasks_service_->GetContextualTaskForTab(tab_id)) {
          if (task->GetThread().has_value() &&
              !task->GetThread()->title.empty()) {
            title = base::UTF8ToUTF16(task->GetThread()->title);
            break;
          }
          if (!task->GetTitle().empty()) {
            title = base::UTF8ToUTF16(task->GetTitle());
            break;
          }
        }
      }
    }
    if (title.empty()) {
      continue;
    }

    candidates.push_back(CandidateThread{
        .task_id = task_id,
        .title = std::move(title),
        .is_active = (wc == active_web_contents),
        .last_active_time_ticks = cache_item->last_active_time_ticks,
    });
  }

  std::ranges::sort(
      candidates, [](const CandidateThread& a, const CandidateThread& b) {
        if (a.is_active != b.is_active) {
          return a.is_active > b.is_active;
        }
        if (a.last_active_time_ticks != b.last_active_time_ticks) {
          return a.last_active_time_ticks > b.last_active_time_ticks;
        }
        return a.task_id < b.task_id;
      });

  std::vector<RecentThreadInfo> result;
  const size_t count = std::min(candidates.size(), kMaxRecentThreads);
  result.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    result.push_back(RecentThreadInfo{
        .task_id = candidates[i].task_id,
        .title = std::move(candidates[i].title),
    });
  }
  return result;
}

void ContextualTasksSidePanelCoordinator::UnshareTabs(
    const std::vector<tabs::TabHandle>& tab_handles) {
  if (tab_handles.empty()) {
    return;
  }

  // 1. Collect all cached side-panel thread WebContents and include the active
  // panel WebContents if it is not already in the cache list.
  std::vector<content::WebContents*> target_web_contents =
      GetPanelWebContentsList();
  if (content::WebContents* active_contents = GetActiveWebContents();
      active_contents &&
      !std::ranges::contains(target_web_contents, active_contents)) {
    target_web_contents.push_back(active_contents);
  }

  // 2. Also include any open browser tabs hosting a full-tab AI Mode thread.
  if (browser_window_) {
    if (auto* tab_list = TabListInterface::From(browser_window_)) {
      for (tabs::TabInterface* tab : tab_list->GetAllTabs()) {
        content::WebContents* tab_contents = tab ? tab->GetContents() : nullptr;
        auto* user_data =
            tab_contents ? ContextualTasksWebContentsUserData::FromWebContents(
                               tab_contents)
                         : nullptr;
        if (user_data && user_data->GetEffectiveTaskId().has_value() &&
            !std::ranges::contains(target_web_contents, tab_contents)) {
          target_web_contents.push_back(tab_contents);
        }
      }
    }
  }

  // 3. For each unshared tab, remove its local underline and delete its tab
  // context from every collected thread WebContents.
  for (tabs::TabHandle tab_handle : tab_handles) {
    if (tab_handle == tabs::TabHandle::Null()) {
      continue;
    }
    if (active_task_context_provider_) {
      active_task_context_provider_->RemoveLocalTabUnderline(tab_handle);
    }
    for (content::WebContents* web_contents : target_web_contents) {
      if (!web_contents) {
        continue;
      }
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents)
          ->DeleteTabContext(tab_handle.raw_value());
    }
  }

  // 4. Refresh the active task context provider to update tab strip underlines.
  NotifyActiveTaskContextProvider();
}

bool ContextualTasksSidePanelCoordinator::IsPanelOpenForContextualTask() const {
  return contextual_tasks_panel_host_->IsPanelOpenForContextualTask();
}

bool ContextualTasksSidePanelCoordinator::IsShowingResults() const {
  return IsPanelOpenForContextualTask() && HasResults();
}

bool ContextualTasksSidePanelCoordinator::HasResults() const {
  return IsShowingLensResults() || IsShowingAimResults();
}

ContextualTasksPanelController::EntrySource
ContextualTasksSidePanelCoordinator::GetActiveEntrySource() const {
  if (content::WebContents* active_web_contents = GetActiveWebContents()) {
    for (const auto& it : task_id_to_web_contents_cache_) {
      if (it.second->web_contents.get() == active_web_contents) {
        return it.second->entry_source;
      }
    }
  }
  return ContextualTasksPanelController::EntrySource::kOther;
}

bool ContextualTasksSidePanelCoordinator::IsOnboardingTooltipVisible() const {
#if !BUILDFLAG(IS_ANDROID)
  return onboarding_controller_->IsBubbleShowing();
#else
  return false;
#endif
}

void ContextualTasksSidePanelCoordinator::TransferWebContentsFromTab(
    const base::Uuid& task_id,
    std::unique_ptr<content::WebContents> web_contents) {
  // Clear the back stack whenever a WebContents is moved to the panel. This
  // helps prevent any unintended back/forward navigation.
  if (web_contents->GetController().CanPruneAllButLastCommitted()) {
    web_contents->GetController().PruneAllButLastCommitted();
  }

  // If dev tools were open, close them. This prevents issues where the tab
  // backing the WebContents disappears and confuses the dev tools.
  // TODO(489511091): Ideally an event is triggered that allows dev tools to
  //                  react by popping the tools out into their own window.
  DevToolsUIBindings::Delegate* dev_tools_delegate =
      DevToolsWindow::GetInstanceForInspectedWebContents(web_contents.get());
  if (dev_tools_delegate) {
    dev_tools_delegate->CloseWindow();
  }

  SetBrowserWindowInterface(web_contents.get(), browser_window_);

  // Unlike `CreateWebContents()`, no `PermissionRequestManager` needs to be
  // created here: this `WebContents` comes from a tab, so it already has one
  // attached via `TabHelpers::AttachTabHelpers()`.

  // Register the transferred `WebContents` with the side panel's single
  // location bar.
  if (permission_controller_) {
    permission_controller_->RegisterWebContents(web_contents.get());
  }
  if (IsContextualTasksSidePanelRearchitectureEnabled()) {
    ContextualTasksPanelThreadObserver::CreateForWebContents(
        web_contents.get());
  }
  auto it = task_id_to_web_contents_cache_.find(task_id);
  // WebContents transferred from a tab has already completed its initial paint
  // as a tab. Do not attach an FCP observer and mark FCP as already recorded
  // to avoid skewing side panel opening latency metrics.
  if (it == task_id_to_web_contents_cache_.end()) {
    auto cache_item = std::make_unique<WebContentsCacheItem>(
        std::move(web_contents), /*is_open=*/true);
    cache_item->first_contentful_paint_recorded = true;
    task_id_to_web_contents_cache_[task_id] = std::move(cache_item);
  } else {
    MaybeDetachWebContents(it->second->web_contents.get());
    it->second->web_contents = std::move(web_contents);
    it->second->web_contents_observer =
        std::make_unique<SidePanelWebContentsObserver>(
            it->second->web_contents.get(), base::NullCallback(),
            base::NullCallback());
    it->second->first_contentful_paint_recorded = true;
  }
  UpdateOpenState(/*is_open=*/true);
  UpdateWebContentsForActiveTab();
  UpdateContextualTaskUI();
}

void ContextualTasksSidePanelCoordinator::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Check if the navigation is a back/forward navigation with the resulting URL
  // being a contextual tasks URL. If the panel is open, this indicates that the
  // user is navigating back to the full tab contextual tasks page.
  if (navigation_handle->GetNavigationEntry() &&
      (navigation_handle->GetNavigationEntry()->GetTransitionType() &
       ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK) &&
      !navigation_handle->IsRendererInitiated() &&
      IsPanelOpenForContextualTask()) {
    if (auto* ui_service = GetUiService();
        ui_service->IsContextualTasksUrl(navigation_handle->GetURL())) {
      RecordUserActionAndHistogram(
          "ContextualTasks.BackButton.UserAction."
          "NavigatedFromSidePanelToFullTab");
    }
  }
}

void ContextualTasksSidePanelCoordinator::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  if (navigation_handle->HasUserGesture() &&
      ui::PageTransitionTypeIncludingQualifiersIs(
          navigation_handle->GetPageTransition(), ui::PAGE_TRANSITION_LINK)) {
    RecordUserActionAndHistogram(
        "ContextualTasks.ActiveTab.UserAction.LinkClicked");
  }
}

void ContextualTasksSidePanelCoordinator::PrimaryPageChanged(
    content::Page& page) {
  // Hide panel if contextual tasks pages is loaded on tab.
  GURL url = page.GetMainDocument().GetLastCommittedURL();
  if (auto* ui_service = GetUiService();
      ui_service->IsContextualTasksUrl(url) &&
      !suppress_hide_on_contextual_tasks_url_for_testing_) {
    UpdateOpenState(/*is_open=*/false);
    Hide();
  }

  UpdateContextualTaskUI();
}

void ContextualTasksSidePanelCoordinator::TitleWasSet(
    content::NavigationEntry* entry) {
  UpdateContextualTaskUI();
}

content::WebContents*
ContextualTasksSidePanelCoordinator::GetActiveWebContents() const {
  return contextual_tasks_panel_host_->GetWebContents();
}

content::WebContents*
ContextualTasksSidePanelCoordinator::GetToolbarWebContents() const {
  return contextual_tasks_panel_host_->GetToolbarWebContents();
}

std::unique_ptr<content::WebContents>
ContextualTasksSidePanelCoordinator::DetachWebContentsForTask(
    const base::Uuid& task_id) {
  auto it = task_id_to_web_contents_cache_.find(task_id);
  if (it != task_id_to_web_contents_cache_.end()) {
    std::unique_ptr<content::WebContents> web_contents =
        std::move(it->second->web_contents);
    MaybeDetachWebContents(web_contents.get());
    MaybeRemoveImplicitTabContext(web_contents.get());
    // The tab's `ContextualTasksTabNavigationObserver` takes over syncing its
    // task with its AI Mode thread.
    web_contents->RemoveUserData(
        ContextualTasksPanelThreadObserver::UserDataKey());
    // Reset the association with the BrowserWindow since the WebContents will
    // soon be associated with a tab.
    webui::SetBrowserWindowInterface(web_contents.get(),
                                     /*browser_window_interface=*/nullptr);
    if (permission_controller_) {
      permission_controller_->UnregisterWebContents(web_contents.get());
    }
#if BUILDFLAG(ENABLE_EXTENSIONS)
    // Set ViewType to kTabContents so `ChromeSpeechRecognitionManagerDelegate`
    // allows speech recognition in `CheckRenderFrameType()`.
    extensions::SetViewType(web_contents.get(),
                            extensions::mojom::ViewType::kTabContents);
#endif
    // Clear the WebContents delegate since `ContextualTasksWebView` will no
    // longer be a valid delegate when WebContents is moved to a tab.
    web_contents->SetDelegate(nullptr);
    task_id_to_web_contents_cache_.erase(it);
    return web_contents;
  }

  return nullptr;
}

void ContextualTasksSidePanelCoordinator::OnTaskChanged(
    content::WebContents* web_contents,
    base::Uuid new_task_id) {
  content::WebContents* target_web_contents = web_contents;
  // Post-rearchitecture, the panel's AI Mode `WebContents` itself may report
  // the change (see `ContextualTasksPanelThreadObserver`), and it may not be
  // the active one. Otherwise, update the active panel `WebContents`.
  if (IsContextualTasksSidePanelRearchitectureEnabled() &&
      (!web_contents || !GetWebContentsCacheItemForWebContents(web_contents))) {
    target_web_contents = GetActiveWebContents();
  }

  std::unique_ptr<WebContentsCacheItem> cache_item;
  std::optional<base::Uuid> old_task_id;
  // Find the web_contents from cache.
  for (auto it = task_id_to_web_contents_cache_.begin();
       it != task_id_to_web_contents_cache_.end(); ++it) {
    if (it->second->web_contents.get() == target_web_contents) {
      old_task_id = it->first;
      cache_item = std::move(it->second);
      task_id_to_web_contents_cache_.erase(it);
      break;
    }
  }

  if (cache_item) {
    // The new task takes over the WebContents, so it doesn't need a rebuild.
    tasks_pending_panel_rebuild_.erase(new_task_id);
    const EntrySource old_entry_source = cache_item->entry_source;
    const bool was_open = cache_item->is_open;

    // If found, update the found WebContents with new_task_id.
    // This will potentially kick out the existing WebContents with new_task_id
    // since only 1 WebContents per task is kept in the cache.
    auto it = task_id_to_web_contents_cache_.find(new_task_id);
    if (it != task_id_to_web_contents_cache_.end()) {
      MaybeDetachWebContents(it->second->web_contents.get());
    }
    task_id_to_web_contents_cache_[new_task_id] = std::move(cache_item);

    // When the thread change is scoped to the tab, tabs that are still
    // associated with the old task keep its thread. Recreate its WebContents
    // right away if one of those tabs shows the panel, so that the thread is
    // ready when the tab becomes active. Otherwise, recreate it when the panel
    // is reopened on one of them.
    if (ShouldScopeSidePanelStateToTab() && old_task_id &&
        *old_task_id != new_task_id && was_open &&
        IsTaskAssociatedWithTabInWindow(*old_task_id)) {
      tasks_pending_panel_rebuild_[*old_task_id] = old_entry_source;
      if (IsPanelOpenOnTabOfTask(*old_task_id)) {
        RebuildCachedWebContentsForTask(*old_task_id);
      }
    }
  }

  // Since the task has just changed, ensure that
  // ContextualSearchWebContentsHelper is updated with the task ID and session
  // handle. Note that it ContextualSearchWebContentsHelper can be from an
  // existing WebContents or a newly created one.
  UpdateContextualSearchWebContentsHelperForTask(
      contextual_search_service_, browser_window_, contextual_tasks_service_,
      this, target_web_contents, new_task_id);
}

void ContextualTasksSidePanelCoordinator::OnAiInteraction() {
  in_cobrowsing_session_ = true;
}

void ContextualTasksSidePanelCoordinator::SetPendingTaskForTab(
    tabs::TabInterface* tab,
    const base::Uuid& task_id) {
  if (tab && tab->GetContents()) {
    ContextualTasksWebContentsUserData::CreateForWebContents(
        tab->GetContents());
    auto* user_data =
        ContextualTasksWebContentsUserData::FromWebContents(tab->GetContents());
    if (user_data) {
      user_data->set_pending_task_id(task_id);
    }
  }
}

contextual_search::ContextualSearchSessionHandle*
ContextualTasksSidePanelCoordinator::
    GetContextualSearchSessionHandleForPanel() {
  auto* web_contents = GetActiveWebContents();
  if (!web_contents) {
    return nullptr;
  }
  if (auto* helper =
          ContextualSearchWebContentsHelper::FromWebContents(web_contents)) {
    if (helper->session_handle()) {
      return helper->session_handle();
    }
  }
  auto* web_ui_interface = GetWebUiInterface(web_contents);
  return web_ui_interface
             ? web_ui_interface->GetOrCreateContextualSessionHandle()
             : nullptr;
}

std::vector<content::WebContents*>
ContextualTasksSidePanelCoordinator::GetPanelWebContentsList() const {
  std::vector<content::WebContents*> result;
  for (const auto& [task_id, cache_item] : task_id_to_web_contents_cache_) {
    result.push_back(cache_item->web_contents.get());
  }
  return result;
}

ContextualTasksSidePanelCoordinator::WebContentsCacheItem*
ContextualTasksSidePanelCoordinator::GetWebContentsCacheItemForWebContents(
    content::WebContents* web_contents) {
  for (auto& it : task_id_to_web_contents_cache_) {
    if (it.second->web_contents.get() == web_contents) {
      return it.second.get();
    }
  }
  return nullptr;
}

void ContextualTasksSidePanelCoordinator::OnTabAdded(TabListInterface& tab_list,
                                                     tabs::TabInterface* tab,
                                                     int index) {
  MaybeRestorePanelForTab(tab);

  // Do not inherit task association from the opener tab if the side panel is
  // not open (e.g. when the panel was closed and only the ephemeral button
  // remains, or when viewing a full-tab Contextual Tasks page).
  if (!IsPanelOpenForContextualTask()) {
    return;
  }

  content::WebContents* content = tab->GetContents();

  // If the new tab is already associated with a task, do nothing.
  if (contextual_tasks_service_->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(content))) {
    return;
  }

  // If the new tab has an opener and it's associated to a task, associate
  // the new tab to the same task.
  tabs::TabInterface* opener = tab_list.GetOpenerForTab(tab->GetHandle());
  if (!opener) {
    return;
  }
  // Check if the new tab is opened through a link click
  content::NavigationController& controller = content->GetController();
  if (!controller.GetActiveEntry() ||
      !ui::PageTransitionCoreTypeIs(
          controller.GetActiveEntry()->GetTransitionType(),
          ui::PAGE_TRANSITION_LINK)) {
    return;
  }
  std::optional<ContextualTask> task =
      contextual_tasks_service_->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(opener->GetContents()));
  if (task) {
    if (auto* ui_service = GetUiService()) {
      ui_service->AssociateWebContentsToTask(content, task->GetTaskId());
    }
  }
}

void ContextualTasksSidePanelCoordinator::OnTabRemoved(
    TabListInterface& tab_list,
    tabs::TabInterface* tab,
    TabRemovedReason removed_reason) {
  // Do not disassociate the tab from the task if inserted into panel or another
  // tab strip.
  if (removed_reason != TabRemovedReason::kInsertedIntoSidePanel &&
      removed_reason != TabRemovedReason::kInsertedIntoOtherTabStrip) {
    // Check if the side panel was open for the tab that is being removed. Only
    // record the user action and histogram if so.
    content::WebContents* web_contents = tab->GetContents();
    if (web_contents) {
      SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
      std::optional<ContextualTask> task =
          contextual_tasks_service_
              ? contextual_tasks_service_->GetContextualTaskForTab(tab_id)
              : std::nullopt;
      if (task) {
        auto it = task_id_to_web_contents_cache_.find(task->GetTaskId());
        if (it != task_id_to_web_contents_cache_.end()) {
          WebContentsCacheItem* cache_item = it->second.get();
          if (cache_item && cache_item->is_open &&
              !tabs_with_closed_panel_.contains(tab->GetHandle())) {
            bool is_active_tab = false;
            content::WebContents* cached_web_contents =
                cache_item->web_contents.get();
            if (cached_web_contents &&
                cached_web_contents == GetActiveWebContents()) {
              is_active_tab = true;
            }
            ContextualTasksTabCloseState close_state =
                is_active_tab ? ContextualTasksTabCloseState::kActiveTab
                              : ContextualTasksTabCloseState::kBackgroundTab;
            base::UmaHistogramEnumeration(
                "ContextualTasks.Tab.ClosedWithSidePanelOpenState",
                close_state);
            if (is_active_tab) {
              base::RecordAction(
                  base::UserMetricsAction("ContextualTasks.Tab.UserAction."
                                          "ClosedActiveTabWithSidePanelOpen"));
            } else {
              base::RecordAction(base::UserMetricsAction(
                  "ContextualTasks.Tab.UserAction."
                  "ClosedBackgroundTabWithSidePanelOpen"));
            }
          }
        }
      }
    }
    DisassociateTabFromTask(tab->GetContents());
  }
  tabs_with_closed_panel_.erase(tab->GetHandle());
}

#if !BUILDFLAG(IS_ANDROID)
void ContextualTasksSidePanelCoordinator::OnThemeChanged() {
  MaybeRefreshWebContentsForDarkModeChange();
}

void ContextualTasksSidePanelCoordinator::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  MaybeRefreshWebContentsForDarkModeChange();
}
#endif

std::optional<ContextualTask>
ContextualTasksSidePanelCoordinator::GetCurrentTask() {
  tabs::TabInterface* active_tab_interface =
      TabListInterface::From(browser_window_)->GetActiveTab();
  if (!active_tab_interface) {
    return std::nullopt;
  }

  if (active_tab_interface->GetContents()) {
    auto* user_data = ContextualTasksWebContentsUserData::FromWebContents(
        active_tab_interface->GetContents());
    if (user_data && user_data->pending_task_id().has_value()) {
      base::Uuid task_id = *user_data->pending_task_id();
      SessionID session_id = sessions::SessionTabHelper::IdForTab(
          active_tab_interface->GetContents());
      if (session_id.is_valid()) {
        if (auto* ui_service = GetUiService()) {
          ui_service->AssociateWebContentsToTask(
              active_tab_interface->GetContents(), task_id);
          user_data->set_pending_task_id(std::nullopt);
        }
      }
      return ContextualTask(task_id, /*is_ephemeral=*/true);
    }
  }

  return contextual_tasks_service_->GetContextualTaskForTab(
      sessions::SessionTabHelper::IdForTab(
          active_tab_interface->GetContents()));
}

std::optional<ContextualTask>
ContextualTasksSidePanelCoordinator::GetCurrentTask() const {
  tabs::TabInterface* active_tab_interface =
      TabListInterface::From(browser_window_)->GetActiveTab();
  if (!active_tab_interface) {
    return std::nullopt;
  }

  if (active_tab_interface->GetContents()) {
    const auto* user_data = ContextualTasksWebContentsUserData::FromWebContents(
        active_tab_interface->GetContents());
    if (user_data && user_data->pending_task_id().has_value()) {
      return ContextualTask(*user_data->pending_task_id(),
                            /*is_ephemeral=*/true);
    }
  }

  return contextual_tasks_service_->GetContextualTaskForTab(
      sessions::SessionTabHelper::IdForTab(
          active_tab_interface->GetContents()));
}

void ContextualTasksSidePanelCoordinator::UpdatePanelVisibility() {
  bool should_be_open = ShouldBeOpen();

  // If state is open and the panel is closed, open the panel.
  if (should_be_open && !IsPanelOpenForContextualTask()) {
    Unhide();
    return;
  }

  // If state is closed and the panel is open, close the panel.
  if (!should_be_open && IsPanelOpenForContextualTask()) {
    Hide();
    return;
  }
}

void ContextualTasksSidePanelCoordinator::CleanUpUnusedWebContents() {
  std::set<SessionID> tab_ids =
      GetAllTabIdsInTabList(TabListInterface::From(browser_window_));
  for (auto it = task_id_to_web_contents_cache_.begin();
       it != task_id_to_web_contents_cache_.end();) {
    base::Uuid task_id = it->first;
    content::WebContents* web_contents = it->second->web_contents.get();

    bool associated_with_tab = false;
    for (auto tab_id :
         contextual_tasks_service_->GetTabsAssociatedWithTask(task_id)) {
      if (tab_ids.contains(tab_id)) {
        associated_with_tab = true;
        break;
      }
    }

    bool is_active = GetActiveWebContents() == web_contents;
    bool expired =
        base::TimeTicks::Now() - it->second->last_active_time_ticks >
        base::Minutes(ContextualTasksInactiveSidePanelKeepInCacheMinutes());

    // If the WebContents has no open tabs associated with it in the current
    // window, or is not active for long enough time, then remove it.
    if (!associated_with_tab || (!is_active && expired)) {
      if (it->second->entry_source ==
              ContextualTasksPanelController::EntrySource::kAioToCobr &&
          !it->second->open_time_ticks.is_null()) {
        base::TimeDelta duration =
            base::TimeTicks::Now() - it->second->open_time_ticks;
        base::UmaHistogramLongTimes(
            "ContextualTasks.Session.Duration.AioToCobr", duration);
        it->second->open_time_ticks = base::TimeTicks();
      } else if (it->second->entry_source ==
                     ContextualTasksPanelController::EntrySource::
                         kEphemeralToolbarButton &&
                 !it->second->open_time_ticks.is_null()) {
        base::TimeDelta duration =
            base::TimeTicks::Now() - it->second->open_time_ticks;
        base::UmaHistogramLongTimes(
            "ContextualTasks.Session.Duration.EphemeralToolbarButton",
            duration);
        it->second->open_time_ticks = base::TimeTicks();
      } else if (contextual_tasks::IsContextualTasksPinningV2Enabled() &&
                 it->second->entry_source ==
                     ContextualTasksPanelController::EntrySource::
                         kPermanentToolbarButton &&
                 !it->second->open_time_ticks.is_null()) {
        base::TimeDelta duration =
            base::TimeTicks::Now() - it->second->open_time_ticks;
        base::UmaHistogramLongTimes(
            "ContextualTasks.Session.Duration.PermanentToolbarButton",
            duration);
        it->second->open_time_ticks = base::TimeTicks();
      }
      MaybeDetachWebContents(web_contents);
      it = task_id_to_web_contents_cache_.erase(it);
    } else {
      ++it;
    }
  }

  // Drop pending rebuilds for tasks that are no longer associated with any tab
  // in this window.
  std::erase_if(tasks_pending_panel_rebuild_, [&](const auto& entry) {
    return std::ranges::none_of(
        contextual_tasks_service_->GetTabsAssociatedWithTask(entry.first),
        [&](SessionID tab_id) { return tab_ids.contains(tab_id); });
  });
}

bool ContextualTasksSidePanelCoordinator::UpdateWebContentsForActiveTab() {
  // Return "can't update" result, e.g. if view has not been initialized yet.
  if (!contextual_tasks_panel_host_->IsPanelInitialized()) {
    return false;
  }

  content::WebContents* prev_web_contents = GetActiveWebContents();
  if (prev_web_contents) {
    auto* cache_item = GetWebContentsCacheItemForWebContents(prev_web_contents);
    if (cache_item) {
      cache_item->last_active_time_ticks = base::TimeTicks::Now();
    }
  }

  content::WebContents* web_contents = GetPanelWebContentsForActiveTab();

  bool contents_changed = prev_web_contents != web_contents;
  if (contents_changed) {
    // The toolbar UI itself and location bar class are shared across tasks.
    // Update the single location bar and permission controller to point to
    // `web_contents` (OR clear the toolbar if `web_contents` is null) and push
    // the updated state to the WebUI. This must run before `SetWebContents()`
    // to have the correct web contents, as `SetWebContents()`
    // synchronously marks `web_contents` visible and may trigger
    // `PermissionRequestManager` to query `location_bar_->GetWebContents()`.
    if (permission_controller_) {
      permission_controller_->Update(web_contents);
    }
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && !BUILDFLAG(IS_ANDROID)
    if (extensions_container_) {
      extensions_container_->SetWebContents(web_contents);
    }
#endif
    auto* next_cache_item = GetWebContentsCacheItemForWebContents(web_contents);
    if (next_cache_item) {
      next_cache_item->last_active_time_ticks = base::TimeTicks::Now();
    }
    contextual_tasks_panel_host_->SetWebContents(web_contents);
    if (prev_web_contents) {
      MaybeRemoveImplicitTabContext(prev_web_contents);
    }
#if !BUILDFLAG(IS_ANDROID)
    onboarding_controller_->CloseBubble();
    if (next_cache_item) {
      onboarding_controller_->SetEntryPoint(next_cache_item->entry_point);
    }
#endif
    NotifyExpandToFullTabStateChanged();
  }

  return contents_changed;
}

void ContextualTasksSidePanelCoordinator::OnActiveTabChanged(
    TabListInterface& tab_list,
    tabs::TabInterface* tab) {
  // crbug.com/477278769: Do not open panel if glic panel is already open on tab
  // changed.
  if (contextual_tasks_panel_host_->IsPanelSuppressed()) {
    UpdateOpenState(/*is_open=*/false);
  }

  MaybeRebuildPanelForActiveTab(tab);
  bool was_panel_open = IsPanelOpenForContextualTask();
  bool web_contents_changed = UpdateWebContentsForActiveTab();
  UpdatePanelVisibility();
  // If panel was previously not open, it could have been opened by the
  // UpdatePanelVisibility() call above. Since UpdatePanelVisibility() will call
  // UpdateContextualTaskUI(), there is no need to call it here again.
  if (was_panel_open) {
    UpdateContextualTaskUI();
  }

  bool is_panel_currently_open = IsPanelOpenForContextualTask();
  if (!was_panel_open && is_panel_currently_open) {
    // Panel was previously closed but it is now open due to switching to a tab
    // with an affiliated task.
    RecordUserActionAndHistogram(
        "ContextualTasks.TabChange.UserAction.OpenSidePanel");
  } else if (was_panel_open && !is_panel_currently_open) {
    // Panel was previously open but it is now closed due to switching to a tab
    // without an affiliated task.
    RecordUserActionAndHistogram(
        "ContextualTasks.TabChange.UserAction.CloseSidePanel");
  } else if (was_panel_open && is_panel_currently_open) {
    RecordUserActionAndHistogram(base::StrCat(
        {"ContextualTasks.TabChange.UserAction.",
         web_contents_changed ? "ChangedThreads" : "StayedOnThread"}));
  }

  if (!is_panel_currently_open && tab && tab->GetContents()) {
    // If this tab was opened from a contextual tasks page (e.g. clicking a link
    // on a full tab AIM page), we should not log OpenFullTab.
    tabs::TabInterface* opener = tab_list.GetOpenerForTab(tab->GetHandle());
    bool opened_from_contextual_tasks =
        opener && opener->GetContents() &&
        ContextualTasksUiService::IsContextualTasksUrl(
            opener->GetContents()->GetLastCommittedURL());

    if (!opened_from_contextual_tasks &&
        ContextualTasksUiService::IsContextualTasksUrl(
            tab->GetContents()->GetLastCommittedURL())) {
      base::RecordAction(base::UserMetricsAction(
          "ContextualTasks.TabChange.UserAction.OpenFullTab"));
    }
  }

  ObserveWebContentsOnActiveTab();

  NotifyActiveTaskContextProvider();

  CleanUpUnusedWebContents();
}

content::WebContents*
ContextualTasksSidePanelCoordinator::GetPanelWebContentsForActiveTab() {
  std::optional<ContextualTask> task = GetCurrentTask();
  if (!task) {
    return nullptr;
  }

  base::Uuid task_id = task->GetTaskId();
  content::WebContents* web_contents;
  auto it = task_id_to_web_contents_cache_.find(task_id);
  if (it == task_id_to_web_contents_cache_.end()) {
    return nullptr;
  }

  web_contents = it->second->web_contents.get();

  return web_contents;
}

void ContextualTasksSidePanelCoordinator::MaybeCreateCachedWebContents(
    omnibox::ChromeAimEntryPoint entry_point) {
  std::optional<ContextualTask> task = GetCurrentTask();
  if (!task) {
    return;
  }

  base::Uuid task_id = task->GetTaskId();
  auto it = task_id_to_web_contents_cache_.find(task_id);
  if (it != task_id_to_web_contents_cache_.end()) {
    return;
  }

  // Reload the task's existing thread if its panel is pending a rebuild.
  if (tasks_pending_panel_rebuild_.contains(task_id)) {
    RebuildCachedWebContentsForTask(task_id);
    return;
  }

  // Create new WebContents for the task.
  if (auto* ui_service = GetUiService()) {
    ui_service->SetInitialEntryPointForTask(task_id, entry_point);
    GURL url;
    if (IsContextualTasksSidePanelRearchitectureEnabled()) {
      if (std::optional<GURL> initial_url =
              ui_service->GetInitialUrlForTask(task_id)) {
        url = *initial_url;
      } else if (ui_service->IsTaskWaitingForUrl(task_id)) {
        url = GURL("about:blank");
      } else {
        url = ui_service->GetDefaultAiPageUrlForTask(task_id);
      }
    } else {
      url = ui_service->GetContextualTaskUrlForTask(task_id);
    }
    std::unique_ptr<content::WebContents> wc =
        CreateWebContents(browser_window_);
    content::WebContents* web_contents = wc.get();
    if (IsContextualTasksSidePanelRearchitectureEnabled()) {
      UpdateContextualSearchWebContentsHelperForTask(
          contextual_search_service_, browser_window_,
          contextual_tasks_service_, this, web_contents, task_id);
      ContextualTasksPanelThreadObserver::CreateForWebContents(web_contents);
      // Apply required side panel URL params before the initial load, as the
      // `WebContents` is not attached to a side panel yet.
      url = ContextualTasksUiService::AddRequiredSidePanelUrlChanges(
          url, web_contents);
    }
    // Insert into `task_id_to_web_contents_cache_` before calling `LoadURL()`
    // so that `IsWebContentsInPanel()` returns true during the initial
    // navigation check.
    task_id_to_web_contents_cache_[task_id] =
        std::make_unique<WebContentsCacheItem>(
            std::move(wc), /*is_open=*/true,
            base::BindOnce(&ContextualTasksSidePanelCoordinator::
                               RecordTimeToFirstContentfulPaint,
                           weak_ptr_factory_.GetWeakPtr()));
    web_contents->GetController().LoadURL(url, content::Referrer(),
                                          ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                          std::string());
  }
}

void ContextualTasksSidePanelCoordinator::CreateCachedWebContentsForTesting(
    base::Uuid task_id,
    bool is_open) {
  CHECK_IS_TEST();
  CHECK(!task_id_to_web_contents_cache_.contains(task_id));

  if (auto* ui_service = GetUiService()) {
    std::unique_ptr<content::WebContents> wc =
        CreateWebContents(browser_window_);
    content::WebContents* web_contents = wc.get();
    // Cache before `LoadURL()` so `IsWebContentsInPanel()` returns true on the
    // initial load.
    task_id_to_web_contents_cache_[task_id] =
        std::make_unique<WebContentsCacheItem>(
            std::move(wc), is_open,
            base::BindOnce(&ContextualTasksSidePanelCoordinator::
                               RecordTimeToFirstContentfulPaint,
                           weak_ptr_factory_.GetWeakPtr()));
    web_contents->GetController().LoadURL(
        ui_service->GetContextualTaskUrlForTask(task_id), content::Referrer(),
        ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  }
}

void ContextualTasksSidePanelCoordinator::Hide() {
  contextual_tasks_panel_host_->Close(
      ContextualTasksPanelHost::AnimationStyle::kNoAnimation);
  MaybeRemoveImplicitTabContext();
  Observe(nullptr);

#if !BUILDFLAG(IS_ANDROID)
  onboarding_controller_->CloseBubble();
#endif

  NotifyActiveTaskContextProvider();
}

void ContextualTasksSidePanelCoordinator::Unhide() {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* cache_item = GetWebContentsCacheItemForWebContents(
          GetPanelWebContentsForActiveTab())) {
    onboarding_controller_->SetEntryPoint(cache_item->entry_point);
  }
#endif
  contextual_tasks_panel_host_->Show(
      ContextualTasksPanelHost::AnimationStyle::kNoAnimation);
  UpdateContextualTaskUI();
  ObserveWebContentsOnActiveTab();

  NotifyActiveTaskContextProvider();
}

void ContextualTasksSidePanelCoordinator::ObserveWebContentsOnActiveTab() {
  CHECK(browser_window_);

  if (!IsPanelOpenForContextualTask()) {
    Observe(nullptr);
    return;
  }

  tabs::TabInterface* active_tab_interface =
      TabListInterface::From(browser_window_)->GetActiveTab();
  if (!active_tab_interface) {
    return;
  }

  content::WebContents* web_contents = active_tab_interface->GetContents();
  if (web_contents) {
    Observe(web_contents);
  }
}

void ContextualTasksSidePanelCoordinator::UpdateContextualTaskUI() {
  content::WebContents* web_contents = GetActiveWebContents();

  if (!IsPanelOpenForContextualTask() || !web_contents) {
    return;
  }

  if (IsContextualTasksRearchitectureEnabled() ||
      IsContextualTasksSidePanelRearchitectureEnabled()) {
    ContextualTasksWebContentsUserData::GetOrCreateForWebContents(web_contents)
        ->UpdateImplicitTabContext();
  } else if (auto* web_ui_interface = GetWebUiInterface(web_contents)) {
    web_ui_interface->OnActiveTabContextStatusChanged();
  }

  std::optional<ContextualTask> task = GetCurrentTask();
  if (task) {
    contextual_tasks_service_->SetLastActiveTask(task->GetTaskId());
  }
}

void ContextualTasksSidePanelCoordinator::MaybeDetachWebContents(
    content::WebContents* web_contents) {
  if (web_contents == GetActiveWebContents()) {
    contextual_tasks_panel_host_->SetWebContents(nullptr);
    if (permission_controller_) {
      permission_controller_->Update(nullptr);
    }
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && !BUILDFLAG(IS_ANDROID)
    if (extensions_container_) {
      extensions_container_->SetWebContents(nullptr);
    }
#endif
  }
}

void ContextualTasksSidePanelCoordinator::MaybeRemoveImplicitTabContext(
    content::WebContents* web_contents) {
  if (!IsContextualTasksRearchitectureEnabled() &&
      !IsContextualTasksSidePanelRearchitectureEnabled()) {
    return;
  }
  content::WebContents* target =
      web_contents ? web_contents : GetActiveWebContents();
  if (auto* user_data =
          target ? ContextualTasksWebContentsUserData::FromWebContents(target)
                 : nullptr) {
    user_data->RemoveImplicitTabContext();
  }
}

void ContextualTasksSidePanelCoordinator::DisassociateTabFromTask(
    content::WebContents* web_contents) {
  if (!contextual_tasks_service_) {
    return;
  }
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
  std::optional<ContextualTask> task =
      contextual_tasks_service_->GetContextualTaskForTab(tab_id);
  if (task) {
    contextual_tasks_service_->DisassociateTabFromTask(task->GetTaskId(),
                                                       tab_id);
  }
}

bool ContextualTasksSidePanelCoordinator::ShouldPreserveTaskOnClose() const {
  if (!browser_window_ || !browser_window_->GetProfile() ||
      browser_window_->GetProfile()->IsOffTheRecord()) {
    return false;
  }
  const bool can_resume_from_pinned_button =
      contextual_tasks::IsContextualTasksPinningV2Enabled() &&
      contextual_tasks::IsContextualTasksPinButtonInToolbarEnabled() &&
      contextual_tasks::GetEffectivePinState(browser_window_->GetProfile());
  return contextual_tasks::kShowEntryPoint.Get() ==
             contextual_tasks::EntryPointOption::kToolbarEphemeralBranded ||
         can_resume_from_pinned_button;
}

void ContextualTasksSidePanelCoordinator::DisassociateAllTabsFromCurrentTask() {
  if (!contextual_tasks_service_) {
    return;
  }
  std::optional<ContextualTask> current_task = GetCurrentTask();
  if (current_task) {
    if (ShouldPreserveTaskOnClose()) {
      return;
    }

    std::vector<SessionID> associated_tab_ids =
        contextual_tasks_service_->GetTabsAssociatedWithTask(
            current_task->GetTaskId());
    for (SessionID tab_id : associated_tab_ids) {
      contextual_tasks_service_->DisassociateTabFromTask(
          current_task->GetTaskId(), tab_id);
    }
  } else {
    TabListInterface* tab_list = TabListInterface::From(browser_window_);
    if (tab_list) {
      tabs::TabInterface* active_tab_interface = tab_list->GetActiveTab();
      if (active_tab_interface && active_tab_interface->GetContents()) {
        DisassociateTabFromTask(active_tab_interface->GetContents());
      }
    }
  }
}

void ContextualTasksSidePanelCoordinator::DisassociateActiveTabOnClose() {
  if (ShouldPreserveTaskOnClose()) {
    return;
  }
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser_window_)->GetActiveTab();
  if (!active_tab || !active_tab->GetContents()) {
    return;
  }
  DisassociateTabFromTask(active_tab->GetContents());
  // The tab is no longer associated with a task, so it no longer needs a
  // closed state.
  tabs_with_closed_panel_.erase(active_tab->GetHandle());
}

bool ContextualTasksSidePanelCoordinator::IsPanelOpenOnOtherTabOfTask(
    const base::Uuid& task_id) {
  auto it = task_id_to_web_contents_cache_.find(task_id);
  if (it == task_id_to_web_contents_cache_.end() || !it->second->is_open) {
    return false;
  }
  TabListInterface* tab_list = TabListInterface::From(browser_window_);
  tabs::TabInterface* active_tab = tab_list->GetActiveTab();
  const std::vector<SessionID> task_tab_ids =
      contextual_tasks_service_->GetTabsAssociatedWithTask(task_id);
  return std::ranges::any_of(
      tab_list->GetAllTabs(), [&](tabs::TabInterface* tab) {
        return tab != active_tab && tab->GetContents() &&
               !tabs_with_closed_panel_.contains(tab->GetHandle()) &&
               std::ranges::contains(
                   task_tab_ids,
                   sessions::SessionTabHelper::IdForTab(tab->GetContents()));
      });
}

bool ContextualTasksSidePanelCoordinator::IsTaskAssociatedWithTabInWindow(
    const base::Uuid& task_id) const {
  std::set<SessionID> tab_ids =
      GetAllTabIdsInTabList(TabListInterface::From(browser_window_));
  return std::ranges::any_of(
      contextual_tasks_service_->GetTabsAssociatedWithTask(task_id),
      [&](SessionID tab_id) { return tab_ids.contains(tab_id); });
}

bool ContextualTasksSidePanelCoordinator::IsPanelOpenOnTabOfTask(
    const base::Uuid& task_id) const {
  const std::vector<SessionID> task_tab_ids =
      contextual_tasks_service_->GetTabsAssociatedWithTask(task_id);
  return std::ranges::any_of(
      TabListInterface::From(browser_window_)->GetAllTabs(),
      [&](tabs::TabInterface* tab) {
        return tab->GetContents() &&
               !tabs_with_closed_panel_.contains(tab->GetHandle()) &&
               std::ranges::contains(
                   task_tab_ids,
                   sessions::SessionTabHelper::IdForTab(tab->GetContents()));
      });
}

void ContextualTasksSidePanelCoordinator::MaybeRestorePanelForTab(
    tabs::TabInterface* tab) {
  ContextualTasksUiService* ui_service = GetUiService();
  if (!ShouldRestoreSidePanelOnTabRestore() || !ui_service ||
      !contextual_tasks_service_) {
    return;
  }
  content::WebContents* contents = tab ? tab->GetContents() : nullptr;
  auto* restore_data =
      contents ? ContextualTasksTabRestoreData::FromWebContents(contents)
               : nullptr;
  if (!restore_data) {
    return;
  }
  base::Uuid task_id = restore_data->task_id();
  GURL thread_url = restore_data->thread_url();
  contents->RemoveUserData(ContextualTasksTabRestoreData::UserDataKey());

  if (!ui_service->IsRestorableSidePanelUrl(thread_url)) {
    thread_url = GURL();
  }

  // Rejoin the task if it still exists. Otherwise, start a new task for the
  // saved page, since a task without a known thread is deleted with its last
  // tab.
  const SessionID tab_id = sessions::SessionTabHelper::IdForTab(contents);
  ui_service->AssociateWebContentsToTask(contents, task_id);
  if (!std::ranges::contains(
          contextual_tasks_service_->GetTabsAssociatedWithTask(task_id),
          tab_id)) {
    if (!thread_url.is_valid()) {
      return;
    }
    task_id =
        contextual_tasks_service_->CreateTaskFromUrl(thread_url).GetTaskId();
    ui_service->AssociateWebContentsToTask(contents, task_id);
  }

  // If no other tab in this window shows the task's panel, recreate it with the
  // saved page when the tab becomes active.
  if (!task_id_to_web_contents_cache_.contains(task_id)) {
    if (thread_url.is_valid()) {
      ui_service->SetInitialUrlForTask(task_id, thread_url);
    }
    tasks_pending_panel_rebuild_[task_id] =
        ContextualTasksPanelController::EntrySource::kOther;
  }
}

void ContextualTasksSidePanelCoordinator::MaybeRebuildPanelForActiveTab(
    tabs::TabInterface* active_tab) {
  // If the panel was closed on this tab, `Show()` rebuilds the WebContents when
  // the panel is reopened.
  if (tasks_pending_panel_rebuild_.empty() || !active_tab ||
      tabs_with_closed_panel_.contains(active_tab->GetHandle())) {
    return;
  }
  std::optional<ContextualTask> task = GetCurrentTask();
  if (!task || !tasks_pending_panel_rebuild_.contains(task->GetTaskId())) {
    return;
  }
  RebuildCachedWebContentsForTask(task->GetTaskId());
}

void ContextualTasksSidePanelCoordinator::RebuildCachedWebContentsForTask(
    base::Uuid task_id) {
  auto pending_it = tasks_pending_panel_rebuild_.find(task_id);
  if (pending_it == tasks_pending_panel_rebuild_.end()) {
    return;
  }
  EntrySource entry_source = pending_it->second;
  tasks_pending_panel_rebuild_.erase(pending_it);

  ContextualTasksUiService* ui_service = GetUiService();
  if (!ui_service || task_id_to_web_contents_cache_.contains(task_id)) {
    return;
  }

  std::unique_ptr<content::WebContents> wc = CreateWebContents(browser_window_);
  content::WebContents* web_contents = wc.get();
  // Size the WebContents to match the active panel, in case it loads in the
  // background before it's attached to the side panel view.
  if (content::WebContents* active_web_contents = GetActiveWebContents()) {
    web_contents->Resize(active_web_contents->GetContainerBounds());
  }
  auto cache_item =
      std::make_unique<WebContentsCacheItem>(std::move(wc), /*is_open=*/true);
  cache_item->entry_source = entry_source;
  // The panel was already open for this task, so don't record open latency or
  // session duration metrics for the rebuilt WebContents.
  cache_item->open_time_ticks = base::TimeTicks();
  // Keep the saved page, if any, in case the tab is closed again before the
  // panel loads it.
  cache_item->restored_url =
      ui_service->GetCreationUrlForTask(task_id).value_or(GURL());
  // Insert into `task_id_to_web_contents_cache_` before loading so that
  // `IsWebContentsInPanel()` returns true during the initial navigation check.
  task_id_to_web_contents_cache_[task_id] = std::move(cache_item);

  if (IsContextualTasksSidePanelRearchitectureEnabled()) {
    UpdateContextualSearchWebContentsHelperForTask(
        contextual_search_service_, browser_window_, contextual_tasks_service_,
        this, web_contents, task_id);
    ContextualTasksPanelThreadObserver::CreateForWebContents(web_contents);
    // Load the task's saved page if it has one, e.g. from a reopened tab.
    if (std::optional<GURL> initial_url =
            ui_service->GetInitialUrlForTask(task_id)) {
      OnThreadUrlForRebuiltWebContents(task_id, web_contents->GetWeakPtr(),
                                       *initial_url);
      return;
    }
    ui_service->GetThreadUrlFromTaskId(
        task_id, base::BindOnce(&ContextualTasksSidePanelCoordinator::
                                    OnThreadUrlForRebuiltWebContents,
                                weak_ptr_factory_.GetWeakPtr(), task_id,
                                web_contents->GetWeakPtr()));
    return;
  }

  web_contents->GetController().LoadURL(
      ui_service->GetContextualTaskUrlForTask(task_id), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
}

void ContextualTasksSidePanelCoordinator::OnThreadUrlForRebuiltWebContents(
    const base::Uuid& task_id,
    base::WeakPtr<content::WebContents> web_contents,
    GURL url) {
  if (!web_contents || !url.is_valid()) {
    return;
  }
  auto it = task_id_to_web_contents_cache_.find(task_id);
  if (it == task_id_to_web_contents_cache_.end() ||
      it->second->web_contents.get() != web_contents.get()) {
    return;
  }
  url = ContextualTasksUiService::AddRequiredSidePanelUrlChanges(
      url, web_contents.get());
  web_contents->GetController().LoadURL(url, content::Referrer(),
                                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                        std::string());
}

void ContextualTasksSidePanelCoordinator::UpdateOpenState(bool is_open) {
  std::optional<ContextualTask> task = GetCurrentTask();
  if (!task) {
    return;
  }
  auto it = task_id_to_web_contents_cache_.find(task->GetTaskId());

  if (ShouldScopeSidePanelStateToTab()) {
    // Track the closed state per tab so that other tabs associated with the
    // task keep their open panel.
    tabs::TabInterface* active_tab =
        TabListInterface::From(browser_window_)->GetActiveTab();
    if (!active_tab) {
      return;
    }
    if (is_open) {
      tabs_with_closed_panel_.erase(active_tab->GetHandle());
      if (it != task_id_to_web_contents_cache_.end()) {
        it->second->is_open = true;
      }
    } else {
      tabs_with_closed_panel_.insert(active_tab->GetHandle());
      if (it != task_id_to_web_contents_cache_.end() &&
          !IsPanelOpenOnTabOfTask(task->GetTaskId())) {
        it->second->is_open = false;
      }
      CloseLensSessionForTab(active_tab);
    }
    return;
  }

  if (it != task_id_to_web_contents_cache_.end()) {
    it->second->is_open = is_open;
  }

  if (!is_open) {
    CloseLensSessionsForTask(*task);
  }
}

bool ContextualTasksSidePanelCoordinator::ShouldBeOpen() {
  std::optional<ContextualTask> task = GetCurrentTask();
  if (!task) {
    return false;
  }
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser_window_)->GetActiveTab();
  if (active_tab && tabs_with_closed_panel_.contains(active_tab->GetHandle())) {
    return false;
  }
  auto it = task_id_to_web_contents_cache_.find(task->GetTaskId());
  if (it != task_id_to_web_contents_cache_.end()) {
    return it->second->is_open;
  }
  return false;
}

GURL ContextualTasksSidePanelCoordinator::GetActiveResultsUrl() const {
  std::optional<ContextualTask> task = GetCurrentTask();
  if (!task) {
    return GURL();
  }

  auto cache_it = task_id_to_web_contents_cache_.find(task->GetTaskId());
  if (cache_it == task_id_to_web_contents_cache_.end() || !cache_it->second ||
      !cache_it->second->web_contents) {
    return GURL();
  }

  content::WebContents* web_contents = cache_it->second->web_contents.get();
  GURL url;
  if (ContextualTasksUIInterface* web_ui_interface =
          GetWebUiInterface(web_contents)) {
    // Pre-rearchitecture, the Google results page is embedded inside the
    // chrome://contextual-tasks WebUI.
    url = web_ui_interface->GetInnerFrameUrl();
  } else {
    // Post-rearchitecture, the Google results page is loaded directly in the
    // panel `WebContents`.
    url = web_contents->GetVisibleURL();
  }

  return url.SchemeIsHTTPOrHTTPS() ? url : GURL();
}

bool ContextualTasksSidePanelCoordinator::IsShowingLensResults() const {
  std::optional<ContextualTask> task = GetCurrentTask();
  if (!task) {
    return false;
  }

  auto cache_it = task_id_to_web_contents_cache_.find(task->GetTaskId());
  if (cache_it == task_id_to_web_contents_cache_.end() || !cache_it->second ||
      !cache_it->second->web_contents) {
    return false;
  }

  ContextualTasksUiService* ui_service = GetUiService();
  if (!ui_service) {
    return false;
  }

  const GURL url = GetActiveResultsUrl();
  if (url.is_empty()) {
    // If the URL is empty, but Lens opened the side panel coordinator, assume
    // that the URL is being generated async and will soon have results.
    return cache_it->second->entry_source ==
           ContextualTasksPanelController::EntrySource::kLensOverlay;
  }

  return !ui_service->IsAiUrl(url) &&
         ui_service->IsValidSearchResultsPage(url);
}

bool ContextualTasksSidePanelCoordinator::IsShowingAimResults() const {
  std::optional<ContextualTask> task = GetCurrentTask();
  if (!task) {
    return false;
  }

  auto cache_it = task_id_to_web_contents_cache_.find(task->GetTaskId());
  if (cache_it == task_id_to_web_contents_cache_.end() || !cache_it->second ||
      !cache_it->second->web_contents) {
    return false;
  }

  ContextualTasksUiService* ui_service = GetUiService();
  if (!ui_service) {
    return false;
  }

  const GURL url = GetActiveResultsUrl();
  if (!url.is_empty()) {
    return ui_service->IsAiUrl(url) &&
           !ContextualTasksUI::IsZeroState(url, ui_service);
  }

  // Pre-rearchitecture, the side panel `WebContents` loads
  // chrome://contextual-tasks before the embedded inner frame commits the
  // thread's HTTP/HTTPS URL (and in tests, a `Thread` may be attached to the
  // task without navigating the embedded frame). In that state,
  // `GetActiveResultsUrl()` is empty even though the task already has an
  // active thread.
  return task->GetThread().has_value();
}

void ContextualTasksSidePanelCoordinator::CloseLensSessionsForTask(
    const ContextualTask& task) {
  TabListInterface* tab_list = TabListInterface::From(browser_window_);
  const auto associated_tab_ids =
      contextual_tasks_service_->GetTabsAssociatedWithTask(task.GetTaskId());

  for (int i = 0; i < tab_list->GetTabCount(); ++i) {
    tabs::TabInterface* tab = tab_list->GetTab(i);
    auto it =
        std::find(associated_tab_ids.begin(), associated_tab_ids.end(),
                  sessions::SessionTabHelper::IdForTab(tab->GetContents()));
    if (it != associated_tab_ids.end()) {
      CloseLensSessionForTab(tab);
    }
  }
}

void ContextualTasksSidePanelCoordinator::CloseLensSessionForTab(
    tabs::TabInterface* tab) {
#if !BUILDFLAG(IS_ANDROID)
  if (auto* lens_controller = LensSearchController::From(tab);
      lens_controller && !lens_controller->IsOff()) {
    lens_controller->CloseLensAsync(
        lens::LensOverlayDismissalSource::kSidePanelCloseButton);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ContextualTasksSidePanelCoordinator::CloseLensSessionIfActive(
    tabs::TabInterface* tab_interface,
    omnibox::ChromeAimEntryPoint entry_point,
    ContextualTasksPanelController::EntrySource entry_source) {
#if !BUILDFLAG(IS_ANDROID)
  if (entry_point != omnibox::ChromeAimEntryPoint::
                         DESKTOP_CHROME_COBROWSE_PINNED_TOOLBAR_BUTTON &&
      entry_source !=
          ContextualTasksPanelController::EntrySource::kEphemeralToolbarButton) {
    return;
  }

  auto* search_controller = LensSearchController::From(tab_interface);
  if (search_controller && search_controller->IsActive()) {
    search_controller->CloseLensSync(
        lens::LensOverlayDismissalSource::kUnexpectedSidePanelOpen);
  }
#endif
}

std::pair<std::optional<base::Uuid>,
          contextual_search::ContextualSearchSessionHandle*>
ContextualTasksSidePanelCoordinator::GetSessionHandleForActiveTabOrPanel() {
  content::WebContents* web_contents = nullptr;
  if (IsPanelOpenForContextualTask()) {
    web_contents = GetActiveWebContents();
  } else {
    tabs::TabInterface* active_tab_interface =
        TabListInterface::From(browser_window_)->GetActiveTab();
    if (active_tab_interface) {
      web_contents = active_tab_interface->GetContents();
      auto* ui_service = GetUiService();
      auto* user_data =
          web_contents ? ContextualTasksWebContentsUserData::FromWebContents(
                             web_contents)
                       : nullptr;
      if (web_contents) {
        // Pre-rearchitecture, full-tab AI Mode is at
        // `chrome://contextual-tasks` (`IsContextualTasksUrl`).
        // Post-rearchitecture, it is a Search AI Mode page whose thread task is
        // set on `ContextualTasksWebContentsUserData`.
        const bool is_full_tab_ai_mode_thread =
            user_data && user_data->task_id().has_value();
        if (!is_full_tab_ai_mode_thread &&
            (!ui_service ||
             !ui_service->IsContextualTasksUrl(web_contents->GetURL()))) {
          web_contents = nullptr;
        }
      }
    }
  }

  if (!web_contents) {
    return {std::nullopt, nullptr};
  }

  ContextualSearchWebContentsHelper* helper =
      ContextualSearchWebContentsHelper::FromWebContents(web_contents);
  auto task_id = helper ? helper->task_id() : std::nullopt;
  if (!task_id.has_value()) {
    return {std::nullopt, nullptr};
  }

  return {task_id, helper->GetSessionForTask(task_id.value())};
}

void ContextualTasksSidePanelCoordinator::NotifyActiveTaskContextProvider() {
  active_task_context_provider_->RefreshContext();
}

void ContextualTasksSidePanelCoordinator::AttachTabsToTask(
    const base::Uuid& task_id,
    const std::vector<tabs::TabHandle>& tab_handles) {
#if !BUILDFLAG(IS_ANDROID)
  if (!browser_window_ || !CanShareTabContext(browser_window_->GetProfile())) {
    return;
  }

  auto it = task_id_to_web_contents_cache_.find(task_id);
  content::WebContents* panel_contents =
      it != task_id_to_web_contents_cache_.end()
          ? it->second->web_contents.get()
          : GetActiveWebContents();

  auto* user_data =
      panel_contents
          ? ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
                panel_contents)
          : nullptr;
  contextual_search::ContextualSearchSessionHandle* session_handle = nullptr;
  if (panel_contents) {
    if (auto* helper = ContextualSearchWebContentsHelper::FromWebContents(
            panel_contents)) {
      session_handle = helper->GetSessionForTask(task_id);
      if (!session_handle) {
        session_handle = helper->session_handle();
      }
    }
  }
  if (!session_handle) {
    session_handle = GetContextualSearchSessionHandleForPanel();
  }

  for (tabs::TabHandle tab_handle : tab_handles) {
    tabs::TabInterface* tab = tab_handle.Get();
    if (!tab || !tab->GetContents()) {
      continue;
    }
    content::WebContents* tab_contents = tab->GetContents();
    const GURL& url = tab_contents->GetLastCommittedURL();
    if (!IsUrlValidForContextSharing(url)) {
      continue;
    }

    SessionID session_id = sessions::SessionTabHelper::IdForTab(tab_contents);
    if (!session_id.is_valid()) {
      continue;
    }

    const int32_t tab_handle_id = tab_handle.raw_value();
    if (session_handle) {
      if (user_data) {
        user_data->OnManualTabContextAdded(url, session_id);
      }
      // If this tab is already staged/selected in the composebox (either
      // pending delayed upload or uploaded) but not yet committed in a query,
      // remove the existing uncommitted file/token to avoid duplicating it.
      const auto uploaded_tokens = session_handle->GetUploadedContextTokens();
      const auto attached_tabs = session_handle->GetTabContextState().attached;
      for (const auto& attached_tab : attached_tabs) {
        if (attached_tab.tab_id == session_id.id() &&
            (attached_tab.pending_delayed_upload ||
             std::ranges::contains(uploaded_tokens,
                                   attached_tab.context_token))) {
          if (user_data) {
            user_data->OnTabContextRemoved(attached_tab.context_token);
          }
          if (!session_handle->DeleteFile(attached_tab.context_token)) {
            session_handle->RemoveUploadedContextToken(
                attached_tab.context_token);
          }
        }
      }

      base::RepeatingClosure on_context_uploaded = base::BindRepeating(
          [](base::WeakPtr<ContextualTasksSidePanelCoordinator> weak_this,
             base::WeakPtr<content::WebContents> weak_panel_contents) {
            if (weak_panel_contents) {
              if (auto* user_data =
                      ContextualTasksWebContentsUserData::FromWebContents(
                          weak_panel_contents.get())) {
                if (auto model = user_data->GetOrCreateInputStateModel()) {
                  model->OnContextChanged();
                }
              }
            }
            if (weak_this) {
              weak_this->NotifyActiveTaskContextProvider();
            }
          },
          weak_ptr_factory_.GetWeakPtr(),
          panel_contents ? panel_contents->GetWeakPtr() : nullptr);

      auto result = CaptureAndUploadTabContext(tab_handle_id, session_handle,
                                               std::move(on_context_uploaded),
                                               browser_window_);
      base::UnguessableToken token =
          result.has_value() ? *result : session_handle->CreateContextToken();
      if (!result.has_value() && active_task_context_provider_) {
        active_task_context_provider_->AddLocalTabUnderline(tab_handle);
      }
      if (user_data) {
        user_data->RecordTabIdMapping(tab_handle_id, session_id.id());
        if (result.has_value()) {
          user_data->AssociateTabWithTask(session_id);
          user_data->OnTabContextUploadStarted(*result);
        }
      }
      if (omnibox::IsTabDeselectionInComposeboxEnabled() ||
          IsContextualTasksRearchitectureEnabled() ||
          IsContextualTasksSidePanelRearchitectureEnabled()) {
        session_handle->RemoveDeselectedTab(session_id);
      }
      session_handle->AttachTabPendingUpload(
          token, session_id.id(), url,
          base::UTF16ToUTF8(tab_contents->GetTitle()));
    } else if (active_task_context_provider_) {
      active_task_context_provider_->AddLocalTabUnderline(tab_handle);
    }
  }

  if (user_data) {
    if (auto model = user_data->GetOrCreateInputStateModel()) {
      model->OnContextChanged();
    }
  }

  NotifyActiveTaskContextProvider();
#endif
}

size_t ContextualTasksSidePanelCoordinator::GetNumberOfActiveTasks() const {
  return task_id_to_web_contents_cache_.size();
}

void ContextualTasksSidePanelCoordinator::OnSurfaceStateChanged(
    ContextualTasksPanelHost::SurfaceState state,
    ContextualTasksPanelHost::StateChangeReason reason) {
  // Attach WebContents upon initial panel creation.
  if (state == ContextualTasksPanelHost::SurfaceState::kVisible &&
      reason == ContextualTasksPanelHost::StateChangeReason::kSystemAction) {
    UpdateWebContentsForActiveTab();
    UpdateContextualTaskUI();
  } else {
    NotifyActiveTaskContextProvider();
  }

  if (state == ContextualTasksPanelHost::SurfaceState::kClosed) {
    MaybeRemoveImplicitTabContext();
#if !BUILDFLAG(IS_ANDROID)
    onboarding_controller_->CloseBubble();
#endif
    if (reason == ContextualTasksPanelHost::StateChangeReason::kUserAction) {
      if (closing_entry_source_.has_value()) {
        EntrySource entry_source = *closing_entry_source_;
        switch (entry_source) {
          case EntrySource::kLensOverlay:
            RecordUserActionAndHistogram(
                "ContextualTasks.SidePanel.UserAction.Close.LensOverlay");
            break;
          case EntrySource::kAiModeLinkClick:
            RecordUserActionAndHistogram(
                "ContextualTasks.SidePanel.UserAction.Close.AiModeLinkClick");
            break;
          case EntrySource::kAioToCobr:
            RecordUserActionAndHistogram(
                "ContextualTasks.SidePanel.UserAction.Close.AioToCobr");
            break;
          case EntrySource::kAskGoogle:
            RecordUserActionAndHistogram(
                "ContextualTasks.SidePanel.UserAction.Close.AskGoogle");
            break;
          case EntrySource::kEphemeralToolbarButton:
            RecordUserActionAndHistogram(
                "ContextualTasks.SidePanel.UserAction.Close."
                "EphemeralToolbarButton");
            break;
          case EntrySource::kPermanentToolbarButton:
            if (contextual_tasks::IsContextualTasksPinningV2Enabled()) {
              RecordUserActionAndHistogram(
                  "ContextualTasks.SidePanel.UserAction.Close."
                  "PermanentToolbarButton");
            } else {
              RecordUserActionAndHistogram(
                  "ContextualTasks.SidePanel.UserAction.Close.Other");
            }
            break;
          default:
            RecordUserActionAndHistogram(
                "ContextualTasks.SidePanel.UserAction.Close.Other");
            break;
        }
      }
      if (auto* ui_service = GetUiService()) {
        ui_service->ShowUndoSnackbar(browser_window_);
      }
    }
    closing_entry_source_.reset();
  }

  observers_.Notify(
      &ContextualTasksPanelController::Observer::OnSurfaceStateChanged, state,
      reason);
}

void ContextualTasksSidePanelCoordinator::MoveTaskUiToNewTab() {
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents) {
    return;
  }
  if (auto* web_ui_interface = GetWebUiInterface(web_contents)) {
    web_ui_interface->MoveTaskUiToNewTab();
  }
}

std::optional<tabs::TabHandle>
ContextualTasksSidePanelCoordinator::GetAutoSuggestedTabHandle() {
  auto* web_contents = GetActiveWebContents();
  if (IsContextualTasksRearchitectureEnabled() ||
      IsContextualTasksSidePanelRearchitectureEnabled()) {
    auto* user_data =
        web_contents
            ? ContextualTasksWebContentsUserData::FromWebContents(web_contents)
            : nullptr;
    return user_data ? user_data->GetAutoSuggestedTabHandle() : std::nullopt;
  }
  auto* web_ui_interface = GetWebUiInterface(web_contents);
  if (!web_ui_interface ||
      !web_ui_interface->IsActiveTabContextSuggestionShowing()) {
    return std::nullopt;
  }

  if (auto* manager = web_ui_interface->GetAutoSuggestionManager()) {
    if (const auto* suggestion = manager->GetCurrentSuggestion()) {
      tabs::TabHandle handle(suggestion->tab_id);
      if (handle.Get()) {
        return handle;
      }
    }
  }

  tabs::TabInterface* active_tab_interface =
      TabListInterface::From(browser_window_)->GetActiveTab();
  return active_tab_interface
             ? std::make_optional(active_tab_interface->GetHandle())
             : std::nullopt;
}

void ContextualTasksSidePanelCoordinator::RecordSessionEndMetrics() {
  if (in_cobrowsing_session_) {
    base::UmaHistogramBoolean("ContextualTasks.Session.Completed", true);
  }
  in_cobrowsing_session_ = false;

  for (auto& [task_id, cache_item] : task_id_to_web_contents_cache_) {
    if (!cache_item || cache_item->open_time_ticks.is_null()) {
      continue;
    }
    if (cache_item->entry_source ==
        ContextualTasksPanelController::EntrySource::kAioToCobr) {
      base::TimeDelta duration =
          base::TimeTicks::Now() - cache_item->open_time_ticks;
      base::UmaHistogramLongTimes("ContextualTasks.Session.Duration.AioToCobr",
                                  duration);
      cache_item->open_time_ticks = base::TimeTicks();
    } else if (cache_item->entry_source ==
               ContextualTasksPanelController::EntrySource::
                   kEphemeralToolbarButton) {
      base::TimeDelta duration =
          base::TimeTicks::Now() - cache_item->open_time_ticks;
      base::UmaHistogramLongTimes(
          "ContextualTasks.Session.Duration.EphemeralToolbarButton", duration);
      cache_item->open_time_ticks = base::TimeTicks();
    } else if (contextual_tasks::IsContextualTasksPinningV2Enabled() &&
               cache_item->entry_source ==
                   ContextualTasksPanelController::EntrySource::
                       kPermanentToolbarButton) {
      base::TimeDelta duration =
          base::TimeTicks::Now() - cache_item->open_time_ticks;
      base::UmaHistogramLongTimes(
          "ContextualTasks.Session.Duration.PermanentToolbarButton", duration);
      cache_item->open_time_ticks = base::TimeTicks();
    }
  }
}

void ContextualTasksSidePanelCoordinator::OnEligibilityChange(
    bool is_eligible) {
  if (!is_eligible) {
    if (IsPanelOpenForContextualTask()) {
      Close();
    }
    task_id_to_web_contents_cache_.clear();
    tasks_pending_panel_rebuild_.clear();
    tabs_with_closed_panel_.clear();
  }
#if !BUILDFLAG(IS_ANDROID)
  if (browser_window_ && BrowserActions::From(browser_window_)) {
    if (auto* action_item = actions::ActionManager::Get().FindAction(
            kActionSidePanelShowContextualTasks,
            BrowserActions::From(browser_window_)->root_action_item())) {
      action_item->SetVisible(
          EntryPointEligibilityManager::IsPinningEligible(
              browser_window_->GetProfile()));
    }
  }
#endif
}

void ContextualTasksSidePanelCoordinator::AddObserver(
    ContextualTasksPanelController::Observer* observer) {
  observers_.AddObserver(observer);
}

void ContextualTasksSidePanelCoordinator::RemoveObserver(
    ContextualTasksPanelController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ContextualTasksSidePanelCoordinator::NotifyExpandToFullTabStateChanged() {
  for (auto& observer : observers_) {
    observer.ExpandToFullTabStateChanged();
  }
}

bool ContextualTasksSidePanelCoordinator::CanExpandToFullTab() const {
  content::WebContents* web_contents = GetActiveWebContents();
  if (!web_contents) {
    return false;
  }
  auto* web_ui_interface = GetWebUiInterface(web_contents);
  return web_ui_interface ? web_ui_interface->CanExpandToFullTab() : false;
}

void ContextualTasksSidePanelCoordinator::ShowPageInfoBubble(
    bool is_pointer_interaction) {
#if !BUILDFLAG(IS_ANDROID)
  if (!IsContextualTasksSidePanelRearchitectureEnabled()) {
    return;
  }

  if (page_info_bubble_suppressor_.ShouldSuppressBubbleShow(
          is_pointer_interaction)) {
    return;
  }

  content::WebContents* contents = GetActiveWebContents();
  if (!contents) {
    return;
  }

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(browser_window_);
  if (!browser_view) {
    return;
  }

  views::BubbleAnchor specification_anchor = GetSuperGButtonAnchor();
  if (specification_anchor.IsNull()) {
    return;
  }

  PageInfoBubbleSpecification::Builder builder(
      specification_anchor, browser_view->GetWidget()->GetNativeWindow(),
      contents, contents->GetVisibleURL());
  builder.SetSurface(PageInfo::Surface::kContextualTasks);
  // The panel's WebContents is not a tab, so page info's default lookup
  // (BrowserCollection::FindBrowserWithTab()) cannot resolve a browser for it.
  // Supply the hosting browser window instead; without it, actions such as
  // "Site settings" dereference a null BrowserWindowInterface.
  builder.AddGetBrowserCallback(base::BindRepeating(
      [](base::WeakPtr<ContextualTasksSidePanelCoordinator> coordinator,
         content::WebContents*) -> BrowserWindowInterface* {
        // Page info is owned by the browser window, so it should never outlive
        // the coordinator. Returning null here would only move the crash into
        // chrome::ShowSiteSettings().
        CHECK(coordinator);
        return coordinator->GetBrowserWindow();
      },
      weak_ptr_factory_.GetWeakPtr()));
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Deliberately does not bind `specification_anchor`: this callback outlives
  // the anchored bubble, and the anchor holds a raw pointer to a WebUI
  // `ui::TrackedElement` owned by the side panel document. The anchor is
  // re-resolved when the menu is actually shown instead.
  builder.SetOnExtensionsClickedCallback(base::BindRepeating(
      &ContextualTasksSidePanelCoordinator::OnSeeExtensionsClicked,
      weak_ptr_factory_.GetWeakPtr()));
#endif
  std::unique_ptr<PageInfoBubbleSpecification> specification = builder.Build();

  views::BubbleDialogDelegateView* const bubble =
      PageInfoBubbleView::CreatePageInfoBubble(std::move(specification));

  page_info_bubble_suppressor_.Observe(bubble->GetWidget());

  bubble->GetWidget()->Show();
#else
  // TODO(crbug.com/536100150): Add support to trigger this menu on Android
  // Desktop
#endif
}

#if !BUILDFLAG(IS_ANDROID)
views::BubbleAnchor
ContextualTasksSidePanelCoordinator::GetSuperGButtonAnchor() {
  content::WebContents* webui_contents =
      contextual_tasks_panel_host_
          ? contextual_tasks_panel_host_->GetToolbarWebContents()
          : nullptr;
  if (!webui_contents) {
    return views::BubbleAnchor();
  }

  auto handler = ui::TrackedElementHandlerDocumentSingleton::GetOrCreate(
      webui_contents->GetPrimaryMainFrame());
  if (!handler) {
    return views::BubbleAnchor();
  }

  ui::TrackedElement* anchor_element =
      ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
          kContextualTasksSuperGButtonElementId, handler->context());
  if (!anchor_element) {
    return views::BubbleAnchor();
  }

  return views::BubbleAnchor(anchor_element);
}
#endif

BrowserWindowInterface* ContextualTasksSidePanelCoordinator::GetBrowserWindow()
    const {
  return browser_window_;
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && !BUILDFLAG(IS_ANDROID)
void ContextualTasksSidePanelCoordinator::OnSeeExtensionsClicked() {
  content::WebContents* contents = GetActiveWebContents();
  if (!contents) {
    return;
  }
  if (!extensions_container_) {
    // `base::Unretained` is safe: `extensions_container_` is owned by `this`
    // and therefore cannot outlive it.
    extensions_container_ =
        std::make_unique<ContextualTasksExtensionsContainer>(
            browser_window_, contents,
            base::BindRepeating(
                &ContextualTasksSidePanelCoordinator::GetSuperGButtonAnchor,
                base::Unretained(this)));
  } else {
    extensions_container_->SetWebContents(contents);
  }
  extensions_container_->ShowExtensionsMenu();
}
#endif

void ContextualTasksSidePanelCoordinator::OnLogoPointerDown() {
#if !BUILDFLAG(IS_ANDROID)
  if (!IsContextualTasksSidePanelRearchitectureEnabled()) {
    return;
  }

  page_info_bubble_suppressor_.OnMousePressed();
#endif
}

#if !BUILDFLAG(IS_ANDROID)
void ContextualTasksSidePanelCoordinator::
    MaybeRefreshWebContentsForDarkModeChange() {
  if (!IsContextualTasksSidePanelRearchitectureEnabled()) {
    return;
  }

  Profile* profile = browser_window_->GetProfile();
  const bool dark_mode = contextual_tasks::ShouldUseDarkMode(profile);
  if (dark_mode == is_dark_mode_) {
    return;
  }
  is_dark_mode_ = dark_mode;

  std::vector<base::WeakPtr<content::WebContents>> web_contents_to_refresh;
  web_contents_to_refresh.reserve(task_id_to_web_contents_cache_.size());
  for (auto& [task_id, cache_item] : task_id_to_web_contents_cache_) {
    if (cache_item && cache_item->web_contents) {
      web_contents_to_refresh.push_back(cache_item->web_contents->GetWeakPtr());
    }
  }

  for (const base::WeakPtr<content::WebContents>& weak_wc :
       web_contents_to_refresh) {
    content::WebContents* wc = weak_wc.get();
    if (!wc) {
      continue;
    }
    GURL url = wc->GetVisibleURL();
    if (url.is_empty()) {
      url = wc->GetLastCommittedURL();
    }
    const GURL updated_url =
        ContextualTasksUiService::AddRequiredSidePanelUrlChanges(url, wc);
    if (updated_url != url) {
      wc->GetController().LoadURL(updated_url, content::Referrer(),
                                  ui::PAGE_TRANSITION_RELOAD, std::string());
    }
    if (weak_wc) {
      ContextualTasksWebView::ConfigureWebContentsBackground(weak_wc.get(),
                                                             profile);
    }
  }
}
#endif

}  // namespace contextual_tasks
