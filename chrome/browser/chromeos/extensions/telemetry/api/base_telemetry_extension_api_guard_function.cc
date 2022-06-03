// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/base_telemetry_extension_api_guard_function.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/hardware_info_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "url/gurl.h"

namespace chromeos {

BaseTelemetryExtensionApiGuardFunction::
    BaseTelemetryExtensionApiGuardFunction() = default;
BaseTelemetryExtensionApiGuardFunction::
    ~BaseTelemetryExtensionApiGuardFunction() = default;

ExtensionFunction::ResponseAction
BaseTelemetryExtensionApiGuardFunction::Run() {
  if (!user_manager::UserManager::Get()->IsCurrentUserOwner()) {
    return RespondNow(Error(
        base::StringPrintf("Unauthorized access to chrome.%s. "
                           "This extension is not run by the device owner",
                           name())));
  }

  if (!IsPwaUiOpen()) {
    return RespondNow(
        Error(base::StringPrintf("Unauthorized access to chrome.%s. "
                                 "Companion PWA UI is not open",
                                 name())));
  }

  // TODO(b/200676085): figure out a better way to async check different
  // conditions.
  HardwareInfoDelegate::Factory::Create()->GetManufacturer(base::BindOnce(
      &BaseTelemetryExtensionApiGuardFunction::OnGetManufacturer, this));

  return RespondLater();
}

bool BaseTelemetryExtensionApiGuardFunction::IsPwaUiOpen() {
  Profile* profile = Profile::FromBrowserContext(browser_context());

  const auto* externally_connectable_info =
      extensions::ExternallyConnectableInfo::Get(extension());

  for (auto* target_browser : *BrowserList::GetInstance()) {
    // Ignore incognito.
    if (target_browser->profile() != profile) {
      continue;
    }

    TabStripModel* target_tab_strip = target_browser->tab_strip_model();
    for (int i = 0; i < target_tab_strip->count(); ++i) {
      content::WebContents* target_contents =
          target_tab_strip->GetWebContentsAt(i);
      if (externally_connectable_info->matches.MatchesURL(
              target_contents->GetLastCommittedURL())) {
        return true;
      }
    }
  }

  return false;
}

void BaseTelemetryExtensionApiGuardFunction::OnGetManufacturer(
    std::string manufacturer) {
  base::TrimWhitespaceASCII(manufacturer, base::TrimPositions::TRIM_ALL,
                            &manufacturer);

  const auto& extension_info = GetChromeOSExtensionInfoForId(extension_id());

  if (manufacturer != extension_info.manufacturer) {
    Respond(Error(base::StringPrintf(
        "Unauthorized access to chrome.%s. "
        "This extension is not allowed to access the API on this device",
        name())));
    return;
  }

  RunIfAllowed();
}

}  // namespace chromeos
