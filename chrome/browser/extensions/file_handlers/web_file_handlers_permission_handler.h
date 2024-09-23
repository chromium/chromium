// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FILE_HANDLERS_WEB_FILE_HANDLERS_PERMISSION_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_FILE_HANDLERS_WEB_FILE_HANDLERS_PERMISSION_HANDLER_H_

#include <vector>

#include "base/files/safe_base_name.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {

class Extension;

// Object that verifies that files can be opened by the extension.
class WebFileHandlersPermissionHandler {
 public:
  explicit WebFileHandlersPermissionHandler(Profile* profile);

  using CallbackType = base::OnceCallback<void(bool)>;

  // Confirm verifies that this file type is allowed to be opened by the
  // extension. CHECKs that base_names is non-empty.
  void Confirm(const Extension& extension,
               const std::vector<base::SafeBaseName>& base_names,
               CallbackType launch_callback);

  ~WebFileHandlersPermissionHandler();

 private:
  void CallbackAfterDialog(const ExtensionId& extension_id,
                           const std::vector<std::u16string>& file_types,
                           CallbackType launch_callback,
                           bool should_open,
                           bool should_remember);

  // Get apps::FileHandlers, useful for automatically obtaining file extensions.
  // TODO(crbug.com/40269541): Refactor CreateIntentFiltersForExtension.
  // TODO(crbug.com/40269541): Store this in place of manifest `file_handlers`.
  const apps::FileHandlers GetAppsFileHandlers(const Extension& extension);

  // Remember the profile when this object is created.
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;

  base::WeakPtrFactory<WebFileHandlersPermissionHandler> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FILE_HANDLERS_WEB_FILE_HANDLERS_PERMISSION_HANDLER_H_
