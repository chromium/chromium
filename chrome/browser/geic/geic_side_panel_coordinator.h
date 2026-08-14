// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEIC_GEIC_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_GEIC_GEIC_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class SidePanelEntryScope;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

namespace geic {

// GeicSidePanelCoordinator handles the creation and registration of the
// GEiC SidePanelEntry.
class GeicSidePanelCoordinator {
 public:
  explicit GeicSidePanelCoordinator(
      BrowserWindowInterface& browser_window_interface);
  ~GeicSidePanelCoordinator();

  static GeicSidePanelCoordinator* From(BrowserWindowInterface* browser);

  DECLARE_USER_DATA(GeicSidePanelCoordinator);

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

  // Toggles the GEiC side panel visibility.
  void Toggle();

 private:
  std::unique_ptr<views::View> CreateGeicView(SidePanelEntryScope& scope);

  const raw_ref<BrowserWindowInterface> browser_window_interface_;
  ui::ScopedUnownedUserData<GeicSidePanelCoordinator> scoped_unowned_user_data_;
};

}  // namespace geic

#endif  // CHROME_BROWSER_GEIC_GEIC_SIDE_PANEL_COORDINATOR_H_
