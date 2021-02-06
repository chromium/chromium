// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_APP_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_APP_WINDOW_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/extensions/window_controller.h"

class Profile;

namespace extensions {

class AppWindow;
class AppBaseWindow;

// A extensions::WindowController specific to extensions::AppWindow.
class AppWindowController : public WindowController {
 public:
  AppWindowController(AppWindow* window,
                      std::unique_ptr<AppBaseWindow> base_window,
                      Profile* profile);
  ~AppWindowController() override;

  // extensions::WindowController:
  int GetWindowId() const override;
  std::string GetWindowTypeText() const override;
  bool CanClose(Reason* reason) const override;
  Browser* GetBrowser() const override;
  bool IsVisibleToTabsAPIForExtension(
      const Extension* extension,
      bool allow_dev_tools_windows) const override;

 private:
  AppWindow* app_window_;  // Owns us.
  std::unique_ptr<AppBaseWindow> base_window_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_APP_WINDOW_CONTROLLER_H_
