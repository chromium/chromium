// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"

namespace guest_os {

GuestOsService::GuestOsService() = default;

GuestOsService::~GuestOsService() = default;

GuestOsService* GuestOsService::GetForProfile(Profile* profile) {
  return GuestOsServiceFactory::GetForProfile(profile);
}

GuestOsMountProviderRegistry* GuestOsService::MountProviderRegistry() {
  return &mount_provider_registry_;
}

}  // namespace guest_os
