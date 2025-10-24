// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_config_overlay.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/ssl_config.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::AllOf;
using ::testing::Not;

class MockSSLConfigClient : public network::mojom::SSLConfigClient {
 public:
  MockSSLConfigClient() = default;
  ~MockSSLConfigClient() override = default;

  void Bind(network::mojom::NetworkContextParamsPtr params) {
    receiver_.Bind(std::move(params->ssl_config_client_receiver));
    connected_ = true;
    receiver_.set_disconnect_handler(base::BindOnce(
        &MockSSLConfigClient::RecordDisconnection, base::Unretained(this)));
  }

  bool IsConnected() const { return connected_; }

  void RecordDisconnection() { connected_ = false; }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void,
              OnSSLConfigUpdated,
              (network::mojom::SSLConfigPtr),
              (override));

 private:
  mojo::Receiver<network::mojom::SSLConfigClient> receiver_{this};
  bool connected_ = false;
};

// Matcher that checks whether the SSLConfig was overridden in this test (we
// choose an arbitrary field to set to a non-default value).
MATCHER(WasOverridden, "") {
  return arg->tls13_cipher_prefer_aes_256;
}

// Matcher that checks whether the other arbitrary field used in this test to
// make SSLConfigs non-default has a non-default value.
MATCHER(HasGlobalNonDefaultSetting, "") {
  return !arg->ech_enabled;
}

class SSLConfigOverlayTest : public ::testing::Test {
 public:
  SSLConfigOverlayTest() = default;
  ~SSLConfigOverlayTest() override = default;

  // Sets up the NetworkContextParams with a default SSLConfig and pending
  // receiver, mimicking SSLConfigServiceManager::AddToNetworkContextParams().
  void SetUpNetworkContextParams(
      network::mojom::NetworkContextParams* network_context_params) {
    network_context_params->initial_ssl_config =
        network::mojom::SSLConfig::New();
    EXPECT_THAT(network_context_params->initial_ssl_config,
                Not(WasOverridden()));
    mojo::Remote<network::mojom::SSLConfigClient> client_remote;
    network_context_params->ssl_config_client_receiver =
        client_remote.BindNewPipeAndPassReceiver();
    clients_.Add(std::move(client_remote));
  }

  void BroadcastGlobalUpdate(network::mojom::SSLConfigPtr new_config) {
    for (const auto& client : clients_) {
      // Mojo calls consume all InterfacePtrs passed to them, so have to
      // clone the config for each call.
      client->OnSSLConfigUpdated(new_config->Clone());
    }
    clients_.FlushForTesting();
  }

  SSLConfigOverlay::OverrideConfigCallback GetOverrideCallback() const {
    return base::BindLambdaForTesting([](network::mojom::SSLConfig* config) {
      // Set an arbitrary bit of the config as the override, so if this field
      // is true we can tell that the override worked.
      config->tls13_cipher_prefer_aes_256 = true;
    });
  }

  // Set a different arbitrary field to a non-default value for tests that need
  // to mark the (global) config.
  void SetGlobalNonDefaultField(network::mojom::SSLConfig* config) {
    ASSERT_TRUE(config->ech_enabled);
    config->ech_enabled = false;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  // Each of these should get hooked up to an SSLConfigOverlay.
  mojo::RemoteSet<network::mojom::SSLConfigClient> clients_;
};

TEST_F(SSLConfigOverlayTest, InitAndOverride) {
  auto network_context_params = network::mojom::NetworkContextParams::New();
  SetUpNetworkContextParams(network_context_params.get());
  SetGlobalNonDefaultField(network_context_params->initial_ssl_config.get());
  EXPECT_THAT(network_context_params->initial_ssl_config,
              AllOf(Not(WasOverridden()), HasGlobalNonDefaultSetting()));

  SSLConfigOverlay overlay(GetOverrideCallback());
  EXPECT_TRUE(overlay.Init(network_context_params.get()));

  // The overlay connected its Mojo pipes.
  EXPECT_TRUE(overlay.IsBound());
  // The initial config picked up the override settings, and pre-existing
  // values in the global settings were preserved.
  EXPECT_THAT(network_context_params->initial_ssl_config,
              AllOf(WasOverridden(), HasGlobalNonDefaultSetting()));

  // Hook up the downstream client. This mimics creating the NetworkContext from
  // params.
  MockSSLConfigClient client;
  client.Bind(std::move(network_context_params));

  // Check that the downstream client is notified when the overlay is updated.
  EXPECT_CALL(client, OnSSLConfigUpdated(WasOverridden()));
  overlay.Update();
  overlay.FlushForTesting();
}

TEST_F(SSLConfigOverlayTest, GlobalUpdate) {
  auto network_context_params = network::mojom::NetworkContextParams::New();
  SetUpNetworkContextParams(network_context_params.get());

  SSLConfigOverlay overlay(GetOverrideCallback());
  EXPECT_TRUE(overlay.Init(network_context_params.get()));

  MockSSLConfigClient client;
  client.Bind(std::move(network_context_params));

  // Broadcast a global update and check that the downstream client gets it.
  auto new_config = network::mojom::SSLConfig::New();
  // Set an arbitrary field of the config so we can tell it apart from the
  // old config.
  SetGlobalNonDefaultField(new_config.get());
  EXPECT_THAT(new_config,
              AllOf(HasGlobalNonDefaultSetting(), Not(WasOverridden())));

  EXPECT_CALL(client, OnSSLConfigUpdated(AllOf(HasGlobalNonDefaultSetting(),
                                               WasOverridden())));
  BroadcastGlobalUpdate(std::move(new_config));
  overlay.FlushForTesting();
}

TEST_F(SSLConfigOverlayTest, OverrideUpdateUsesLastGlobalConfig) {
  auto network_context_params = network::mojom::NetworkContextParams::New();
  SetUpNetworkContextParams(network_context_params.get());

  SSLConfigOverlay overlay(GetOverrideCallback());
  EXPECT_TRUE(overlay.Init(network_context_params.get()));

  MockSSLConfigClient client;
  client.Bind(std::move(network_context_params));

  // Broadcast a global update so the overlay caches the latest global config,
  // then update the override settings.
  auto new_config = network::mojom::SSLConfig::New();
  // Set an arbitrary field of the config so we can tell it apart from the
  // original config.
  SetGlobalNonDefaultField(new_config.get());
  EXPECT_THAT(new_config,
              AllOf(HasGlobalNonDefaultSetting(), Not(WasOverridden())));

  // Both updates (one for the global update, and one for the override update)
  // should derive from the latest global config.
  EXPECT_CALL(client, OnSSLConfigUpdated(
                          AllOf(HasGlobalNonDefaultSetting(), WasOverridden())))
      .Times(2);

  BroadcastGlobalUpdate(std::move(new_config));
  overlay.Update();
  overlay.FlushForTesting();
}

TEST_F(SSLConfigOverlayTest, MultipleOverlaysReceiveGlobalUpdate) {
  auto params1 = network::mojom::NetworkContextParams::New();
  auto params2 = network::mojom::NetworkContextParams::New();
  SetUpNetworkContextParams(params1.get());
  SetUpNetworkContextParams(params2.get());

  SSLConfigOverlay overlay1{GetOverrideCallback()};
  EXPECT_TRUE(overlay1.Init(params1.get()));

  SSLConfigOverlay overlay2{GetOverrideCallback()};
  EXPECT_TRUE(overlay2.Init(params2.get()));

  MockSSLConfigClient client1, client2;
  client1.Bind(std::move(params1));
  client2.Bind(std::move(params2));

  // Broadcast a global update and check that both downstream clients get it.
  auto new_config = network::mojom::SSLConfig::New();
  EXPECT_THAT(new_config, Not(WasOverridden()));
  EXPECT_CALL(client1, OnSSLConfigUpdated(WasOverridden()));
  EXPECT_CALL(client2, OnSSLConfigUpdated(WasOverridden()));
  BroadcastGlobalUpdate(std::move(new_config));
  overlay1.FlushForTesting();
  overlay2.FlushForTesting();
}

TEST_F(SSLConfigOverlayTest, MultipleOverlaysIndividuallyUpdateable) {
  auto params1 = network::mojom::NetworkContextParams::New();
  auto params2 = network::mojom::NetworkContextParams::New();
  SetUpNetworkContextParams(params1.get());
  SetUpNetworkContextParams(params2.get());

  SSLConfigOverlay overlay1{GetOverrideCallback()};
  EXPECT_TRUE(overlay1.Init(params1.get()));

  SSLConfigOverlay overlay2{GetOverrideCallback()};
  EXPECT_TRUE(overlay2.Init(params2.get()));

  MockSSLConfigClient client1, client2;
  client1.Bind(std::move(params1));
  client2.Bind(std::move(params2));

  // Overlays can be updated individually for changes in the specific override
  // settings.
  EXPECT_CALL(client1, OnSSLConfigUpdated(WasOverridden()));
  EXPECT_CALL(client2, OnSSLConfigUpdated(WasOverridden())).Times(0);
  overlay1.Update();
  overlay1.FlushForTesting();
  overlay2.FlushForTesting();
}

TEST_F(SSLConfigOverlayTest, InitNoClient) {
  auto network_context_params = network::mojom::NetworkContextParams::New();
  SetUpNetworkContextParams(network_context_params.get());
  network_context_params->ssl_config_client_receiver.reset();

  SSLConfigOverlay overlay(GetOverrideCallback());
  EXPECT_FALSE(overlay.Init(network_context_params.get()));
  EXPECT_FALSE(overlay.IsBound());
}

TEST_F(SSLConfigOverlayTest, DisconnectReceiver) {
  auto network_context_params = network::mojom::NetworkContextParams::New();
  SetUpNetworkContextParams(network_context_params.get());

  SSLConfigOverlay overlay(GetOverrideCallback());
  EXPECT_TRUE(overlay.Init(network_context_params.get()));
  EXPECT_TRUE(overlay.IsBound());

  MockSSLConfigClient client;
  client.Bind(std::move(network_context_params));

  // Disconnect the overlay's receiver pipe.
  clients_.Clear();
  clients_.FlushForTesting();

  overlay.FlushForTesting();
  EXPECT_FALSE(overlay.IsBound());

  // The other end also got the message.
  client.FlushForTesting();
  EXPECT_FALSE(client.IsConnected());
}

TEST_F(SSLConfigOverlayTest, DisconnectRemote) {
  auto network_context_params = network::mojom::NetworkContextParams::New();
  SetUpNetworkContextParams(network_context_params.get());

  SSLConfigOverlay overlay(GetOverrideCallback());
  EXPECT_TRUE(overlay.Init(network_context_params.get()));
  EXPECT_TRUE(overlay.IsBound());

  {
    MockSSLConfigClient client;
    client.Bind(std::move(network_context_params));
  }

  // The overlay loses its remote connection when `client` goes out of scope.
  overlay.FlushForTesting();
  EXPECT_FALSE(overlay.IsBound());

  // The other end also got the message.
  clients_.FlushForTesting();
  for (const auto& client : clients_) {
    EXPECT_FALSE(client.is_connected());
  }
}

}  // namespace
