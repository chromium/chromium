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
    std::optional<base::Uuid> task,
    bool open)
    : web_contents(std::move(wc)), task_id(task), is_open(open) {}
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
}

void ContextualTasksSidePanelCoordinator::Close() {
  UpdateOpenStateForCurrentTask(/*is_open=*/false);
  browser_window_->GetFeatures().side_panel_ui()->Close(
      SidePanelEntry::PanelType::kToolbar);
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
  WebContentsCacheItem* item = FindWebContentsCacheItem(task_id);
  if (item) {
    item->web_contents = std::move(web_contents);
    item->is_open = true;
  } else {
    auto new_item =
        std::make_unique<WebContentsCacheItem>(std::move(web_contents), task_id,
                                               /*is_open=*/true);
    task_id_to_web_contents_cache_.push_back(std::move(new_item));
  }
  UpdateWebContentsForActiveTab();
}

content::WebContents*
ContextualTasksSidePanelCoordinator::GetActiveWebContentsForTesting() {
  return web_view_ ? web_view_->web_contents() : nullptr;
}

std::unique_ptr<content::WebContents>
ContextualTasksSidePanelCoordinator::DetachWebContentsForTask(
    const base::Uuid& task_id) {
  // TODO(crbug.com/451706231): Simplify this when it's a map.
  for (auto it = task_id_to_web_contents_cache_.begin();
       it != task_id_to_web_contents_cache_.end(); ++it) {
    if ((*it)->task_id && (*it)->task_id.value() == task_id) {
      std::unique_ptr<content::WebContents> web_contents =
          std::move((*it)->web_contents);
      // Reset the association with the BrowserWindow since the WebContents will
      // soon be associated with a tab.
      webui::SetBrowserWindowInterface(web_contents.get(),
                                       /*browser_window_interface=*/nullptr);
      if (web_view_ && web_view_->web_contents() == web_contents.get()) {
        web_view_->SetWebContents(nullptr);
      }

      task_id_to_web_contents_cache_.erase(it);
      return web_contents;
    }
  }

  return nullptr;
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
  WebContentsCacheItem* item = FindWebContentsCacheItem(task->GetTaskId());
  if (item) {
    is_open = item->is_open;
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

  WebContentsCacheItem* item = FindWebContentsCacheItem(task->GetTaskId());
  if (item) {
    item->is_open = is_open;
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

ContextualTasksSidePanelCoordinator::WebContentsCacheItem*
ContextualTasksSidePanelCoordinator::FindWebContentsCacheItem(
    const base::Uuid& task_id) {
  for (const auto& item : task_id_to_web_contents_cache_) {
    if (item->task_id && item->task_id == task_id) {
      return item.get();
    }
  }
  return nullptr;
}

content::WebContents* ContextualTasksSidePanelCoordinator::
    MaybeGetOrCreateSidePanelWebContentsForActiveTab() {
  std::optional<ContextualTask> task = GetCurrentTask();
  if (!task) {
    return nullptr;
  }

  base::Uuid task_id = task->GetTaskId();
  WebContentsCacheItem* item = FindWebContentsCacheItem(task_id);
  if (!item) {
    auto new_item = std::make_unique<WebContentsCacheItem>(
        CreateWebContents(browser_window_,
                          ui_service_->GetContextualTaskUrlForTask(task_id)),
        task_id,
        /*is_open=*/true);
    task_id_to_web_contents_cache_.push_back(std::move(new_item));
    item = task_id_to_web_contents_cache_.back().get();
  }

  return item->web_contents.get();
}

void ContextualTasksSidePanelCoordinator::Hide() {
  auto* side_panel_ui = static_cast<SidePanelCoordinator*>(
      browser_window_->GetFeatures().side_panel_ui());
  side_panel_ui->Close(SidePanelEntry::PanelType::kToolbar,
                       SidePanelEntryHideReason::kSidePanelClosed,
                       /*suppress_animations=*/true);
}

void ContextualTasksSidePanelCoordinator::Unhide() {
  auto* side_panel_ui = static_cast<SidePanelCoordinator*>(
      browser_window_->GetFeatures().side_panel_ui());
  side_panel_ui->Show(
      SidePanelUIBase::UniqueKey{
          /*tab_handle=*/std::nullopt,
          SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks)},
      /*open_trigger=*/std::nullopt, /*suppress_animations=*/true);
}

}  // namespace contextual_tasks
