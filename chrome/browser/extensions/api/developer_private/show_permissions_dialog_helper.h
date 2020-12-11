// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_SHOW_PERMISSIONS_DIALOG_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_SHOW_PERMISSIONS_DIALOG_HELPER_H_

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/extensions/extension_install_prompt.h"

class Profile;

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {
class Extension;

// Helper class to handle showing a permissions dialog for an extension. Will
// show either the newer AppInfo-style permissions dialog, or the traditional,
// install-prompt style dialog.
class ShowPermissionsDialogHelper {
 public:
  static void Show(content::BrowserContext* browser_context,
                   content::WebContents* web_contents,
                   const Extension* extension,
                   bool from_webui,
                   const base::Closure& on_complete);

 private:
  ShowPermissionsDialogHelper(Profile* profile,
                              const base::Closure& on_complete);
  ~ShowPermissionsDialogHelper();  // Manages its own lifetime.

  // Shows the old-style (not AppInfo) permissions dialog.
  void ShowPermissionsDialog(content::WebContents* web_contents,
                             const Extension* extension);

  void OnInstallPromptDone(ExtensionInstallPrompt::Result result);

  std::unique_ptr<ExtensionInstallPrompt> prompt_;

  Profile* profile_;

  base::Closure on_complete_;

  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ShowPermissionsDialogHelper);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_SHOW_PERMISSIONS_DIALOG_HELPER_H_
