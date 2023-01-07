// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_external_protocol_handler.h"

#include "base/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_files.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/display/display.h"

namespace guest_os {
namespace {

using VmType = guest_os::VmType;

bool AppHandlesProtocol(
    const guest_os::GuestOsRegistryService::Registration& app,
    const GURL& url) {
  if (app.VmType() == guest_os::VmType::BOREALIS &&
      !borealis::IsExternalURLAllowed(url)) {
    return false;
  }
  return app.MimeTypes().count("x-scheme-handler/" + url.scheme()) != 0;
}

}  // namespace

absl::optional<GuestOsRegistryService::Registration> GetHandler(
    Profile* profile,
    const GURL& url) {
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  if (!registry_service) {
    // GuestOsRegistryService does not exist for incognito or guest profiles, so
    // don't try and use it.
    return absl::nullopt;
  }

  absl::optional<GuestOsRegistryService::Registration> result;
  for (auto& pair : registry_service->GetEnabledApps()) {
    auto& registration = pair.second;
    if (AppHandlesProtocol(registration, url) &&
        (!result || registration.LastLaunchTime() > result->LastLaunchTime())) {
      result = std::move(registration);
    }
  }
  return result;
}

void Launch(Profile* profile, const GURL& url) {
  absl::optional<GuestOsRegistryService::Registration> registration =
      GetHandler(profile, url);
  if (!registration) {
    LOG(ERROR) << "No handler for " << url;
    return;
  }

  switch (registration->VmType()) {
    case VmType::TERMINA:
      crostini::LaunchCrostiniApp(profile, registration->app_id(),
                                  display::kInvalidDisplayId, {url.spec()},
                                  base::DoNothing());
      break;

    case VmType::PLUGIN_VM:
      plugin_vm::LaunchPluginVmApp(profile, registration->app_id(),
                                   {url.spec()}, base::DoNothing());
      break;

    case VmType::BOREALIS:
      borealis::BorealisService::GetForProfile(profile)->AppLauncher().Launch(
          registration->app_id(), {url.spec()}, base::DoNothing());
      break;

    default:
      LOG(ERROR) << "Unsupported VmType: "
                 << static_cast<int>(registration->VmType());
  }
}

}  // namespace guest_os
