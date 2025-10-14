// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_scope.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/metadata/metadata_impl_macros.h"

using SidePanelWebUIViewT_ContextualTasksUI =
    SidePanelWebUIViewT<ContextualTasksUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_ContextualTasksUI,
                        SidePanelWebUIViewT)
END_METADATA

namespace contextual_tasks {

namespace {

inline constexpr char kContextualTasksUrl[] = "chrome://contextual-tasks/";

}  // namespace

DEFINE_USER_DATA(ContextualTasksSidePanelCoordinator);

ContextualTasksSidePanelCoordinator::ContextualTasksSidePanelCoordinator(
    BrowserWindowInterface* browser_window,
    SidePanelCoordinator* side_panel_coordinator)
    : side_panel_coordinator_(side_panel_coordinator),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
  CreateAndRegisterEntry(side_panel_coordinator_->GetWindowRegistry());
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
      SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks),
      base::BindRepeating(&ContextualTasksSidePanelCoordinator::CreateWebView,
                          base::Unretained(this)),
      /*default_content_width_callback=*/base::NullCallback());
  entry->set_should_show_header(false);
  entry->set_should_show_outline(false);
  entry->AddObserver(this);
  global_registry->Register(std::move(entry));
}

void ContextualTasksSidePanelCoordinator::Show() {
  side_panel_coordinator_->Show(
      SidePanelEntry::Key(SidePanelEntry::Id::kContextualTasks));
}

void ContextualTasksSidePanelCoordinator::OnEntryShown(SidePanelEntry* entry) {}

std::unique_ptr<views::View> ContextualTasksSidePanelCoordinator::CreateWebView(
    SidePanelEntryScope& scope) {
  // TODO(crbug.com/449225421): Add web contents cache for existing threads.
  return std::make_unique<SidePanelWebUIViewT<ContextualTasksUI>>(
      scope, base::RepeatingClosure(), base::RepeatingClosure(),
      std::make_unique<WebUIContentsWrapperT<ContextualTasksUI>>(
          GURL(kContextualTasksUrl),
          scope.GetBrowserWindowInterface().GetProfile(),
          IDS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TITLE,
          /*esc_closes_ui=*/false));
}

}  // namespace contextual_tasks
