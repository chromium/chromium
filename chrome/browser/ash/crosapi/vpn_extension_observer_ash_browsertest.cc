// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/network_config_service.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/ash_requires_lacros_browsertestbase.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_observer.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "content/public/test/browser_test.h"

namespace {

namespace cros_network = chromeos::network_config;

constexpr char kVpnExtensionName[] = "vpn_extension";

}  // namespace

class VpnExtensionObserverBrowserTest
    : public crosapi::AshRequiresLacrosBrowserTestBase {
 public:
  std::string LoadVpnExtension(const std::string& extension_name) {
    crosapi::mojom::StandaloneBrowserTestControllerAsyncWaiter waiter(
        GetStandaloneBrowserTestController());

    std::string extension_id;
    waiter.LoadVpnExtension(extension_name, &extension_id);
    return extension_id;
  }
};

class ExtensionEventWaiter
    : public ash::network_config::CrosNetworkConfigTestObserver {
 public:
  using VpnProviders = std::vector<cros_network::mojom::VpnProviderPtr>;

  void StartObserving() {
    ash::GetNetworkConfigService(
        cros_network_config_.BindNewPipeAndPassReceiver());
    cros_network_config_->AddObserver(GenerateRemote());
  }

  // cros_network::mojom::CrosNetworkConfigObserver:
  void OnVpnProvidersChanged() override {
    cros_network_config_->GetVpnProviders(base::BindOnce(
        [](base::OnceCallback<void(VpnProviders)> callback,
           VpnProviders vpn_providers) {
          std::move(callback).Run(std::move(vpn_providers));
        },
        vpn_providers_.GetCallback<VpnProviders>()));
  }

  VpnProviders GetVpnProviders() { return vpn_providers_.Take(); }

 private:
  base::test::TestFuture<VpnProviders> vpn_providers_;

  mojo::Remote<cros_network::mojom::CrosNetworkConfig> cros_network_config_;
};

IN_PROC_BROWSER_TEST_F(VpnExtensionObserverBrowserTest, LoadVpnExtension) {
  if (!HasLacrosArgument()) {
    return;
  }
  auto waiter = std::make_unique<ExtensionEventWaiter>();

  // Starts observing crosapi::VpnExtensionObserverAsh and
  // cros_network::mojom::CrosNetworkConfigObserver.
  waiter->StartObserving();

  // Send load extension request to Lacros.
  const std::string extension_id = LoadVpnExtension(kVpnExtensionName);

  // Should receive an event |OnVpnProvidersChanged| from
  // cros_network::mojom::CrosNetworkConfigObserver.
  auto vpn_providers = waiter->GetVpnProviders();

  // Find out extension among current vpn providers.
  auto itr =
      base::ranges::find_if(vpn_providers, [&](const auto& vpn_provider) {
        return vpn_provider->type == cros_network::mojom::VpnType::kExtension &&
               vpn_provider->app_id == extension_id;
      });

  // Ensure that it's there.
  ASSERT_NE(itr, std::end(vpn_providers));

  // Ensure that the name matches.
  ASSERT_EQ((*itr)->provider_name, kVpnExtensionName);
}
