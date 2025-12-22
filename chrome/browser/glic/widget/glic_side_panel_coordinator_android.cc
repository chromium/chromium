// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_android.h"

#include "components/tabs/public/tab_interface.h"

namespace glic {

GlicSidePanelCoordinatorAndroid::GlicSidePanelCoordinatorAndroid(
    tabs::TabInterface* tab)
    : GlicSidePanelCoordinator(tab) {}

GlicSidePanelCoordinatorAndroid::~GlicSidePanelCoordinatorAndroid() = default;

void GlicSidePanelCoordinatorAndroid::Show(bool suppress_animations) {}

void GlicSidePanelCoordinatorAndroid::Close() {}

bool GlicSidePanelCoordinatorAndroid::IsShowing() const {
  return false;
}

GlicSidePanelCoordinator::State GlicSidePanelCoordinatorAndroid::state() {
  return State::kClosed;
}

base::CallbackListSubscription
GlicSidePanelCoordinatorAndroid::AddStateCallback(
    base::RepeatingCallback<void(State state)> callback) {
  return base::CallbackListSubscription();
}

int GlicSidePanelCoordinatorAndroid::GetPreferredWidth() {
  return 0;
}

bool GlicSidePanelCoordinatorAndroid::IsGlicSidePanelActive() {
  return false;
}

}  // namespace glic
