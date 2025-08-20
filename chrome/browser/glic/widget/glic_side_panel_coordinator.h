// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_COORDINATOR_H_

#include <memory>

class SidePanelEntryScope;
class SidePanelRegistry;

namespace views {
class View;
}  // namespace views

namespace glic {

// GlicSidePanelCoordinator handles the creation and registration of the
// glic SidePanelEntry.
class GlicSidePanelCoordinator {
 public:
  GlicSidePanelCoordinator();
  ~GlicSidePanelCoordinator() = default;

  // Create and register the Glic side panel entry.
  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

 private:
  // Gets the Glic WebView from the Glic service.
  std::unique_ptr<views::View> CreateGlicWebView(SidePanelEntryScope& scope);
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_SIDE_PANEL_COORDINATOR_H_
