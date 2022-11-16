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

class Browser;  // TODO(stevenjb) eliminate this dependency.
class Profile;

namespace ui {
class BaseWindow;
}

namespace extensions {
class Extension;

// This API needs to be implemented by any window that might be accessed
// through various extension APIs for modifying or finding the window.
// Subclasses must add/remove themselves from the WindowControllerList
// upon construction/destruction.
class WindowController {
 public:
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

  // Returns false if the window is in a state where closing the window is not
  // permitted and sets |reason| if not NULL.
  virtual bool CanClose(Reason* reason) const = 0;

  // Returns a Browser if available. Defaults to returning NULL.
  // TODO(stevenjb): Temporary workaround. Eliminate this.
  virtual Browser* GetBrowser() const;

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

 private:
  raw_ptr<ui::BaseWindow, DanglingUntriaged> window_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_H_
