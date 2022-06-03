// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include "base/check.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"

namespace platform_util {

gfx::NativeWindow GetTopLevel(gfx::NativeView view) {
  return view->GetToplevelWindow();
}

gfx::NativeView GetViewForWindow(gfx::NativeWindow window) {
  DCHECK(window);
  return window;
}

gfx::NativeView GetParent(gfx::NativeView view) {
  return view->parent();
}

bool IsWindowActive(gfx::NativeWindow window) {
  return wm::IsActiveWindow(window);
}

void ActivateWindow(gfx::NativeWindow window) {
  wm::ActivateWindow(window);
}

bool IsVisible(gfx::NativeView view) {
  return view->IsVisible();
}

}  // namespace platform_util
