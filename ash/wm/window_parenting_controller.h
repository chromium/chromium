// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_PARENTING_CONTROLLER_H_
#define ASH_WM_WINDOW_PARENTING_CONTROLLER_H_

#include "ui/aura/client/window_parenting_client.h"

namespace ash {

class WindowParentingController : public aura::client::WindowParentingClient {
 public:
  WindowParentingController();

  WindowParentingController(const WindowParentingController&) = delete;
  WindowParentingController& operator=(const WindowParentingController&) =
      delete;

  ~WindowParentingController() override;

  // aura::client::WindowParentingClient:
  aura::Window* GetDefaultParent(aura::Window* window,
                                 const gfx::Rect& bounds) override;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_PARENTING_CONTROLLER_H_
