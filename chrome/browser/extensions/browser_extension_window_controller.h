// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BROWSER_EXTENSION_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_BROWSER_EXTENSION_WINDOW_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/window_controller.h"
#include "components/sessions/core/session_id.h"

class BrowserWindow;
class BrowserWindowInterface;
class GURL;
class Profile;
class TabStripModel;

namespace extensions {
class Extension;
namespace api::tabs {
enum class WindowType;
}  // namespace api::tabs

class BrowserExtensionWindowController : public WindowController {
 public:
  explicit BrowserExtensionWindowController(BrowserWindowInterface* browser);

  BrowserExtensionWindowController(const BrowserExtensionWindowController&) =
      delete;
  BrowserExtensionWindowController& operator=(
      const BrowserExtensionWindowController&) = delete;

  ~BrowserExtensionWindowController() override;

  // Sets the window's fullscreen state. `extension_url` provides the url
  // associated with the extension (used by FullscreenController).

  // WindowController implementation.
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
  bool OpenOptionsPage(const Extension* extension,
                       const GURL& url,
                       bool open_in_tab) override;
  bool SupportsTabs() override;

 private:
  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<Profile> profile_;
  const raw_ptr<BrowserWindow> window_;
  const raw_ptr<TabStripModel> tab_strip_model_;
  const SessionID session_id_;
  const api::tabs::WindowType window_type_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BROWSER_EXTENSION_WINDOW_CONTROLLER_H_
