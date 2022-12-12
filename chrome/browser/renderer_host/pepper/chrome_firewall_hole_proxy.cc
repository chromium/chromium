// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/chrome_firewall_hole_proxy.h"

#include "base/functional/callback.h"
#include "base/notreached.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/network/firewall_hole.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
void OnFirewallHoleOpened(content::FirewallHoleProxy::OpenCallback callback,
                          std::unique_ptr<ash::FirewallHole> firewall_hole) {
  if (!firewall_hole) {
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(
      std::make_unique<ChromeFirewallHoleProxy>(std::move(firewall_hole)));
}
#endif

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
ChromeFirewallHoleProxy::ChromeFirewallHoleProxy(
    std::unique_ptr<ash::FirewallHole> firewall_hole)
    : firewall_hole_(std::move(firewall_hole)) {}

ChromeFirewallHoleProxy::~ChromeFirewallHoleProxy() = default;
#endif

void ChromeFirewallHoleProxyFactory::OpenTCPFirewallHole(
    const std::string& interface,
    uint16_t port,
    OpenCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::FirewallHole::Open(
      ash::FirewallHole::PortType::TCP, port, interface,
      base::BindOnce(&OnFirewallHoleOpened, std::move(callback)));
#else
  NOTIMPLEMENTED();
#endif
}

void ChromeFirewallHoleProxyFactory::OpenUDPFirewallHole(
    const std::string& interface,
    uint16_t port,
    OpenCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::FirewallHole::Open(
      ash::FirewallHole::PortType::UDP, port, interface,
      base::BindOnce(&OnFirewallHoleOpened, std::move(callback)));
#else
  NOTIMPLEMENTED();
#endif
}
