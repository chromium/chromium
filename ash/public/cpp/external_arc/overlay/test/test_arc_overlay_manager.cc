// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/overlay/test/test_arc_overlay_manager.h"

#include "ash/public/cpp/external_arc/overlay/test/fake_arc_overlay_controller_impl.h"

namespace ash {

TestArcOverlayManager::TestArcOverlayManager() = default;

TestArcOverlayManager::~TestArcOverlayManager() = default;

std::unique_ptr<ArcOverlayController> TestArcOverlayManager::CreateController(
    aura::Window* host_window) {
  return std::make_unique<FakeArcOverlayControllerImpl>(host_window);
}

}  // namespace ash
