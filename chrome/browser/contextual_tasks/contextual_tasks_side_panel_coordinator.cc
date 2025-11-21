// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"

#include "base/functional/bind.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_scope.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
  CreateAndRegisterEntry(SidePanelRegistry::From(browser_window_));
  active_tab_subscription_ =
      browser_window->RegisterActiveTabDidChange(base::BindRepeating(
          &ContextualTasksSidePanelCoordinator::OnActiveTabChanged,
          base::Unretained(this)));
}

ContextualTasksSidePanelCoordinator::~ContextualTasksSidePanelCoordinator() =
    default;

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

void ContextualTasksSidePanelCoordinator::Show() {
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

  browser_window_->GetFeatures().side_panel_ui()->Show(
      SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks));
  UpdateOpenStateForCurrentTask(/*is_open=*/true);
  UpdateForActiveTab();
}

void ContextualTasksSidePanelCoordinator::Close() {
  UpdateOpenStateForCurrentTask(/*is_open=*/false);
  browser_window_->GetFeatures().side_panel_ui()->Close(
      SidePanelEntry::PanelType::kToolbar);
  Observe(nullptr);
}

bool ContextualTasksSidePanelCoordinator::IsSidePanelOpen() {
  return browser_window_->GetFeatures().side_panel_ui()->IsSidePanelShowing(
      SidePanelEntry::PanelType::kToolbar);
}

bool ContextualTasksSidePanelCoordinator::IsSidePanelOpenForContextualTask() {
  return browser_window_->GetFeatures()
      .side_panel_ui()
      ->IsSidePanelEntryShowing(
          SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks));
}

void ContextualTasksSidePanelCoordinator::TransferWebContentsFromTab(
    const base::Uuid& task_id,
    std::unique_ptr<content::WebContents> web_contents) {
  SetBrowserWindowInterface(web_contents.get(), browser_window_);
  auto it = task_id_to_web_contents_cache_.find(task_id);
  if (it == task_id_to_web_contents_cache_.end()) {
    task_id_to_web_contents_cache_[task_id] =
        std::make_unique<WebContentsCacheItem>(std::move(web_contents),
                                               /*is_open=*/true);
  } else {
    MaybeDetachWebContentsFromWebView(it->second->web_contents.get());
    it->second->web_contents = std::move(web_contents);
    it->second->is_open = true;
  }
  UpdateWebContentsForActiveTab();
}

void ContextualTasksSidePanelCoordinator::PrimaryPageChanged(
    content::Page& page) {
  UpdateActiveTabContextStatus();
}

void ContextualTasksSidePanelCoordinator::TitleWasSet(
    content::NavigationEntry* entry) {
  UpdateActiveTabContextStatus();
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
  std::optional<ContextualTask> task = GetCurrentTask();

  // If no open state found and side panel is open, close it.
  if (!task) {
    if (IsSidePanelOpenForContextualTask()) {
      Hide();
    }
    return;
  }

  bool is_open = false;
  auto it = task_id_to_web_contents_cache_.find(task->GetTaskId());
  if (it != task_id_to_web_contents_cache_.end()) {
    is_open = it->second->is_open;
  }

  // If state is open and the side panel is closed, open the side panel.
  if (is_open && !IsSidePanelOpenForContextualTask()) {
    Unhide();
    return;
  }

  // If state is closed and the side panel is open, close the side panel.
  if (!is_open && IsSidePanelOpenForContextualTask()) {
    Hide();
    return;
  }
}

void ContextualTasksSidePanelCoordinator::UpdateOpenStateForCurrentTask(
    bool is_open) {
  std::optional<ContextualTask> task = GetCurrentTask();
  if (!task) {
    return;
  }

  auto it = task_id_to_web_contents_cache_.find(task->GetTaskId());
  if (it != task_id_to_web_contents_cache_.end()) {
    it->second->is_open = is_open;
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
  UpdateWebContentsForActiveTab();
  UpdateSidePanelVisibility();
  UpdateForActiveTab();
}

std::unique_ptr<views::View>
ContextualTasksSidePanelCoordinator::CreateSidePanelView(
    SidePanelEntryScope& scope) {
  std::unique_ptr<ContextualTasksWebView> web_view =
      std::make_unique<ContextualTasksWebView>(browser_window_->GetProfile());
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

  return web_contents;
}

void ContextualTasksSidePanelCoordinator::Hide() {
  auto* side_panel_ui = static_cast<SidePanelCoordinator*>(
      browser_window_->GetFeatures().side_panel_ui());
  side_panel_ui->Close(SidePanelEntry::PanelType::kToolbar,
                       SidePanelEntryHideReason::kSidePanelClosed,
                       /*suppress_animations=*/true);
  Observe(nullptr);
}

void ContextualTasksSidePanelCoordinator::Unhide() {
  auto* side_panel_ui = static_cast<SidePanelCoordinator*>(
      browser_window_->GetFeatures().side_panel_ui());
  side_panel_ui->Show(
      SidePanelUIBase::UniqueKey{
          /*tab_handle=*/std::nullopt,
          SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks)},
      /*open_trigger=*/std::nullopt, /*suppress_animations=*/true);
  UpdateActiveTabContextStatus();
  UpdateForActiveTab();
}

void ContextualTasksSidePanelCoordinator::UpdateForActiveTab() {
  CHECK(browser_window_);

  UpdateActiveTabContextStatus();

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

void ContextualTasksSidePanelCoordinator::UpdateActiveTabContextStatus() {
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

}  // namespace contextual_tasks
