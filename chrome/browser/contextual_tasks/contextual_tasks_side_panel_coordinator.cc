// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"

#include "base/functional/bind.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_scope.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_tasks/public/features.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_entry.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"

using SidePanelWebUIViewT_ContextualTasksUI =
    SidePanelWebUIViewT<ContextualTasksUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_ContextualTasksUI,
                        SidePanelWebUIViewT)
END_METADATA

namespace {
inline constexpr int kSidePanelPreferredDefaultWidth = 440;

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

std::set<SessionID> GetAllTabIdsInTabStrip(TabStripModel* tab_strip_model) {
  std::set<SessionID> tab_ids;
  for (tabs::TabInterface* tab : *tab_strip_model) {
    tab_ids.insert(sessions::SessionTabHelper::IdForTab(tab->GetContents()));
  }
  return tab_ids;
}

}  // namespace

namespace contextual_tasks {

ContextualTasksSidePanelCoordinator::WebContentsCacheItem::WebContentsCacheItem(
    std::unique_ptr<content::WebContents> wc,
    bool open)
    : web_contents(std::move(wc)), is_open(open) {}
ContextualTasksSidePanelCoordinator::WebContentsCacheItem::
    ~WebContentsCacheItem() = default;

DEFINE_USER_DATA(ContextualTasksSidePanelCoordinator);

class ContextualTasksWebView : public views::WebView {
 public:
  explicit ContextualTasksWebView(content::BrowserContext* browser_context)
      : views::WebView(browser_context) {
    SetProperty(views::kElementIdentifierKey,
                kContextualTasksSidePanelWebViewElementId);
  }
  ~ContextualTasksWebView() override = default;

  base::WeakPtr<ContextualTasksWebView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<ContextualTasksWebView> weak_ptr_factory_{this};
};

ContextualTasksSidePanelCoordinator::ContextualTasksSidePanelCoordinator(
    BrowserWindowInterface* browser_window)
    : browser_window_(browser_window),
      context_controller_(
          ContextualTasksContextControllerFactory::GetForProfile(
              browser_window->GetProfile())),
      ui_service_(ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser_window->GetProfile())),
      pref_service_(browser_window->GetProfile()->GetPrefs()),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
  CreateAndRegisterEntry(SidePanelRegistry::From(browser_window_));
  active_tab_subscription_ =
      browser_window->RegisterActiveTabDidChange(base::BindRepeating(
          &ContextualTasksSidePanelCoordinator::OnActiveTabChanged,
          base::Unretained(this)));
  browser_window_->GetTabStripModel()->AddObserver(this);
}

ContextualTasksSidePanelCoordinator::~ContextualTasksSidePanelCoordinator() {
  browser_window_->GetTabStripModel()->RemoveObserver(this);
}

// static
ContextualTasksSidePanelCoordinator* ContextualTasksSidePanelCoordinator::From(
    BrowserWindowInterface* window) {
  return Get(window->GetUnownedUserDataHost());
}

void ContextualTasksSidePanelCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  if (global_registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks))) {
    return;
  }

  auto entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::PanelType::kToolbar,
      SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks),
      base::BindRepeating(
          &ContextualTasksSidePanelCoordinator::CreateSidePanelView,
          base::Unretained(this)),
      base::BindRepeating(&ContextualTasksSidePanelCoordinator::
                              GetPreferredDefaultSidePanelWidth,
                          base::Unretained(this)));
  entry->set_should_show_ephemerally_in_toolbar(false);
  entry->set_should_show_header(false);
  entry->set_should_show_outline(false);
  global_registry->Register(std::move(entry));
}

void ContextualTasksSidePanelCoordinator::Show(bool transition_from_tab) {
  // Increment the impression count and attempt to show the HaTS survey.
  int impression_count =
      pref_service_->GetInteger(prefs::kContextualTasksNextPanelOpenCount);
  if (base::FeatureList::IsEnabled(
          features::kHappinessTrackingSurveysForDesktopNextPanel) &&
      impression_count >= 1) {
    HatsService* hats_service =
        HatsServiceFactory::GetForProfile(browser_window_->GetProfile(),
                                          /* create_if_necessary = */ true);
    if (hats_service) {
      hats_service->LaunchDelayedSurvey(
          kHatsSurveyTriggerNextPanel, 90000, {},
          {{"Experiment ID", "4R1Q1L4GennVNwyF88Ccc6"}});
    }
  }
  pref_service_->SetInteger(prefs::kContextualTasksNextPanelOpenCount,
                            impression_count + 1);

  if (!GetCurrentTask()) {
    // If no task is found, create a new task and associate it with the active
    // tab.
    ContextualTask task = context_controller_->CreateTask();
    tabs::TabInterface* active_tab_interface =
        browser_window_->GetActiveTabInterface();
    CHECK(active_tab_interface);
    ui_service_->AssociateWebContentsToTask(active_tab_interface->GetContents(),
                                            task.GetTaskId());
  }

  if (transition_from_tab) {
    views::View* content =
        BrowserElementsViews::From(browser_window_)
            ->RetrieveView(kActiveContentsWebViewRetrievalId);
    gfx::Rect content_bounds_in_browser_coordinates =
        content->ConvertRectToWidget(content->GetContentsBounds());
    browser_window_->GetFeatures().side_panel_ui()->ShowFrom(
        SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks),
        content_bounds_in_browser_coordinates);
  } else {
    browser_window_->GetFeatures().side_panel_ui()->Show(
        SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks));
  }
  UpdateOpenState(/*is_open=*/true);
  UpdateContextualTaskUI();
  ObserveWebContentsOnActiveTab();
  browser_window_->GetFeatures()
      .contextual_tasks_active_task_context_provider()
      ->OnSidePanelStateUpdated(GetContextualSearchSessionHandleForSidePanel());
}

void ContextualTasksSidePanelCoordinator::Close() {
  UpdateOpenState(/*is_open=*/false);
  browser_window_->GetFeatures().side_panel_ui()->Close(
      SidePanelEntry::PanelType::kToolbar);
  Observe(nullptr);

  browser_window_->GetFeatures()
      .contextual_tasks_active_task_context_provider()
      ->OnSidePanelStateUpdated(/*session_handle=*/nullptr);
}

bool ContextualTasksSidePanelCoordinator::IsSidePanelOpen() {
  return browser_window_->GetFeatures().side_panel_ui()->IsSidePanelShowing(
      SidePanelEntry::PanelType::kToolbar);
}

bool ContextualTasksSidePanelCoordinator::IsSidePanelOpenForContextualTask()
    const {
  return browser_window_->GetFeatures()
      .side_panel_ui()
      ->IsSidePanelEntryShowing(
          SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks));
}

void ContextualTasksSidePanelCoordinator::TransferWebContentsFromTab(
    const base::Uuid& task_id,
    std::unique_ptr<content::WebContents> web_contents) {
  // Clear the back stack whenever a WebContents is moved to the side panel.
  // This helps prevent any unintended back/forward navigation.
  if (web_contents->GetController().CanPruneAllButLastCommitted()) {
    web_contents->GetController().PruneAllButLastCommitted();
  }

  SetBrowserWindowInterface(web_contents.get(), browser_window_);
  auto it = task_id_to_web_contents_cache_.find(task_id);
  if (it == task_id_to_web_contents_cache_.end()) {
    task_id_to_web_contents_cache_[task_id] =
        std::make_unique<WebContentsCacheItem>(std::move(web_contents),
                                               /*is_open=*/true);
  } else {
    MaybeDetachWebContentsFromWebView(it->second->web_contents.get());
    it->second->web_contents = std::move(web_contents);
  }
  UpdateOpenState(/*is_open=*/true);
  UpdateWebContentsForActiveTab();
}

void ContextualTasksSidePanelCoordinator::PrimaryPageChanged(
    content::Page& page) {
  UpdateContextualTaskUI();
}

void ContextualTasksSidePanelCoordinator::TitleWasSet(
    content::NavigationEntry* entry) {
  UpdateContextualTaskUI();
}

content::WebContents*
ContextualTasksSidePanelCoordinator::GetActiveWebContentsForTesting() {
  return web_view_ ? web_view_->web_contents() : nullptr;
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
    MaybeDetachWebContentsFromWebView(web_contents.get());
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

  if (!cache_item) {
    return;
  }

  // If found, update the found WebContents with new_task_id
  // This will potentially kick out the existing WebContents with new_task_id
  // since only 1 WebContents per task is kept in the cache.
  auto it = task_id_to_web_contents_cache_.find(new_task_id);
  if (it != task_id_to_web_contents_cache_.end()) {
    MaybeDetachWebContentsFromWebView(it->second->web_contents.get());
  }
  task_id_to_web_contents_cache_[new_task_id] = std::move(cache_item);
}

contextual_search::ContextualSearchSessionHandle*
ContextualTasksSidePanelCoordinator::
    GetContextualSearchSessionHandleForSidePanel() {
  if (!web_view_ || !web_view_->GetWebContents()) {
    return nullptr;
  }
  auto* helper = ContextualSearchWebContentsHelper::FromWebContents(
      web_view_->GetWebContents());
  return helper ? helper->session_handle() : nullptr;
}

std::optional<ContextualTask>
ContextualTasksSidePanelCoordinator::GetCurrentTask() {
  tabs::TabInterface* active_tab_interface =
      browser_window_->GetActiveTabInterface();
  if (!active_tab_interface) {
    return std::nullopt;
  }

  return context_controller_->GetContextualTaskForTab(
      sessions::SessionTabHelper::IdForTab(
          active_tab_interface->GetContents()));
}

void ContextualTasksSidePanelCoordinator::UpdateSidePanelVisibility() {
  bool should_be_open = ShouldBeOpen();

  // If state is open and the side panel is closed, open the side panel.
  if (should_be_open && !IsSidePanelOpenForContextualTask()) {
    Unhide();
    return;
  }

  // If state is closed and the side panel is open, close the side panel.
  if (!should_be_open && IsSidePanelOpenForContextualTask()) {
    Hide();
    return;
  }
}

void ContextualTasksSidePanelCoordinator::CleanUpUnusedWebContents() {
  std::set<SessionID> tab_ids =
      GetAllTabIdsInTabStrip(browser_window_->GetTabStripModel());
  for (auto it = task_id_to_web_contents_cache_.begin();
       it != task_id_to_web_contents_cache_.end();) {
    base::Uuid task_id = it->first;
    // If the WebContents has no open tabs associated with it in the current
    // window, then remove it.
    bool found = false;
    for (auto tab_id :
         context_controller_->GetTabsAssociatedWithTask(task_id)) {
      if (base::Contains(tab_ids, tab_id)) {
        found = true;
        break;
      }
    }

    if (!found) {
      MaybeDetachWebContentsFromWebView(it->second->web_contents.get());
      it = task_id_to_web_contents_cache_.erase(it);
    } else {
      ++it;
    }
  }
}

int ContextualTasksSidePanelCoordinator::GetPreferredDefaultSidePanelWidth() {
  return kSidePanelPreferredDefaultWidth;
}

void ContextualTasksSidePanelCoordinator::UpdateWebContentsForActiveTab() {
  if (!web_view_) {
    return;
  }

  content::WebContents* web_contents =
      MaybeGetOrCreateSidePanelWebContentsForActiveTab();
  if (web_contents) {
    web_view_->SetWebContents(web_contents);
  }
}

void ContextualTasksSidePanelCoordinator::OnActiveTabChanged(
    BrowserWindowInterface* browser_interface) {
  bool is_side_panel_open = IsSidePanelOpenForContextualTask();
  UpdateWebContentsForActiveTab();
  UpdateSidePanelVisibility();
  // If side panel was previously not open, it could have been opened
  // by the UpdateSidePanelVisibility() call above. Since
  // UpdateSidePanelVisibility() will call UpdateContextualTaskUI(),
  // there is no need to call it here again.
  if (is_side_panel_open) {
    UpdateContextualTaskUI();
  }
  ObserveWebContentsOnActiveTab();

  browser_window_->GetFeatures()
      .contextual_tasks_active_task_context_provider()
      ->OnSidePanelStateUpdated(
          IsSidePanelOpenForContextualTask()
              ? GetContextualSearchSessionHandleForSidePanel()
              : nullptr);
}

void ContextualTasksSidePanelCoordinator::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& content : change.GetInsert()->contents) {
      // If the new tab is already associated with a task, do nothing.
      if (context_controller_->GetContextualTaskForTab(
              sessions::SessionTabHelper::IdForTab(content.contents))) {
        continue;
      }

      // If the new tab has an opener and it's associated to a task, associate
      // the new tab to the same task.
      tabs::TabInterface* opener =
          tab_strip_model->GetOpenerOfTabAt(content.index);
      if (!opener) {
        continue;
      }
      // Check if the new tab is opened through a link click
      content::NavigationController& controller =
          content.contents->GetController();
      if (!controller.GetActiveEntry() ||
          !ui::PageTransitionCoreTypeIs(
              controller.GetActiveEntry()->GetTransitionType(),
              ui::PAGE_TRANSITION_LINK)) {
        continue;
      }
      std::optional<ContextualTask> task =
          context_controller_->GetContextualTaskForTab(
              sessions::SessionTabHelper::IdForTab(opener->GetContents()));
      if (task) {
        ui_service_->AssociateWebContentsToTask(content.contents,
                                                task->GetTaskId());
      }
    }
  } else if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& content : change.GetRemove()->contents) {
      // Do not disassociate the tab from the task if insert into side panel.
      if (content.remove_reason !=
          TabStripModelChange::RemoveReason::kInsertedIntoSidePanel) {
        DisassociateTabFromTask(content.contents);
      }
    }
    CleanUpUnusedWebContents();
  } else if (change.type() == TabStripModelChange::kReplaced) {
    DisassociateTabFromTask(change.GetReplace()->old_contents);
    CleanUpUnusedWebContents();
  }
}

std::unique_ptr<views::View>
ContextualTasksSidePanelCoordinator::CreateSidePanelView(
    SidePanelEntryScope& scope) {
  std::unique_ptr<ContextualTasksWebView> web_view =
      std::make_unique<ContextualTasksWebView>(browser_window_->GetProfile());
  web_view->SetPaintToLayer();
  web_view->layer()->SetFillsBoundsOpaquely(false);
  web_view_ = web_view->GetWeakPtr();
  UpdateWebContentsForActiveTab();
  return web_view;
}

content::WebContents* ContextualTasksSidePanelCoordinator::
    MaybeGetOrCreateSidePanelWebContentsForActiveTab() {
  std::optional<ContextualTask> task = GetCurrentTask();
  if (!task) {
    return nullptr;
  }

  base::Uuid task_id = task->GetTaskId();
  content::WebContents* web_contents;
  auto it = task_id_to_web_contents_cache_.find(task_id);
  if (it == task_id_to_web_contents_cache_.end()) {
    auto new_item = std::make_unique<WebContentsCacheItem>(
        CreateWebContents(browser_window_,
                          ui_service_->GetContextualTaskUrlForTask(task_id)),
        /*is_open=*/true);
    web_contents = new_item->web_contents.get();
    task_id_to_web_contents_cache_[task_id] = std::move(new_item);
  } else {
    web_contents = it->second->web_contents.get();
  }
  MaybeInitTabScopedOpenState();

  return web_contents;
}

void ContextualTasksSidePanelCoordinator::Hide() {
  auto* side_panel_ui = static_cast<SidePanelCoordinator*>(
      browser_window_->GetFeatures().side_panel_ui());
  side_panel_ui->Close(SidePanelEntry::PanelType::kToolbar,
                       SidePanelEntryHideReason::kSidePanelClosed,
                       /*suppress_animations=*/true);
  Observe(nullptr);

  browser_window_->GetFeatures()
      .contextual_tasks_active_task_context_provider()
      ->OnSidePanelStateUpdated(/*session_handle=*/nullptr);
}

void ContextualTasksSidePanelCoordinator::Unhide() {
  auto* side_panel_ui = static_cast<SidePanelCoordinator*>(
      browser_window_->GetFeatures().side_panel_ui());
  side_panel_ui->Show(
      SidePanelUIBase::UniqueKey{
          /*tab_handle=*/std::nullopt,
          SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks)},
      /*open_trigger=*/std::nullopt, /*suppress_animations=*/true);
  UpdateContextualTaskUI();
  ObserveWebContentsOnActiveTab();

  browser_window_->GetFeatures()
      .contextual_tasks_active_task_context_provider()
      ->OnSidePanelStateUpdated(GetContextualSearchSessionHandleForSidePanel());
}

void ContextualTasksSidePanelCoordinator::ObserveWebContentsOnActiveTab() {
  CHECK(browser_window_);

  if (!IsSidePanelOpenForContextualTask()) {
    Observe(nullptr);
    return;
  }

  tabs::TabInterface* active_tab_interface =
      browser_window_->GetActiveTabInterface();
  if (!active_tab_interface) {
    return;
  }

  content::WebContents* web_contents = active_tab_interface->GetContents();
  if (web_contents) {
    Observe(web_contents);
  }
}

void ContextualTasksSidePanelCoordinator::UpdateContextualTaskUI() {
  if (!web_view_) {
    return;
  }

  if (!IsSidePanelOpenForContextualTask()) {
    return;
  }

  content::WebContents* web_contents = web_view_->GetWebContents();
  content::WebUI* web_ui = web_contents ? web_contents->GetWebUI() : nullptr;
  ContextualTasksUI* contextual_tasks_ui = nullptr;
  if (web_ui->GetController()) {
    contextual_tasks_ui = web_ui->GetController()->GetAs<ContextualTasksUI>();
  }

  if (contextual_tasks_ui) {
    // TODO(http://crbug.com/451688545): Get the TabContextStatus from
    // `context_controller_`.
    contextual_tasks_ui->OnActiveTabContextStatusChanged(
        ContextualTasksUI::TabContextStatus::kNotUploaded);
  }
}

void ContextualTasksSidePanelCoordinator::MaybeDetachWebContentsFromWebView(
    content::WebContents* web_contents) {
  if (web_view_ && web_view_->web_contents() == web_contents) {
    web_view_->SetWebContents(nullptr);
  }
}

void ContextualTasksSidePanelCoordinator::DisassociateTabFromTask(
    content::WebContents* web_contents) {
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
  std::optional<ContextualTask> task =
      context_controller_->GetContextualTaskForTab(tab_id);
  if (task) {
    context_controller_->DisassociateTabFromTask(task->GetTaskId(), tab_id);
  }
  if (!kTaskScopedSidePanel.Get()) {
    tab_scoped_open_state_.erase(tab_id);
  }
}

void ContextualTasksSidePanelCoordinator::UpdateOpenState(bool is_open) {
  if (kTaskScopedSidePanel.Get()) {
    std::optional<ContextualTask> task = GetCurrentTask();
    if (!task) {
      return;
    }
    auto it = task_id_to_web_contents_cache_.find(task->GetTaskId());
    if (it != task_id_to_web_contents_cache_.end()) {
      it->second->is_open = is_open;
    }
  } else {
    tabs::TabInterface* active_tab = browser_window_->GetActiveTabInterface();
    if (!active_tab) {
      return;
    }
    SessionID tab_id =
        sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
    auto it = tab_scoped_open_state_.find(tab_id);
    if (it != tab_scoped_open_state_.end()) {
      it->second = is_open;
    } else {
      tab_scoped_open_state_[tab_id] = is_open;
    }
  }
}

void ContextualTasksSidePanelCoordinator::MaybeInitTabScopedOpenState() {
  if (kTaskScopedSidePanel.Get()) {
    return;
  }

  tabs::TabInterface* active_tab = browser_window_->GetActiveTabInterface();
  if (!active_tab) {
    return;
  }
  // If the open state of the active tab is not found, set the open state
  // to the current open state of the side panel.
  SessionID tab_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  auto it = tab_scoped_open_state_.find(tab_id);
  if (it == tab_scoped_open_state_.end()) {
    tab_scoped_open_state_[tab_id] = IsSidePanelOpenForContextualTask();
  }
}

bool ContextualTasksSidePanelCoordinator::ShouldBeOpen() {
  if (kTaskScopedSidePanel.Get()) {
    std::optional<ContextualTask> task = GetCurrentTask();
    if (!task) {
      return false;
    }
    auto it = task_id_to_web_contents_cache_.find(task->GetTaskId());
    if (it != task_id_to_web_contents_cache_.end()) {
      return it->second->is_open;
    }
    return false;
  } else {
    tabs::TabInterface* active_tab = browser_window_->GetActiveTabInterface();
    if (!active_tab) {
      return false;
    }
    auto it = tab_scoped_open_state_.find(
        sessions::SessionTabHelper::IdForTab(active_tab->GetContents()));
    if (it != tab_scoped_open_state_.end()) {
      return it->second;
    }
    return false;
  }
}

}  // namespace contextual_tasks
