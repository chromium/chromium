// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"

class Browser;  // TODO(stevenjb) eliminate this dependency.
class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace ui {
class BaseWindow;
}

namespace extensions {
class Extension;

// This API provides a way for the extension system to talk "up" to the
// enclosing window (the `Browser` object on desktop builds) without depending
// on the implementation details of the exact object.
//
// Subclasses must add/remove themselves from the WindowControllerList upon
// construction/destruction.
class WindowController {
 public:
  enum PopulateTabBehavior {
    kPopulateTabs,
    kDontPopulateTabs,
  };

  enum Reason {
    REASON_NONE,
    REASON_NOT_EDITABLE,
  };

  // A bitmaks used as filter on window types.
  using TypeFilter = uint32_t;

  // Represents the lack of any window filter, implying
  // IsVisibleToExtension will be used as non-filtered behavior.
  static const TypeFilter kNoWindowFilter = 0;

  // Returns a filter allowing all window types to be manipulated
  // through the chrome.windows APIs.
  static TypeFilter GetAllWindowFilter();

  // Builds a filter out of a vector of window types.
  static TypeFilter GetFilterFromWindowTypes(
      const std::vector<api::windows::WindowType>& types);

  static TypeFilter GetFilterFromWindowTypesValues(
      const base::Value::List* types);

  WindowController(ui::BaseWindow* window, Profile* profile);
  WindowController(const WindowController&) = delete;
  WindowController& operator=(const WindowController&) = delete;
  virtual ~WindowController();

  ui::BaseWindow* window() const { return window_; }

  Profile* profile() const { return profile_; }

  // Return an id uniquely identifying the window.
  virtual int GetWindowId() const = 0;

  // Return the type name for the window.
  // TODO(devlin): Remove this in favor of the method on ExtensionTabUtil.
  virtual std::string GetWindowTypeText() const = 0;

  // Sets the window's fullscreen state. |extension_url| provides the url
  // associated with the extension (used by FullscreenController).
  virtual void SetFullscreenMode(bool is_fullscreen,
                                 const GURL& extension_url) const = 0;

  // Returns false if the window is in a state where closing the window is not
  // permitted and sets |reason| if not NULL.
  virtual bool CanClose(Reason* reason) const = 0;

  // Returns a Browser if available. Defaults to returning NULL.
  // TODO(stevenjb): Temporary workaround. Eliminate this.
  virtual Browser* GetBrowser() const;

  // Returns true if the window is in the process of being torn down. See
  // Browser::is_delete_scheduled().
  virtual bool IsDeleteScheduled() const = 0;

  // Returns the WebContents associated with the active tab, if any. Returns
  // null if there is no active tab.
  virtual content::WebContents* GetActiveTab() const = 0;

  // Returns true if this window has a tab strip that's currently editable or
  // if there's no visible tab strip.
  //
  // During some animations and drags the tab strip won't be editable and
  // extensions should not update it. Many callers should use
  // ExtensionTabUtil::IsTabStripEditable() which will check *all* tab strips
  // because some move operations span tab strips. This checking of all windows
  // is why windows that don't have visible tab strips should still return true
  // here: otherwise they will prevent some operations from happening that use
  // the ExtensionTabUtil.
  virtual bool HasEditableTabStrip() const = 0;

  // Returns the number of tabs in this window.
  virtual int GetTabCount() const = 0;

  // Returns the web contents at the given tab index, or null if it's off the
  // end of the tab strip.
  virtual content::WebContents* GetWebContentsAt(int i) const = 0;

  // Returns true if the window is visible to the tabs API, when used by the
  // given |extension|.
  // |allow_dev_tools_windows| indicates whether dev tools windows should be
  // treated as visible.
  // TODO(devlin): Remove include_dev_tools_windows.
  virtual bool IsVisibleToTabsAPIForExtension(
      const Extension* extension,
      bool include_dev_tools_windows) const = 0;

  // Returns true if the window type of the controller matches the |filter|.
  bool MatchesFilter(TypeFilter filter) const;

  // Notifies that a window's bounds are changed.
  void NotifyWindowBoundsChanged();

  // Creates a base::Value::Dict representing the window for the browser and
  // scrubs any privacy-sensitive data that `extension` does not have access to.
  // `populate_tab_behavior` determines whether tabs will be populated in the
  // result. `context` is used to determine the ScrubTabBehavior for the
  // populated tabs data.
  // TODO(devlin): Convert this to a api::Windows::Window object.
  virtual base::Value::Dict CreateWindowValueForExtension(
      const Extension* extension,
      PopulateTabBehavior populate_tab_behavior,
      mojom::ContextType context) const = 0;

  // Returns the JSON tab information for all tabs in this window. See the
  // chrome.tabs.getAllInWindow() extensions API.
  virtual base::Value::List CreateTabList(const Extension* extension,
                                          mojom::ContextType context) const = 0;

  // Open the extension's options page. Returns true if an options page was
  // successfully opened (though it may not necessarily *load*, e.g. if the
  // URL does not exist).
  virtual bool OpenOptionsPage(const Extension* extension) = 0;

  // Returns true if the Browser can report tabs to extensions. Example of
  // Browsers which don't support tabs include apps and devtools.
  virtual bool SupportsTabs() = 0;

 private:
  raw_ptr<ui::BaseWindow, DanglingUntriaged> window_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_H_
