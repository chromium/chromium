// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/public/guest_os_service.h"

#include <memory>

#include "chrome/browser/ash/guest_os/public/guest_os_terminal_provider_registry.h"
#include "chrome/browser/ash/guest_os/public/guest_os_wayland_server.h"

namespace guest_os {

GuestOsService::GuestOsService(Profile* profile)
    : terminal_provider_registry_(profile),
      wayland_server_(std::make_unique<GuestOsWaylandServer>(profile)) {}

GuestOsService::~GuestOsService() = default;

GuestOsMountProviderRegistry* GuestOsService::MountProviderRegistry() {
  return &mount_provider_registry_;
}

GuestOsTerminalProviderRegistry* GuestOsService::TerminalProviderRegistry() {
  return &terminal_provider_registry_;
}

GuestOsWaylandServer* GuestOsService::WaylandServer() {
  return wayland_server_.get();
}

GuestOsSkForwarder* GuestOsService::SkForwarder() {
  return &guest_os_sk_forwarder_;
}

}  // namespace guest_os
