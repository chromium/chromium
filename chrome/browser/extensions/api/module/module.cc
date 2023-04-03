// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/module/module.h"

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_url_handlers.h"

namespace extensions {

ExtensionFunction::ResponseAction ExtensionSetUpdateUrlDataFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
  EXTENSION_FUNCTION_VALIDATE(args()[0].is_string());
  const std::string& data = args()[0].GetString();

  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(browser_context());
  if (extension_management->UpdatesFromWebstore(*extension())) {
    return RespondNow(Error(kUnknownErrorDoNotUse));
  }

  ExtensionPrefs::Get(browser_context())
      ->UpdateExtensionPref(extension_id(), kUpdateURLData, base::Value(data));
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction
ExtensionIsAllowedIncognitoAccessFunction::Run() {
  return RespondNow(WithArguments(
      util::IsIncognitoEnabled(extension_id(), browser_context())));
}

ExtensionFunction::ResponseAction
ExtensionIsAllowedFileSchemeAccessFunction::Run() {
  return RespondNow(
      WithArguments(util::AllowFileAccess(extension_id(), browser_context())));
}

}  // namespace extensions
