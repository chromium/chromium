
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/actor/ui/mock_actor_overlay_view_controller.h"
namespace actor::ui {
MockActorOverlayViewController::MockActorOverlayViewController(
    tabs::TabInterface& tab_interface)
    : ActorOverlayViewController(tab_interface) {}
MockActorOverlayViewController::~MockActorOverlayViewController() = default;
}  // namespace actor::ui
