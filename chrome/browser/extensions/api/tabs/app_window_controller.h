// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_APP_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_APP_WINDOW_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
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

  AppWindowController(const AppWindowController&) = delete;
  AppWindowController& operator=(const AppWindowController&) = delete;

  ~AppWindowController() override;

  // extensions::WindowController:
  int GetWindowId() const override;
  std::string GetWindowTypeText() const override;
  void SetFullscreenMode(bool is_fullscreen,
                         const GURL& extension_url) const override;
  bool CanClose(Reason* reason) const override;
  Browser* GetBrowser() const override;
  bool IsDeleteScheduled() const override;
  content::WebContents* GetActiveTab() const override;
  bool HasEditableTabStrip() const override;
  int GetTabCount() const override;
  content::WebContents* GetWebContentsAt(int i) const override;
  bool IsVisibleToTabsAPIForExtension(
      const Extension* extension,
      bool allow_dev_tools_windows) const override;
  base::Value::Dict CreateWindowValueForExtension(
      const Extension* extension,
      PopulateTabBehavior populate_tab_behavior,
      mojom::ContextType context) const override;
  base::Value::List CreateTabList(const Extension* extension,
                                  mojom::ContextType context) const override;
  bool OpenOptionsPage(const Extension* extension) override;
  bool SupportsTabs() override;

 private:
  raw_ptr<AppWindow> app_window_;  // Owns us.
  std::unique_ptr<AppBaseWindow> base_window_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_APP_WINDOW_CONTROLLER_H_
