// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_TEST_FAKE_ARC_OVERLAY_CONTROLLER_IMPL_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_TEST_FAKE_ARC_OVERLAY_CONTROLLER_IMPL_H_

#include "ash/public/cpp/external_arc/overlay/arc_overlay_controller.h"

namespace ash {

class ASH_PUBLIC_EXPORT FakeArcOverlayControllerImpl
    : public ArcOverlayController {
 public:
  explicit FakeArcOverlayControllerImpl(aura::Window* host_window);
  ~FakeArcOverlayControllerImpl() override;

  // ArcOverlayController:
  void AttachOverlay(aura::Window* overlay_window) override;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_TEST_FAKE_ARC_OVERLAY_CONTROLLER_IMPL_H_
