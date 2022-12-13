// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/chrome_firewall_hole_proxy.h"

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/network/firewall_hole.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS_LACROS)
crosapi::mojom::FirewallHoleService* GetFirewallHoleService() {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::FirewallHoleService>()) {
    LOG(ERROR) << "FirewallHoleService is not available in Lacros";
    return nullptr;
  }
  return service->GetRemote<crosapi::mojom::FirewallHoleService>().get();
}
#endif

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)

// static
std::unique_ptr<ChromeFirewallHoleProxyAsh> ChromeFirewallHoleProxyAsh::Create(
    std::unique_ptr<ash::FirewallHole> firewall_hole) {
  if (!firewall_hole) {
    return nullptr;
  }
  return base::WrapUnique(
      new ChromeFirewallHoleProxyAsh(std::move(firewall_hole)));
}

ChromeFirewallHoleProxyAsh::ChromeFirewallHoleProxyAsh(
    std::unique_ptr<ash::FirewallHole> firewall_hole)
    : firewall_hole_(std::move(firewall_hole)) {}

ChromeFirewallHoleProxyAsh::~ChromeFirewallHoleProxyAsh() = default;

#else

// static
std::unique_ptr<ChromeFirewallHoleProxyLacros>
ChromeFirewallHoleProxyLacros::Create(
    mojo::PendingRemote<crosapi::mojom::FirewallHole> firewall_hole) {
  if (!firewall_hole) {
    return nullptr;
  }
  return base::WrapUnique(
      new ChromeFirewallHoleProxyLacros(std::move(firewall_hole)));
}

ChromeFirewallHoleProxyLacros::ChromeFirewallHoleProxyLacros(
    mojo::PendingRemote<crosapi::mojom::FirewallHole> firewall_hole)
    : firewall_hole_(std::move(firewall_hole)) {}

ChromeFirewallHoleProxyLacros::~ChromeFirewallHoleProxyLacros() = default;

#endif

void ChromeFirewallHoleProxyFactory::OpenTCPFirewallHole(
    const std::string& interface,
    uint16_t port,
    OpenCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::FirewallHole::Open(ash::FirewallHole::PortType::TCP, port, interface,
                          base::BindOnce(&ChromeFirewallHoleProxyAsh::Create)
                              .Then(std::move(callback)));
#else
  auto* firewall_hole_service = GetFirewallHoleService();
  if (!firewall_hole_service) {
    std::move(callback).Run(nullptr);
    return;
  }
  firewall_hole_service->OpenTCPFirewallHole(
      interface, port,
      base::BindOnce(&ChromeFirewallHoleProxyLacros::Create)
          .Then(std::move(callback)));
#endif
}

void ChromeFirewallHoleProxyFactory::OpenUDPFirewallHole(
    const std::string& interface,
    uint16_t port,
    OpenCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::FirewallHole::Open(ash::FirewallHole::PortType::UDP, port, interface,
                          base::BindOnce(&ChromeFirewallHoleProxyAsh::Create)
                              .Then(std::move(callback)));
#else
  auto* firewall_hole_service = GetFirewallHoleService();
  if (!firewall_hole_service) {
    std::move(callback).Run(nullptr);
    return;
  }
  firewall_hole_service->OpenUDPFirewallHole(
      interface, port,
      base::BindOnce(&ChromeFirewallHoleProxyLacros::Create)
          .Then(std::move(callback)));
#endif
}
