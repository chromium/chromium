// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_WIDGET_GLIC_SIDE_PANEL_COORDINATOR_ANDROID_H_
#define CHROME_BROWSER_GLIC_PUBLIC_WIDGET_GLIC_SIDE_PANEL_COORDINATOR_ANDROID_H_

#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

class GlicSidePanelCoordinatorAndroid : public GlicSidePanelCoordinator {
 public:
  explicit GlicSidePanelCoordinatorAndroid(tabs::TabInterface* tab);
  ~GlicSidePanelCoordinatorAndroid() override;

  // GlicSidePanelCoordinator:
  using GlicSidePanelCoordinator::Show;
  void Show(bool suppress_animations) override;
  void Close() override;
  bool IsShowing() const override;
  State state() override;
  base::CallbackListSubscription AddStateCallback(
      base::RepeatingCallback<void(State state)> callback) override;
  int GetPreferredWidth() override;
  bool IsGlicSidePanelActive() override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_WIDGET_GLIC_SIDE_PANEL_COORDINATOR_ANDROID_H_
