// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_PARENTING_CONTROLLER_H_
#define ASH_WM_WINDOW_PARENTING_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/client/window_parenting_client.h"

namespace aura {
class Window;
}

namespace ash {

class WindowParentingController : public aura::client::WindowParentingClient {
 public:
  explicit WindowParentingController(aura::Window* root_window);

  WindowParentingController(const WindowParentingController&) = delete;
  WindowParentingController& operator=(const WindowParentingController&) =
      delete;

  ~WindowParentingController() override;

  // aura::client::WindowParentingClient:
  aura::Window* GetDefaultParent(aura::Window* window,
                                 const gfx::Rect& bounds,
                                 const int64_t display_id) override;

 private:
  raw_ptr<aura::Window> root_window_;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_PARENTING_CONTROLLER_H_
