// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_SHOW_PERMISSIONS_DIALOG_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_SHOW_PERMISSIONS_DIALOG_HELPER_H_

#include <memory>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
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
  ShowPermissionsDialogHelper(const ShowPermissionsDialogHelper&) = delete;
  ShowPermissionsDialogHelper& operator=(const ShowPermissionsDialogHelper&) =
      delete;

  static void Show(content::BrowserContext* browser_context,
                   content::WebContents* web_contents,
                   const Extension* extension,
                   base::OnceClosure on_complete);

 private:
  ShowPermissionsDialogHelper(Profile* profile, base::OnceClosure on_complete);
  ~ShowPermissionsDialogHelper();  // Manages its own lifetime.

  // Shows the old-style (not AppInfo) permissions dialog.
  void ShowPermissionsDialog(content::WebContents* web_contents,
                             const Extension* extension);

  void OnInstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload payload);

  std::unique_ptr<ExtensionInstallPrompt> prompt_;

  raw_ptr<Profile> profile_;

  base::OnceClosure on_complete_;

  std::string extension_id_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_SHOW_PERMISSIONS_DIALOG_HELPER_H_
