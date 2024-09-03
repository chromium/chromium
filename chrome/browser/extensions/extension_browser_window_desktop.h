// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_WINDOW_DESKTOP_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_WINDOW_DESKTOP_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/extensions/extension_browser_window.h"

namespace extensions {

class ExtensionBrowserWindowDesktop : public ExtensionBrowserWindow {
 public:
  // This object is designed to be owned by the Browser object so the reference
  // parameter will outlive this class.
  explicit ExtensionBrowserWindowDesktop(Browser& browser);
  ~ExtensionBrowserWindowDesktop() override;

  // ExtensionBrowserWindow implementation:
  Browser* GetBrowserObject() const override;
  int GetWindowId() const override;
  std::string GetBrowserWindowTypeText() const override;
  base::Value::Dict CreateWindowValueForExtension(
      const Extension* extension,
      PopulateTabBehavior populate_tab_behavior,
      mojom::ContextType context) const override;
  base::Value::List CreateTabList(const Extension* extension,
                                  mojom::ContextType context) const override;
  bool GetActiveTab(content::WebContents** contents,
                    int* tab_id) const override;

 private:
  const raw_ref<Browser> browser_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_BROWSER_WINDOW_DESKTOP_H_
