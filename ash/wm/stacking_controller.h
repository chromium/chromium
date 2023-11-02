// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_STACKING_CONTROLLER_H_
#define ASH_WM_STACKING_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/window_parenting_client.h"

namespace ash {

class ASH_EXPORT StackingController
    : public aura::client::WindowParentingClient {
 public:
  StackingController();

  StackingController(const StackingController&) = delete;
  StackingController& operator=(const StackingController&) = delete;

  ~StackingController() override;

  // Overridden from aura::client::WindowParentingClient:
  aura::Window* GetDefaultParent(aura::Window* window,
                                 const gfx::Rect& bounds) override;
};

}  // namespace ash

#endif  // ASH_WM_STACKING_CONTROLLER_H_
