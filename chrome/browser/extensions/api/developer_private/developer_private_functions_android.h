// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_ANDROID_H_

#include "extensions/browser/extension_function.h"

// We don't have to port it to desktop android because it's an old API for
// chromeos, that is kept just in case there's an app still using it.
DECLARE_UNIMPLEMENTED_EXTENSION_FUNCTION(DeveloperPrivateLoadDirectoryFunction,
                   "developerPrivate.loadDirectory",
                   DEVELOPERPRIVATE_LOADUNPACKEDCROS);

// We don't have to port it to desktop android because MV2 is not supported
// there.
DECLARE_UNIMPLEMENTED_EXTENSION_FUNCTION(
    DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction,
    "developerPrivate.dismissMv2DeprecationNoticeForExtension",
    DEVELOPERPRIVATE_DISMISSMV2DEPRECATIONNOTICEFOREXTENSION);

namespace extensions::api {

class DeveloperPrivateShowSiteSettingsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.showSiteSettings",
                             DEVELOPERPRIVATE_SHOWSITESETTINGS)

 protected:
  ~DeveloperPrivateShowSiteSettingsFunction() override {}
  ResponseAction Run() override;
};

}  // namespace extensions::api

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_ANDROID_H_
