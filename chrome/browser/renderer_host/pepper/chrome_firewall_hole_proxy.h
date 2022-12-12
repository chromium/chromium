// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_CHROME_FIREWALL_HOLE_PROXY_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_CHROME_FIREWALL_HOLE_PROXY_H_

#include "build/chromeos_buildflags.h"
#include "content/public/browser/firewall_hole_proxy.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace ash {
class FirewallHole;
}  // namespace ash

class ChromeFirewallHoleProxy : public content::FirewallHoleProxy {
 public:
  explicit ChromeFirewallHoleProxy(
      std::unique_ptr<ash::FirewallHole> firewall_hole);
  ~ChromeFirewallHoleProxy() override;

 private:
  std::unique_ptr<ash::FirewallHole> firewall_hole_;
};
#endif

class ChromeFirewallHoleProxyFactory
    : public content::FirewallHoleProxyFactory {
 public:
  ~ChromeFirewallHoleProxyFactory() override = default;

  // content::FirewallHoleProxyFactory:
  void OpenTCPFirewallHole(const std::string& interface,
                           uint16_t port,
                           OpenCallback callback) override;
  void OpenUDPFirewallHole(const std::string& interface,
                           uint16_t port,
                           OpenCallback callback) override;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_CHROME_FIREWALL_HOLE_PROXY_H_
