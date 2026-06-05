// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_search_session_finder.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_interface.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/view_type_utils.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

#if !BUILDFLAG(IS_ANDROID)
// LINT.IfChange(GetContextualTasksArmShortName)
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
// LINT.ThenChange(//chrome/browser/about_flags.cc)
#endif  // !BUILDFLAG(IS_ANDROID)

std::unique_ptr<content::WebContents> CreateWebContents(
    BrowserWindowInterface* browser_window,
    GURL url) {
  content::WebContents::CreateParams create_params(
      browser_window->GetProfile());
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);
  web_contents->GetController().LoadURL(url, content::Referrer(),
                                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                        std::string());
  webui::SetBrowserWindowInterface(web_contents.get(), browser_window);

  // Create PermissionRequestManager explicitly for this WebContents.
  // The permission bubble will anchor to the browser window via
  // BrowserWindowInterface.
  permissions::PermissionRequestManager::CreateForWebContents(
      web_contents.get());

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

}  // namespace

namespace contextual_tasks {

ContextualTasksSidePanelCoordinator::WebContentsCacheItem::WebContentsCacheItem(
    std::unique_ptr<content::WebContents> wc,
    bool open)
    : web_contents(std::move(wc)),
      is_open(open),
      last_active_time_ticks(base::TimeTicks::Now()) {}
ContextualTasksSidePanelCoordinator::WebContentsCacheItem::
    ~WebContentsCacheItem() = default;

DEFINE_USER_DATA(ContextualTasksSidePanelCoordinator);

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
      contextual_tasks_panel_host_(std::move(contextual_tasks_panel_host)),
      contextual_tasks_service_(ContextualTasksServiceFactory::GetForProfile(
          browser_window->GetProfile())),
      contextual_search_service_(ContextualSearchServiceFactory::GetForProfile(
          browser_window->GetProfile())),
      ui_service_(ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser_window->GetProfile())),
      pref_service_(browser_window->GetProfile()->GetPrefs()),
      active_task_context_provider_(active_task_context_provider),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
  active_task_context_provider_->SetContextualTasksPanelController(this);
  TabListInterface::From(browser_window_)->AddTabListInterfaceObserver(this);
  contextual_tasks_panel_host_->AddObserver(this);

  if (eligibility_manager) {
    eligibility_change_subscription_ =
        eligibility_manager->RegisterOnEntryPointEligibilityChanged(
            base::BindRepeating(
                &ContextualTasksSidePanelCoordinator::OnEligibilityChange,
                weak_ptr_factory_.GetWeakPtr()));
  }
}

ContextualTasksSidePanelCoordinator::~ContextualTasksSidePanelCoordinator() {
  for (auto& observer : observers_) {
    observer.OnControllerDestroyed();
  }
  active_task_context_provider_->SetContextualTasksPanelController(nullptr);
  TabListInterface::From(browser_window_)->RemoveTabListInterfaceObserver(this);
  contextual_tasks_panel_host_->RemoveObserver(this);

  RecordSessionEndMetrics();
}

// static
ContextualTasksPanelController* ContextualTasksPanelController::From(
    BrowserWindowInterface* window) {
  return ContextualTasksSidePanelCoordinator::Get(
      window->GetUnownedUserDataHost());
}

void ContextualTasksSidePanelCoordinator::Show(
    bool transition_from_tab,
    omnibox::ChromeAimEntryPoint entry_point) {
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
  if (!GetCurrentTask()) {
    // If no task is found, create a new task and associate it with the active
    // tab.
    ContextualTask task = contextual_tasks_service_->CreateTask();
    ui_service_->AssociateWebContentsToTask(active_tab_interface->GetContents(),
                                            task.GetTaskId());
  }

  MaybeCreateCachedWebContents(entry_point);
  UpdateWebContentsForActiveTab();

  contextual_tasks_panel_host_->Show(
      transition_from_tab
          ? ContextualTasksPanelHost::AnimationStyle::kTransitionFromTab
          : ContextualTasksPanelHost::AnimationStyle::kStandard);

  UpdateOpenState(/*is_open=*/true);
  UpdateContextualTaskUI();
  ObserveWebContentsOnActiveTab();
  NotifyActiveTaskContextProvider();

#if !BUILDFLAG(IS_ANDROID)
  // Hide the GLIC nudge when the panel is opened.
  if (auto* glic_nudge_controller =
          browser_window_->GetFeatures().glic_nudge_controller()) {
    glic_nudge_controller->UpdateNudgeLabel(
        active_tab_interface->GetContents(), "", std::nullopt,
        /*anchored_message_text=*/std::string(),
        glic::GlicNudgeActivity::kNudgeIgnoredOpenedContextualTasksSidePanel,
        base::DoNothing());
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ContextualTasksSidePanelCoordinator::Close() {
  UpdateOpenState(/*is_open=*/false);
  contextual_tasks_panel_host_->Close(
      ContextualTasksPanelHost::AnimationStyle::kStandard);
  Observe(nullptr);

  NotifyActiveTaskContextProvider();

  RecordSessionEndMetrics();
}

bool ContextualTasksSidePanelCoordinator::IsPanelOpenForContextualTask() const {
  return contextual_tasks_panel_host_->IsPanelOpenForContextualTask();
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
  auto it = task_id_to_web_contents_cache_.find(task_id);
  if (it == task_id_to_web_contents_cache_.end()) {
    task_id_to_web_contents_cache_[task_id] =
        std::make_unique<WebContentsCacheItem>(std::move(web_contents),
                                               /*is_open=*/true);
  } else {
    MaybeDetachWebContents(it->second->web_contents.get());
    it->second->web_contents = std::move(web_contents);
  }
  UpdateOpenState(/*is_open=*/true);
  UpdateWebContentsForActiveTab();
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
      IsPanelOpenForContextualTask() &&
      ui_service_->IsContextualTasksUrl(navigation_handle->GetURL())) {
    RecordUserActionAndHistogram(
        "ContextualTasks.BackButton.UserAction."
        "NavigatedFromSidePanelToFullTab");
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
  if (ui_service_->IsContextualTasksUrl(url) &&
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

std::unique_ptr<content::WebContents>
ContextualTasksSidePanelCoordinator::DetachWebContentsForTask(
    const base::Uuid& task_id) {
  auto it = task_id_to_web_contents_cache_.find(task_id);
  if (it != task_id_to_web_contents_cache_.end()) {
    std::unique_ptr<content::WebContents> web_contents =
        std::move(it->second->web_contents);
    // Reset the association with the BrowserWindow since the WebContents will
    // soon be associated with a tab.
    webui::SetBrowserWindowInterface(web_contents.get(),
                                     /*browser_window_interface=*/nullptr);
    MaybeDetachWebContents(web_contents.get());
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
  std::unique_ptr<WebContentsCacheItem> cache_item;
  // Find the web_contents from cache.
  for (auto it = task_id_to_web_contents_cache_.begin();
       it != task_id_to_web_contents_cache_.end(); ++it) {
    if (it->second->web_contents.get() == web_contents) {
      cache_item = std::move(it->second);
      task_id_to_web_contents_cache_.erase(it);
      break;
    }
  }

  if (cache_item) {
    // If found, update the found WebContents with new_task_id.
    // This will potentially kick out the existing WebContents with new_task_id
    // since only 1 WebContents per task is kept in the cache.
    auto it = task_id_to_web_contents_cache_.find(new_task_id);
    if (it != task_id_to_web_contents_cache_.end()) {
      MaybeDetachWebContents(it->second->web_contents.get());
    }
    task_id_to_web_contents_cache_[new_task_id] = std::move(cache_item);
  }

  // Since the task has just changed, ensure that
  // ContextualSearchWebContentsHelper is updated with the task ID and session
  // handle. Note that it ContextualSearchWebContentsHelper can be from an
  // existing WebContents or a newly created one.
  UpdateContextualSearchWebContentsHelperForTask(
      contextual_search_service_, browser_window_, contextual_tasks_service_,
      this, web_contents, new_task_id);
}

void ContextualTasksSidePanelCoordinator::OnAiInteraction() {
  in_cobrowsing_session_ = true;
}

contextual_search::ContextualSearchSessionHandle*
ContextualTasksSidePanelCoordinator::
    GetContextualSearchSessionHandleForPanel() {
  auto* web_contents = GetActiveWebContents();
  if (!web_contents) {
    return nullptr;
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
    ui_service_->AssociateWebContentsToTask(content, task->GetTaskId());
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
    DisassociateTabFromTask(tab->GetContents());
  }
}

std::optional<ContextualTask>
ContextualTasksSidePanelCoordinator::GetCurrentTask() {
  tabs::TabInterface* active_tab_interface =
      TabListInterface::From(browser_window_)->GetActiveTab();
  if (!active_tab_interface) {
    return std::nullopt;
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
      MaybeDetachWebContents(web_contents);
      it = task_id_to_web_contents_cache_.erase(it);
    } else {
      ++it;
    }
  }
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
  if (web_contents) {
    contextual_tasks_panel_host_->SetWebContents(web_contents);
  }

  if (prev_web_contents != web_contents) {
    NotifyExpandToFullTabStateChanged();
  }

  return prev_web_contents != web_contents;
}

void ContextualTasksSidePanelCoordinator::OnActiveTabChanged(
    TabListInterface& tab_list,
    tabs::TabInterface* tab) {
  // crbug.com/477278769: Do not open panel if glic panel is already open on tab
  // changed.
  if (contextual_tasks_panel_host_->IsPanelSuppressed()) {
    UpdateOpenState(/*is_open=*/false);
  }

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

  // Create new WebContents for the task.
  ui_service_->SetInitialEntryPointForTask(task_id, entry_point);
  task_id_to_web_contents_cache_[task_id] =
      std::make_unique<WebContentsCacheItem>(
          CreateWebContents(
              browser_window_,
              ui_service_->GetContextualTaskUrlForTask(task->GetTaskId())),
          /*is_open=*/true);
}

void ContextualTasksSidePanelCoordinator::CreateCachedWebContentsForTesting(
    base::Uuid task_id,
    bool is_open) {
  CHECK_IS_TEST();
  CHECK(!task_id_to_web_contents_cache_.contains(task_id));

  task_id_to_web_contents_cache_[task_id] =
      std::make_unique<WebContentsCacheItem>(
          CreateWebContents(browser_window_,
                            ui_service_->GetContextualTaskUrlForTask(task_id)),
          is_open);
}

void ContextualTasksSidePanelCoordinator::Hide() {
  contextual_tasks_panel_host_->Close(
      ContextualTasksPanelHost::AnimationStyle::kNoAnimation);
  Observe(nullptr);

  NotifyActiveTaskContextProvider();
}

void ContextualTasksSidePanelCoordinator::Unhide() {
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

  if (auto* web_ui_interface = GetWebUiInterface(web_contents)) {
    web_ui_interface->OnActiveTabContextStatusChanged();
  }
}

void ContextualTasksSidePanelCoordinator::MaybeDetachWebContents(
    content::WebContents* web_contents) {
  if (web_contents == GetActiveWebContents()) {
    contextual_tasks_panel_host_->SetWebContents(nullptr);
  }
}

void ContextualTasksSidePanelCoordinator::DisassociateTabFromTask(
    content::WebContents* web_contents) {
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
  std::optional<ContextualTask> task =
      contextual_tasks_service_->GetContextualTaskForTab(tab_id);
  if (task) {
    contextual_tasks_service_->DisassociateTabFromTask(task->GetTaskId(),
                                                       tab_id);
  }
}

void ContextualTasksSidePanelCoordinator::UpdateOpenState(bool is_open) {
  std::optional<ContextualTask> task = GetCurrentTask();
  if (!task) {
    return;
  }
  auto it = task_id_to_web_contents_cache_.find(task->GetTaskId());
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
  auto it = task_id_to_web_contents_cache_.find(task->GetTaskId());
  if (it != task_id_to_web_contents_cache_.end()) {
    return it->second->is_open;
  }
  return false;
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
#if !BUILDFLAG(IS_ANDROID)
      if (auto* lens_controller = LensSearchController::From(tab);
          !lens_controller->IsOff()) {
        lens_controller->CloseLensAsync(
            lens::LensOverlayDismissalSource::kSidePanelCloseButton);
      }
#endif  // !BUILDFLAG(IS_ANDROID)
    }
  }
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
      if (web_contents &&
          !ui_service_->IsContextualTasksUrl(web_contents->GetURL())) {
        web_contents = nullptr;
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
  } else {
    NotifyActiveTaskContextProvider();
  }

  if (state == ContextualTasksPanelHost::SurfaceState::kClosed &&
      reason == ContextualTasksPanelHost::StateChangeReason::kUserAction) {
    ui_service_->ShowUndoSnackbar(browser_window_);
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
  auto* web_ui_interface = GetWebUiInterface(web_contents);
  if (!web_ui_interface ||
      !web_ui_interface->IsActiveTabContextSuggestionShowing()) {
    return std::nullopt;
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
}

void ContextualTasksSidePanelCoordinator::OnEligibilityChange(
    bool is_eligible) {
  if (!is_eligible) {
    if (IsPanelOpenForContextualTask()) {
      Close();
    }
    task_id_to_web_contents_cache_.clear();
  }
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

}  // namespace contextual_tasks
