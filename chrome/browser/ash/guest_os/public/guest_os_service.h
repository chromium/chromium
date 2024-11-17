// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_SERVICE_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_SERVICE_H_

#include <memory>

#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider_registry.h"
#include "chrome/browser/ash/guest_os/public/guest_os_sk_forwarder.h"
#include "chrome/browser/ash/guest_os/public/guest_os_terminal_provider_registry.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace guest_os {

class GuestOsWaylandServer;

// A service to hold the subservices that make up the Guest OS API surface.
// NOTE: We don't start at browser startup, instead being created on-demand. At
// some point we may change that, but for now creating is cheap, we won't always
// be used (e.g. if the guest os flag isn't set), and we don't need to listen
// for events - everything we care about knows how to access us via KeyedService
// machinery.
class GuestOsService : public KeyedService {
 public:
  explicit GuestOsService(Profile* profile);
  ~GuestOsService() override;

  GuestOsMountProviderRegistry* MountProviderRegistry();
  GuestOsTerminalProviderRegistry* TerminalProviderRegistry();

  GuestOsWaylandServer* WaylandServer();

  GuestOsSkForwarder* SkForwarder();

 private:
  GuestOsMountProviderRegistry mount_provider_registry_;
  GuestOsTerminalProviderRegistry terminal_provider_registry_;
  std::unique_ptr<GuestOsWaylandServer> wayland_server_;
  GuestOsSkForwarder guest_os_sk_forwarder_;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_SERVICE_H_
