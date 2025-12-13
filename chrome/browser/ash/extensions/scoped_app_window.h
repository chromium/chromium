// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_SCOPED_APP_WINDOW_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_SCOPED_APP_WINDOW_H_

#include "base/memory/raw_ptr.h"

namespace extensions {
class AppWindow;
}  // namespace extensions

namespace ash {

// Test utility to destroy extensions::AppWindow in RAII style.
class ScopedAppWindow {
 public:
  ScopedAppWindow();
  explicit ScopedAppWindow(extensions::AppWindow* window);
  ScopedAppWindow(ScopedAppWindow&& other);
  ScopedAppWindow& operator=(ScopedAppWindow&& other);
  ~ScopedAppWindow();

  extensions::AppWindow* Get();
  void Reset(extensions::AppWindow* window);
  extensions::AppWindow* operator->();

 private:
  raw_ptr<extensions::AppWindow> window_ = nullptr;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_SCOPED_APP_WINDOW_H_
