// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_service.h"

#include <memory>

#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/ash/guest_os/public/guest_os_wayland_server.h"

namespace guest_os {

GuestOsService::GuestOsService(Profile* profile)
    : wayland_server_(std::make_unique<GuestOsWaylandServer>(profile)) {}

GuestOsService::~GuestOsService() = default;

GuestOsService* GuestOsService::GetForProfile(Profile* profile) {
  return GuestOsServiceFactory::GetForProfile(profile);
}

GuestOsMountProviderRegistry* GuestOsService::MountProviderRegistry() {
  return &mount_provider_registry_;
}

GuestOsWaylandServer* GuestOsService::WaylandServer() {
  return wayland_server_.get();
}

}  // namespace guest_os
