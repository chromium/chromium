// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/network_config_service.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_engine_stopped_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/sync_wifi/network_identifier.h"
#include "chromeos/ash/components/sync_wifi/test_data_generator.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/protocol/wifi_configuration_specifics.pb.h"
#include "components/sync/test/fake_server.h"
#include "components/sync/test/nigori_test_utils.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

std::string GetClientTag(const sync_pb::WifiConfigurationSpecifics& specifics) {
  return ash::sync_wifi::NetworkIdentifier::FromProto(specifics)
      .SerializeToString();
}

void InjectKeystoreEncryptedServerWifiConfiguration(
    fake_server::FakeServer* fake_server,
    const sync_pb::WifiConfigurationSpecifics& unencrypted_specifics) {
  sync_pb::EntitySpecifics wrapped_unencrypted_specifics;
  *wrapped_unencrypted_specifics.mutable_wifi_configuration() =
      unencrypted_specifics;

  sync_pb::EntitySpecifics encrypted_specifics;
  const syncer::KeyParamsForTesting keystore_key_params =
      syncer::KeystoreKeyParamsForTesting(
          fake_server->GetKeystoreKeys().back());
  auto cryptographer = syncer::CryptographerImpl::FromSingleKeyForTesting(
      keystore_key_params.password, keystore_key_params.derivation_params);
  bool encrypt_result = cryptographer->Encrypt(
      wrapped_unencrypted_specifics, encrypted_specifics.mutable_encrypted());
  *encrypted_specifics.mutable_wifi_configuration() =
      sync_pb::WifiConfigurationSpecifics();
  DCHECK(encrypt_result);

  fake_server->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/"encrypted", GetClientTag(unencrypted_specifics),
          encrypted_specifics,
          /*creation_time=*/0, /*last_modified_time=*/0));
}

// Observes Shill networks state via IPC. Used to wait until wifi configuration
// becomes locally available.
class LocalWifiConfigurationChecker
    : public StatusChangeChecker,
      public chromeos::network_config::CrosNetworkConfigObserver {
 public:
  LocalWifiConfigurationChecker(
      mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>*
          remote_cros_network_config,
      const std::string& expected_ssid)
      : expected_ssid_(expected_ssid),
        remote_cros_network_config_(remote_cros_network_config) {
    DCHECK(remote_cros_network_config_);
    (*remote_cros_network_config_)
        ->AddObserver(receiver_.BindNewPipeAndPassRemote());
    OnNetworkStateListChanged();
  }

  LocalWifiConfigurationChecker(const LocalWifiConfigurationChecker& other) =
      delete;
  LocalWifiConfigurationChecker& operator=(
      const LocalWifiConfigurationChecker& other) = delete;
  ~LocalWifiConfigurationChecker() override = default;

  // CrosNetworkConfigObserver implementation.
  void OnNetworkStateListChanged() override {
    (*remote_cros_network_config_)
        ->GetNetworkStateList(
            chromeos::network_config::mojom::NetworkFilter::New(
                chromeos::network_config::mojom::FilterType::kConfigured,
                chromeos::network_config::mojom::NetworkType::kWiFi,
                /*limit=*/0),
            base::BindOnce(&LocalWifiConfigurationChecker::OnGetNetworkList,
                           weak_ptr_factory_.GetWeakPtr()));
  }

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for network with ssid: " << expected_ssid_;
    for (const auto& network : networks_) {
      if (network->type_state->get_wifi()->ssid == expected_ssid_) {
        return true;
      }
    }
    return false;
  }

  void OnGetNetworkList(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks) {
    networks_ = std::move(networks);
    CheckExitCondition();
  }

 private:
  const std::string expected_ssid_;
  std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
      networks_;

  raw_ptr<mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>>
      remote_cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      receiver_{this};
  base::WeakPtrFactory<LocalWifiConfigurationChecker> weak_ptr_factory_{this};
};

// TODO(crbug.com/1077152): add more tests.
class SingleClientWifiConfigurationSyncTest : public SyncTest {
 public:
  SingleClientWifiConfigurationSyncTest() : SyncTest(SINGLE_CLIENT) {}
  SingleClientWifiConfigurationSyncTest(
      const SingleClientWifiConfigurationSyncTest&) = delete;
  SingleClientWifiConfigurationSyncTest& operator=(
      const SingleClientWifiConfigurationSyncTest&) = delete;
  ~SingleClientWifiConfigurationSyncTest() override = default;

  void SetUpOnMainThread() override {
    ash::GetNetworkConfigService(
        remote_cros_network_config_.BindNewPipeAndPassReceiver());

    SyncTest::SetUpOnMainThread();
  }

  void SetupShill() {
    // TODO(crbug.com/1077152): figure out where |userhash| is hardcoded and use
    // some shared constant here.
    ash::ShillProfileClient::Get()->GetTestInterface()->AddProfile(
        GetProfile(0)->GetPath().value(), /*userhash=*/"test-user");
  }

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>*
  remote_cros_network_config() {
    return &remote_cros_network_config_;
  }

 private:
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
};

IN_PROC_BROWSER_TEST_F(SingleClientWifiConfigurationSyncTest,
                       ShouldDownloadSingleWifiConfiguration) {
  const std::string kTestSsid = "test_wifi";
  InjectKeystoreEncryptedServerWifiConfiguration(
      GetFakeServer(),
      /*unencrypted_specifics=*/ash::sync_wifi::GenerateTestWifiSpecifics(
          ash::sync_wifi::GeneratePskNetworkId(kTestSsid)));

  ASSERT_TRUE(SetupSync());
  SetupShill();

  EXPECT_TRUE(
      LocalWifiConfigurationChecker(remote_cros_network_config(), kTestSsid)
          .Wait());
}

// Regression test for crbug.com/1318390: the client should clear metadata when
// sync requires it and perform initial sync again (was crashing before the
// fix).
IN_PROC_BROWSER_TEST_F(SingleClientWifiConfigurationSyncTest,
                       ShouldHandleClientDataObsolete) {
  const std::string kTestSsid1 = "test_wifi";
  InjectKeystoreEncryptedServerWifiConfiguration(
      GetFakeServer(),
      /*unencrypted_specifics=*/ash::sync_wifi::GenerateTestWifiSpecifics(
          ash::sync_wifi::GeneratePskNetworkId(kTestSsid1)));

  ASSERT_TRUE(SetupSync());
  SetupShill();
  ASSERT_TRUE(
      LocalWifiConfigurationChecker(remote_cros_network_config(), kTestSsid1)
          .Wait());

  GetFakeServer()->TriggerError(sync_pb::SyncEnums::CLIENT_DATA_OBSOLETE);

  // Trigger sync by making one more change.
  const std::string kTestSsid2 = "test_wifi2";
  InjectKeystoreEncryptedServerWifiConfiguration(
      GetFakeServer(),
      /*unencrypted_specifics=*/ash::sync_wifi::GenerateTestWifiSpecifics(
          ash::sync_wifi::GeneratePskNetworkId(kTestSsid2)));
  ASSERT_TRUE(syncer::SyncEngineStoppedChecker(GetSyncService(0)).Wait());

  // Make server return SUCCESS so that sync can initialize.
  GetFakeServer()->TriggerError(sync_pb::SyncEnums::SUCCESS);
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());

  // Ensure client has both networks.
  EXPECT_TRUE(
      LocalWifiConfigurationChecker(remote_cros_network_config(), kTestSsid1)
          .Wait());
  EXPECT_TRUE(
      LocalWifiConfigurationChecker(remote_cros_network_config(), kTestSsid2)
          .Wait());
}

}  // namespace
