// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_guest_os.h"

#include "base/values.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

ExtensionFunction::ResponseAction
FileManagerPrivateListMountableGuestsFunction::Run() {
  Profile* profile =
      Profile::FromBrowserContext(browser_context())->GetOriginalProfile();
  auto* registry =
      guest_os::GuestOsService::GetForProfile(profile)->MountProviderRegistry();
  auto entries = base::Value(base::Value::Type::LIST);
  for (const auto& id : registry->List()) {
    auto entry = base::Value(base::Value::Type::DICTIONARY);
    auto* provider = registry->Get(id);
    entry.SetIntKey("id", id);
    entry.SetStringKey("displayName", provider->DisplayName());
    entries.Append(std::move(entry));
  }
  return RespondNow(OneArgument(std::move(entries)));
}

}  // namespace extensions
