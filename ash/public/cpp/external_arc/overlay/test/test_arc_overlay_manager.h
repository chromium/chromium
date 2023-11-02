// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_TEST_TEST_ARC_OVERLAY_MANAGER_H_
#define ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_TEST_TEST_ARC_OVERLAY_MANAGER_H_

#include "ash/public/cpp/external_arc/overlay/arc_overlay_manager.h"

namespace ash {

class ASH_PUBLIC_EXPORT TestArcOverlayManager : public ArcOverlayManager {
 public:
  TestArcOverlayManager();
  ~TestArcOverlayManager() override;

  // ArcOverlayManager:
  std::unique_ptr<ArcOverlayController> CreateController(
      aura::Window* host_window) override;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EXTERNAL_ARC_OVERLAY_TEST_TEST_ARC_OVERLAY_MANAGER_H_
