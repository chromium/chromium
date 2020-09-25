// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/guest_os/guest_os_external_protocol_handler.h"

#include "base/time/time.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_files.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/display/display.h"

namespace guest_os {
namespace {

using VmType = guest_os::GuestOsRegistryService::VmType;

bool AppHandlesProtocol(
    const guest_os::GuestOsRegistryService::Registration& app,
    const std::string& scheme) {
  return app.MimeTypes().count("x-scheme-handler/" + scheme) != 0;
}

}  // namespace

base::Optional<GuestOsRegistryService::Registration> GetHandler(
    Profile* profile,
    const GURL& url) {
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);

  base::Optional<GuestOsRegistryService::Registration> result;
  for (auto& pair : registry_service->GetEnabledApps()) {
    auto& registration = pair.second;
    if (AppHandlesProtocol(registration, url.scheme()) &&
        (!result || registration.LastLaunchTime() > result->LastLaunchTime())) {
      result = std::move(registration);
    }
  }
  return result;
}

void Launch(Profile* profile, const GURL& url) {
  base::Optional<GuestOsRegistryService::Registration> registration =
      GetHandler(profile, url);
  if (!registration) {
    LOG(ERROR) << "No handler for " << url;
    return;
  }

  switch (registration->VmType()) {
    case VmType::ApplicationList_VmType_TERMINA:
      crostini::LaunchCrostiniApp(profile, registration->app_id(),
                                  display::kInvalidDisplayId, {url.spec()},
                                  base::DoNothing());
      break;

    case VmType::ApplicationList_VmType_PLUGIN_VM:
      plugin_vm::LaunchPluginVmApp(profile, registration->app_id(),
                                   {url.spec()}, base::DoNothing());
      break;

    default:
      LOG(ERROR) << "Unsupported VmType: "
                 << static_cast<int>(registration->VmType());
  }
}

}  // namespace guest_os
