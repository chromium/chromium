// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ASSETS_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ASSETS_MANAGER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"

class Profile;

namespace extensions {

class Extension;

// Assets manager for installed extensions. Some extensions can be installed in
// a shared place for multiple profiles (users). This class manages install and
// uninstall. At the time being shared location is used for default apps on
// Chrome OS only. This class must be used only from the extension file task
// runner thread.
class ExtensionAssetsManager {
 public:
  // Callback that is invoked when the extension assets are installed.
  // |file_path| is destination directory on success or empty in case of error.
  typedef base::OnceCallback<void(const base::FilePath& file_path)>
      InstallExtensionCallback;

  static ExtensionAssetsManager* GetInstance();

  // Copy extension assets to final location. This location could be under
  // |local_install_dir| or some common location shared for multiple users.
  virtual void InstallExtension(
      const Extension* extension,
      const base::FilePath& unpacked_extension_root,
      const base::FilePath& local_install_dir,
      Profile* profile,
      InstallExtensionCallback callback,
      bool updates_from_webstore_or_empty_update_url) = 0;

  // Remove extension assets if it is not used by anyone else.
  // `extensions_install_dir` is the path to where extensions of this type are
  // being installed. E.g. "/path/to/Profile/Extensions".
  // `extension_dir_to_delete` is the directory that should be deleted to
  // uninstall the extension.
  virtual void UninstallExtension(const std::string& id,
                                  const std::string& profile_user_name,
                                  const base::FilePath& extensions_install_dir,
                                  const base::FilePath& extension_dir_to_delete,
                                  const base::FilePath& profile_dir) = 0;

 protected:
  virtual ~ExtensionAssetsManager() {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ASSETS_MANAGER_H_
