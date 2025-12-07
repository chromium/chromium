// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FILE_HANDLERS_WEB_FILE_HANDLERS_PERMISSION_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_FILE_HANDLERS_WEB_FILE_HANDLERS_PERMISSION_HANDLER_H_

#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/files/safe_base_name.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "extensions/common/extension_id.h"
#include "ui/base/interaction/element_identifier.h"

class Profile;

DECLARE_ELEMENT_IDENTIFIER_VALUE(kWebFileHandlersFileLaunchDialogCheckbox);

namespace extensions {

class Extension;

// Object that verifies that files can be opened by the extension.
class WebFileHandlersPermissionHandler {
 public:
  using CallbackType = base::OnceCallback<void(bool)>;

  explicit WebFileHandlersPermissionHandler(Profile* profile);
  ~WebFileHandlersPermissionHandler();

  static base::AutoReset<bool> SetRememberSelectionForTesting(
      bool remember_selection);

  // Confirm verifies that this file type is allowed to be opened by the
  // extension. CHECKs that base_names is non-empty.
  void Confirm(const Extension& extension,
               const std::vector<base::SafeBaseName>& base_names,
               CallbackType launch_callback);

 private:
  static void ShowFileLaunchDialog(
      const std::vector<base::SafeBaseName>& base_names,
      const std::vector<std::u16string>& file_types,
      base::OnceCallback<void(bool, bool)> callback);

  void CallbackAfterDialog(const ExtensionId& extension_id,
                           const std::vector<std::u16string>& file_types,
                           CallbackType launch_callback,
                           bool should_open,
                           bool should_remember);

  static bool remember_selection_;

  // Get apps::FileHandlers, useful for automatically obtaining file
  // extensions.
  // TODO(crbug.com/40269541): Refactor CreateIntentFiltersForExtension.
  // TODO(crbug.com/40269541): Store this in place of manifest
  // `file_handlers`.
  const apps::FileHandlers GetAppsFileHandlers(const Extension& extension);

  // Remember the profile when this object is created.
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;

  base::WeakPtrFactory<WebFileHandlersPermissionHandler> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FILE_HANDLERS_WEB_FILE_HANDLERS_PERMISSION_HANDLER_H_
