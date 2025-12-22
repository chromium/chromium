// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"

#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace glic {

DEFINE_USER_DATA(GlicSidePanelCoordinator);

GlicSidePanelCoordinator::GlicSidePanelCoordinator(tabs::TabInterface* tab)
    : scoped_user_data_(tab->GetUnownedUserDataHost(), *this) {}

GlicSidePanelCoordinator::~GlicSidePanelCoordinator() = default;

// static
GlicSidePanelCoordinator* GlicSidePanelCoordinator::GetForTab(
    tabs::TabInterface* tab) {
  if (!tab) {
    return nullptr;
  }
  return Get(tab->GetUnownedUserDataHost());
}

// static
bool GlicSidePanelCoordinator::IsGlicSidePanelActive(tabs::TabInterface* tab) {
  if (auto* coordinator = GetForTab(tab)) {
    return coordinator->IsGlicSidePanelActive();
  }
  return false;
}

}  // namespace glic
