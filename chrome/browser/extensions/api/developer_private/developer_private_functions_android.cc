// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_functions_android.h"

#include "chrome/browser/ui/android/extensions/extension_developer_private_bridge.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "extensions/browser/extension_function.h"

DEFINE_UNIMPLEMENTED_EXTENSION_FUNCTION(DeveloperPrivateLoadDirectoryFunction,
                                        "developerPrivate.loadDirectory")
DEFINE_UNIMPLEMENTED_EXTENSION_FUNCTION(
    DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction,
    "developerPrivate.dismissMv2DeprecationNoticeForExtension")

namespace extensions::api {

ExtensionFunction::ResponseAction
DeveloperPrivateShowSiteSettingsFunction::Run() {
  std::optional<developer_private::ShowSiteSettings::Params> params =
      developer_private::ShowSiteSettings::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  const std::string& extension_id = params->extension_id;
  ExtensionDeveloperPrivateBridge::ShowSiteSettings(extension_id);
  return RespondNow(NoArguments());
}

}  // namespace extensions::api
