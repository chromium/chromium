// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_SHARED_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_SHARED_H_

#include "chrome/common/extensions/api/developer_private.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/extension.h"

namespace extensions::api {

class DeveloperPrivateAPIFunction : public ExtensionFunction {
 protected:
  ~DeveloperPrivateAPIFunction() override;

  // Returns the extension with the given |id| from the registry, including
  // all possible extensions (enabled, disabled, terminated, etc).
  const Extension* GetExtensionById(const ExtensionId& id);

  // Returns the extension with the given |id| from the registry, only checking
  // enabled extensions.
  const Extension* GetEnabledExtensionById(const ExtensionId& id);
};

class DeveloperPrivateUpdateProfileConfigurationFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.updateProfileConfiguration",
                             DEVELOPERPRIVATE_UPDATEPROFILECONFIGURATION)

 private:
  ~DeveloperPrivateUpdateProfileConfigurationFunction() override;
  ResponseAction Run() override;
};

}  // namespace extensions::api

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_SHARED_H_
