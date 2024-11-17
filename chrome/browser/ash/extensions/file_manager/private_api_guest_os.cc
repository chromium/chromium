// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/private_api_guest_os.h"

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/ash/extensions/file_manager/private_api_util.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "extensions/browser/extension_function.h"

namespace {

// Thresholds for mountGuest() API.
constexpr base::TimeDelta kMountGuestSlowOperationThreshold = base::Seconds(10);
constexpr base::TimeDelta kMountGuestVerySlowOperationThreshold =
    base::Seconds(30);

}  // namespace

namespace extensions {

ExtensionFunction::ResponseAction
FileManagerPrivateListMountableGuestsFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  auto guests = file_manager::util::CreateMountableGuestList(profile);
  auto response = extensions::api::file_manager_private::ListMountableGuests::
      Results::Create(guests);
  return RespondNow(ArgumentList(std::move(response)));
}

FileManagerPrivateMountGuestFunction::FileManagerPrivateMountGuestFunction() {
  // Mounting shares may require a VM to be started.
  SetWarningThresholds(kMountGuestSlowOperationThreshold,
                       kMountGuestVerySlowOperationThreshold);
}

FileManagerPrivateMountGuestFunction::~FileManagerPrivateMountGuestFunction() =
    default;

ExtensionFunction::ResponseAction FileManagerPrivateMountGuestFunction::Run() {
  using extensions::api::file_manager_private::MountGuest::Params;
  const std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  Profile* profile = Profile::FromBrowserContext(browser_context());
  auto* registry = guest_os::GuestOsServiceFactory::GetForProfile(profile)
                       ->MountProviderRegistry();
  auto* provider = registry->Get(params->id);
  if (provider == nullptr) {
    auto error =
        base::StringPrintf("Unable to find guest for id %d", params->id);
    LOG(ERROR) << error;
    return RespondNow(Error(error));
  }
  provider->Mount(base::BindOnce(
      &FileManagerPrivateMountGuestFunction::MountCallback, this));
  return RespondLater();
}

void FileManagerPrivateMountGuestFunction::MountCallback(bool success) {
  if (!success) {
    Respond(Error("Error mounting guest"));
    return;
  }
  Respond(NoArguments());
}

}  // namespace extensions
