// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"

#include "base/functional/bind.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_scope.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
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
    content::BrowserContext* context) {
  content::WebContents::CreateParams create_params(context);
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);
  web_contents->GetController().LoadURL(
      GURL(chrome::kChromeUIContextualTasksURL), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  return web_contents;
}

}  // namespace

namespace contextual_tasks {

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
    BrowserWindowInterface* browser_window,
    SidePanelCoordinator* side_panel_coordinator)
    : browser_window_(browser_window),
      side_panel_coordinator_(side_panel_coordinator),
      context_controller_(
          ContextualTasksContextControllerFactory::GetForProfile(
              browser_window->GetProfile())),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
  CreateAndRegisterEntry(side_panel_coordinator_->GetWindowRegistry());
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
  side_panel_coordinator_->Show(
      SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks));
}

void ContextualTasksSidePanelCoordinator::Close() {
  side_panel_coordinator_->Close(SidePanelEntry::PanelType::kToolbar);
}

bool ContextualTasksSidePanelCoordinator::IsSidePanelOpen() {
  return side_panel_coordinator_->IsSidePanelShowing(
      SidePanelEntry::PanelType::kToolbar);
}

bool ContextualTasksSidePanelCoordinator::IsSidePanelOpenForContextualTask() {
  return side_panel_coordinator_->IsSidePanelEntryShowing(
      SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks));
}

void ContextualTasksSidePanelCoordinator::TransferWebContentsFromTab(
    const base::Uuid& task_id,
    std::unique_ptr<content::WebContents> web_contents) {
  task_id_to_web_contents_cache_.emplace(task_id, std::move(web_contents));
  UpdateWebContentsForActiveTab();
}

content::WebContents*
ContextualTasksSidePanelCoordinator::GetActiveWebContentsForTesting() {
  return web_view_ ? web_view_->web_contents() : nullptr;
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
  content::WebContents* active_web_contents =
      browser_window_->GetTabStripModel()->GetActiveWebContents();
  if (!active_web_contents) {
    return nullptr;
  }

  std::optional<ContextualTask> task =
      context_controller_->GetContextualTaskForTab(
          sessions::SessionTabHelper::IdForTab(active_web_contents));
  if (!task) {
    return nullptr;
  }

  base::Uuid task_id = task->GetTaskId();
  if (!base::Contains(task_id_to_web_contents_cache_, task_id)) {
    task_id_to_web_contents_cache_.emplace(
        task_id, CreateWebContents(browser_window_->GetProfile()));
  }

  return task_id_to_web_contents_cache_.at(task_id).get();
}

}  // namespace contextual_tasks
