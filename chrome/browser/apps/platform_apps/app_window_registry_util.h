// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_APP_WINDOW_REGISTRY_UTIL_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_APP_WINDOW_REGISTRY_UTIL_H_

#include "ui/gfx/native_widget_types.h"

namespace extensions {
class AppWindow;
}

// Utility functions to interact with app windows across all profiles.
class AppWindowRegistryUtil {
 public:
  // Returns the app window for |window|, looking in all browser contexts.
  static extensions::AppWindow* GetAppWindowForNativeWindowAnyProfile(
      gfx::NativeWindow window);

  // Returns true if the number of visible app windows registered across all
  // browser contexts is non-zero. |window_type_mask| is a bitwise OR filter of
  // AppWindow::WindowType, or 0 for any window type.
  static bool IsAppWindowVisibleInAnyProfile(int window_type_mask);

  // Close all app windows in all profiles.
  static void CloseAllAppWindows();
};

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_APP_WINDOW_REGISTRY_UTIL_H_
