// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fullscreen.h"

#include <vector>

#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#include "ui/views/widget/widget.h"

bool IsFullScreenMode() {
  std::vector<aura::Window*> all_windows =
      views::DesktopWindowTreeHostLinux::GetAllOpenWindows();
  // Only the topmost window is checked. This works fine in the most cases, but
  // it may return false when there are multiple displays and one display has
  // a fullscreen window but others don't. See: crbug.com/345484
  if (all_windows.empty())
    return false;

  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(all_windows[0]);
  return widget && widget->IsFullscreen();
}
