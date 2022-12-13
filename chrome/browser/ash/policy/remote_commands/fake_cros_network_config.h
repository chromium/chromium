// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_FAKE_CROS_NETWORK_CONFIG_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_FAKE_CROS_NETWORK_CONFIG_H_

#include <string>
#include <vector>

#include "chrome/browser/ash/policy/remote_commands/fake_cros_network_config_base.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"

namespace policy::test {

class NetworkBuilder;

// Fake implementation of `CrosNetworkConfig` that simply stores a list of
// networks that will be returned on each `GetNetworkStateList()` call.
class FakeCrosNetworkConfig : public FakeCrosNetworkConfigBase {
 public:
  FakeCrosNetworkConfig();
  FakeCrosNetworkConfig(const FakeCrosNetworkConfig&) = delete;
  FakeCrosNetworkConfig& operator=(const FakeCrosNetworkConfig&) = delete;
  ~FakeCrosNetworkConfig() override;

  void SetActiveNetworks(std::vector<NetworkBuilder> networks);
  void AddActiveNetwork(NetworkBuilder builder);

  void ClearActiveNetworks();

  // `FakeCrosNetworkConfigBase` implementation:
  // Warning: this implementation does *not* actually use the filter.
  // Instead, it always returns all networks added through `SetActiveNetworks()`
  // and `AddActiveNetwork()`.
  void GetNetworkStateList(
      chromeos::network_config::mojom::NetworkFilterPtr filter,
      GetNetworkStateListCallback callback) override;

 private:
  std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
      active_networks_;
};

// Same as `FakeCrosNetworkConfig` but will install itself as a singleton in
// its constructor (and uninstall itself in its destructor).
class ScopedFakeCrosNetworkConfig : public FakeCrosNetworkConfig {
 public:
  ScopedFakeCrosNetworkConfig();
  ~ScopedFakeCrosNetworkConfig() override;
};

// Helper class that creates a `NetworkStatePropertiesPtr`.
// It will auto-assign an unique guid to the network if none was provided.
class NetworkBuilder {
 public:
  explicit NetworkBuilder(chromeos::network_config::mojom::NetworkType type,
                          const std::string& guid = "");
  NetworkBuilder(NetworkBuilder&& other);
  NetworkBuilder& operator=(NetworkBuilder&&);
  NetworkBuilder(const NetworkBuilder& other);
  void operator=(const NetworkBuilder& other);

  ~NetworkBuilder();

  NetworkBuilder& SetOncSource(
      chromeos::network_config::mojom::OncSource source);

  chromeos::network_config::mojom::NetworkStatePropertiesPtr Build() const;

 private:
  chromeos::network_config::mojom::NetworkTypeStatePropertiesPtr
  CreateTypeStateForType(chromeos::network_config::mojom::NetworkType type);

  chromeos::network_config::mojom::NetworkStatePropertiesPtr network_ =
      chromeos::network_config::mojom::NetworkStateProperties::New();
};

}  // namespace policy::test

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_FAKE_CROS_NETWORK_CONFIG_H_
