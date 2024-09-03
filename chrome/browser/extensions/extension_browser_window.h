// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_WINDOW_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_WINDOW_H_

#include <string>

#include "base/values.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"

class Browser;

namespace content {
class WebContents;
}

namespace extensions {

class Extension;

// An abstract interface that allows the extensions system to refer to a browser
// window. This corresponds to a Browser object on desktop and adds
// extension-specific getters and utilities.
//
// TODO(b/361838438) use this for cross-platform references in the extensions
// system. Currently this is being rolled out.
class ExtensionBrowserWindow {
 public:
  enum PopulateTabBehavior {
    kPopulateTabs,
    kDontPopulateTabs,
  };

  virtual ~ExtensionBrowserWindow() = default;

  // TODO(b/361838438) Remove this. This getter is here to aid in the conversion
  // to using the ExtensionBrowserWindow object instead of Browser*. While this
  // conversion is in place, we need to be able to convert between the two.
  virtual Browser* GetBrowserObject() const = 0;

  // Returns the window ID of this browser window. This is the integer value
  // exposed in the extension window and tab APIs that identifies the window.
  virtual int GetWindowId() const = 0;

  // Returns the tabs:: API constant for the window type of the `browser`.
  virtual std::string GetBrowserWindowTypeText() const = 0;

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

  // On success, returns true and fills in the WebContents and extensions API
  // tab ID for the active tab. The optional_tab_id may be null if the caller
  // doesn't need it. Returns false if there is no active tab.
  virtual bool GetActiveTab(content::WebContents** contents,
                            int* optional_tab_id) const = 0;

  // Open the extension's options page. Returns true if an options page was
  // successfully opened (though it may not necessarily *load*, e.g. if the
  // URL does not exist).
  virtual bool OpenOptionsPage(const Extension* extension) = 0;

  // Returns true if the Browser can report tabs to extensions. Example of
  // Browsers which don't support tabs include apps and devtools.
  virtual bool SupportsTabs() = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_WINDOW_H_
