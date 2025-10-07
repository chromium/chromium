// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class SidePanelCoordinator;
class SidePanelEntryScope;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

namespace contextual_tasks {

class ContextualTasksSidePanelCoordinator : public SidePanelEntryObserver {
 public:
  DECLARE_USER_DATA(ContextualTasksSidePanelCoordinator);

  ContextualTasksSidePanelCoordinator(
      BrowserWindowInterface* browser_window,
      SidePanelCoordinator* side_panel_coordinator);
  ContextualTasksSidePanelCoordinator(
      const ContextualTasksSidePanelCoordinator&) = delete;
  ContextualTasksSidePanelCoordinator& operator=(
      const ContextualTasksSidePanelCoordinator&) = delete;
  ~ContextualTasksSidePanelCoordinator() override;

  static ContextualTasksSidePanelCoordinator* From(
      BrowserWindowInterface* window);

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

  void Show();

 protected:
  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;

 private:
  std::unique_ptr<views::View> CreateWebView(SidePanelEntryScope& scope);

  // `side_panel_coordinator_` is expected to outlife this class.
  const raw_ptr<SidePanelCoordinator> side_panel_coordinator_ = nullptr;

  ui::ScopedUnownedUserData<ContextualTasksSidePanelCoordinator>
      scoped_unowned_user_data_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SIDE_PANEL_COORDINATOR_H_
