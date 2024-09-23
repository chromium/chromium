// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/nearby_process_manager_impl.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ash/nearby/bluetooth_adapter_manager.h"
#include "chrome/browser/ash/nearby/nearby_dependencies_provider.h"
#include "chrome/browser/ash/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "chrome/services/sharing/nearby/test_support/fake_nearby_presence_credential_storage.h"
#include "chrome/services/sharing/nearby/test_support/mock_webrtc_dependencies.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_firewall_hole_factory.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_mdns_manager.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_nearby_presence.h"
#include "chromeos/ash/services/nearby/public/cpp/fake_tcp_socket_factory.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_connections.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_nearby_sharing_decoder.h"
#include "chromeos/ash/services/nearby/public/cpp/mock_quick_start_decoder.h"
#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/mdns.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/sharing.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/tcp_socket_factory.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/webrtc.mojom.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Noop implementation for tests.
class FakeWifiDirectManager
    : public ash::wifi_direct::mojom::WifiDirectManager {
  // ash::wifi_direct::mojom::WifiDirectManager
  void CreateWifiDirectGroup(
      ash::wifi_direct::mojom::WifiCredentialsPtr credentials,
      CreateWifiDirectGroupCallback callback) override {
    // Noop
  }
  void ConnectToWifiDirectGroup(
      ash::wifi_direct::mojom::WifiCredentialsPtr credentials,
      std::optional<uint32_t> frequency,
      ConnectToWifiDirectGroupCallback callback) override {
    // Noop
  }
  void GetWifiP2PCapabilities(
      GetWifiP2PCapabilitiesCallback callback) override {
    // Noop
  }
};

}  // namespace

namespace ash {
namespace nearby {
namespace {

class FakeSharingMojoService : public sharing::mojom::Sharing {
 public:
  FakeSharingMojoService() = default;
  ~FakeSharingMojoService() override = default;

  bool AreMocksSet() const { return mock_connections_ && mock_decoder_; }

  mojo::PendingRemote<sharing::mojom::Sharing> BindSharingService() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void Reset() {
    receiver_.reset();
    mock_connections_.reset();
    mock_decoder_.reset();
    mock_quick_start_decoder_.reset();
  }

 private:
  // mojom::Sharing:
  void Connect(
      sharing::mojom::NearbyDependenciesPtr deps,
      mojo::PendingReceiver<NearbyConnectionsMojom> connections_receiver,
      mojo::PendingReceiver<NearbyPresenceMojom> presence_receiver,
      mojo::PendingReceiver<sharing::mojom::NearbySharingDecoder>
          decoder_receiver,
      mojo::PendingReceiver<ash::quick_start::mojom::QuickStartDecoder>
          quick_start_decoder_receiver) override {
    EXPECT_FALSE(mock_connections_);
    EXPECT_FALSE(fake_presence_);
    EXPECT_FALSE(mock_decoder_);
    EXPECT_FALSE(mock_quick_start_decoder_);

    mock_connections_ = std::make_unique<MockNearbyConnections>();
    mock_connections_->BindInterface(std::move(connections_receiver));

    fake_presence_ = std::make_unique<presence::FakeNearbyPresence>();
    fake_presence_->BindInterface(std::move(presence_receiver));

    mock_decoder_ = std::make_unique<MockNearbySharingDecoder>();
    mock_decoder_->BindInterface(std::move(decoder_receiver));

    mock_quick_start_decoder_ = std::make_unique<MockQuickStartDecoder>();
    mock_quick_start_decoder_->BindInterface(
        std::move(quick_start_decoder_receiver));
  }

  void ShutDown(ShutDownCallback callback) override {
    mock_connections_.reset();
    fake_presence_.reset();
    mock_decoder_.reset();
    mock_quick_start_decoder_.reset();
    std::move(callback).Run();
  }

  std::unique_ptr<MockNearbyConnections> mock_connections_;
  std::unique_ptr<presence::FakeNearbyPresence> fake_presence_;
  std::unique_ptr<MockNearbySharingDecoder> mock_decoder_;
  std::unique_ptr<MockQuickStartDecoder> mock_quick_start_decoder_;
  mojo::Receiver<sharing::mojom::Sharing> receiver_{this};
};

}  // namespace

class NearbyProcessManagerImplTest : public testing::Test {
 public:
  class FakeNearbyDependenciesProvider : public NearbyDependenciesProvider {
   public:
    FakeNearbyDependenciesProvider() = default;
    ~FakeNearbyDependenciesProvider() override = default;

    // NearbyDependenciesProvider:
    sharing::mojom::NearbyDependenciesPtr GetDependencies() override {
      fake_adapter_ = std::make_unique<bluetooth::FakeAdapter>();
      fake_nearby_presence_credential_storage_ =
          std::make_unique<presence::FakeNearbyPresenceCredentialStorage>();
      webrtc_dependencies_ =
          std::make_unique<sharing::MockWebRtcDependencies>();

      // Set up CrosNetworkConfig mojo service.
      mojo::PendingRemote<chromeos::network_config::mojom::CrosNetworkConfig>
          cros_network_config_remote;
      ash::GetNetworkConfigService(
          cros_network_config_remote.InitWithNewPipeAndPassReceiver());

      // Set up firewall hole factory mojo service.
      mojo::PendingRemote<sharing::mojom::FirewallHoleFactory>
          firewall_hole_factory_remote;
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<ash::nearby::FakeFirewallHoleFactory>(),
          firewall_hole_factory_remote.InitWithNewPipeAndPassReceiver());

      // Set up TCP socket factory mojo service.
      mojo::PendingRemote<sharing::mojom::TcpSocketFactory>
          tcp_socket_factory_remote;
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<ash::nearby::FakeTcpSocketFactory>(
              /*default_local_addr=*/net::IPEndPoint(
                  net::IPAddress(192, 168, 86, 75), 44444)),
          tcp_socket_factory_remote.InitWithNewPipeAndPassReceiver());

      // Set up Mdns Manager mojo service.
      mojo::PendingRemote<sharing::mojom::MdnsManager> mdns_manager_remote;
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<ash::nearby::FakeMdnsManager>(),
          mdns_manager_remote.InitWithNewPipeAndPassReceiver());

      // Set up fake WiFiDirect mojo services.
      mojo::PendingRemote<ash::wifi_direct::mojom::WifiDirectManager>
          wifi_direct_manager_remote;
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<FakeWifiDirectManager>(),
          wifi_direct_manager_remote.InitWithNewPipeAndPassReceiver());
      mojo::PendingRemote<::sharing::mojom::FirewallHoleFactory>
          wifi_direct_firewall_hole_factory_remote;
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<ash::nearby::FakeFirewallHoleFactory>(),
          wifi_direct_firewall_hole_factory_remote
              .InitWithNewPipeAndPassReceiver());

      return sharing::mojom::NearbyDependencies::New(
          fake_adapter_->adapter_.BindNewPipeAndPassRemote(),
          sharing::mojom::WebRtcDependencies::New(
              webrtc_dependencies_->socket_manager_.BindNewPipeAndPassRemote(),
              webrtc_dependencies_->mdns_responder_factory_
                  .BindNewPipeAndPassRemote(),
              webrtc_dependencies_->ice_config_fetcher_
                  .BindNewPipeAndPassRemote(),
              webrtc_dependencies_->messenger_.BindNewPipeAndPassRemote()),
          sharing::mojom::WifiLanDependencies::New(
              std::move(cros_network_config_remote),
              std::move(firewall_hole_factory_remote),
              std::move(tcp_socket_factory_remote),
              std::move(mdns_manager_remote)),
          ::sharing::mojom::WifiDirectDependencies::New(
              std::move(wifi_direct_manager_remote),
              std::move(wifi_direct_firewall_hole_factory_remote)),
          fake_nearby_presence_credential_storage_->receiver()
              .BindNewPipeAndPassRemote(),
          ::nearby::api::LogMessage::Severity::kInfo);
    }

    void PrepareForShutdown() override { prepare_for_shutdown_count_++; }

    int prepare_for_shutdown_count() { return prepare_for_shutdown_count_; }

   private:
    network_config::CrosNetworkConfigTestHelper
        cros_network_config_test_helper_;
    std::unique_ptr<bluetooth::FakeAdapter> fake_adapter_;
    std::unique_ptr<presence::FakeNearbyPresenceCredentialStorage>
        fake_nearby_presence_credential_storage_;
    std::unique_ptr<sharing::MockWebRtcDependencies> webrtc_dependencies_;
    int prepare_for_shutdown_count_ = 0;
  };

  NearbyProcessManagerImplTest() = default;
  ~NearbyProcessManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    auto mock_timer = std::make_unique<base::MockOneShotTimer>();
    mock_timer_ = mock_timer.get();

    nearby_process_manager_ = base::WrapUnique(new NearbyProcessManagerImpl(
        &fake_deps_provider_, std::move(mock_timer),
        base::BindRepeating(&FakeSharingMojoService::BindSharingService,
                            base::Unretained(&fake_sharing_mojo_service_))));
  }

  std::unique_ptr<NearbyProcessManager::NearbyProcessReference>
  CreateReference() {
    auto reference = nearby_process_manager_->GetNearbyProcessReference(
        base::BindOnce(&NearbyProcessManagerImplTest::OnProcessStopped,
                       base::Unretained(this)));
    GetImpl()->sharing_.FlushForTesting();
    return reference;
  }

  FakeSharingMojoService* fake_sharing_mojo_service() {
    return &fake_sharing_mojo_service_;
  }

  size_t num_process_stopped_calls() const {
    return num_process_stopped_calls_;
  }

  FakeNearbyDependenciesProvider* fake_deps_provider() {
    return &fake_deps_provider_;
  }

  void VerifyBound(
      const NearbyProcessManager::NearbyProcessReference* reference) {
    EXPECT_TRUE(GetImpl()->sharing_.is_bound());
    EXPECT_TRUE(reference->GetNearbyConnections().is_bound());
    EXPECT_TRUE(reference->GetNearbyPresence().is_bound());
    EXPECT_TRUE(reference->GetNearbySharingDecoder().is_bound());
    EXPECT_TRUE(reference->GetQuickStartDecoder().is_bound());
    EXPECT_TRUE(fake_sharing_mojo_service_.AreMocksSet());
  }

  void VerifyNotBound() {
    EXPECT_FALSE(GetImpl()->sharing_.is_bound());
    EXPECT_FALSE(fake_sharing_mojo_service_.AreMocksSet());
  }

  bool IsTimerRunning() const { return mock_timer_->IsRunning(); }

  void FireTimer() { mock_timer_->Fire(); }

 private:
  NearbyProcessManagerImpl* GetImpl() {
    return static_cast<NearbyProcessManagerImpl*>(
        nearby_process_manager_.get());
  }

  void OnProcessStopped(NearbyProcessManager::NearbyProcessShutdownReason) {
    ++num_process_stopped_calls_;
  }

  const base::test::TaskEnvironment task_environment_;
  size_t num_process_stopped_calls_ = 0u;

  FakeSharingMojoService fake_sharing_mojo_service_;
  FakeNearbyDependenciesProvider fake_deps_provider_;

  std::unique_ptr<NearbyProcessManager> nearby_process_manager_;

  raw_ptr<base::MockOneShotTimer> mock_timer_ = nullptr;
};

TEST_F(NearbyProcessManagerImplTest, StartAndStop) {
  std::unique_ptr<NearbyProcessManager::NearbyProcessReference> reference =
      CreateReference();
  VerifyBound(reference.get());

  reference.reset();
  FireTimer();
  base::RunLoop().RunUntilIdle();
  VerifyNotBound();
  EXPECT_EQ(0u, num_process_stopped_calls());
  EXPECT_EQ(1, fake_deps_provider()->prepare_for_shutdown_count());

  // Reset, then repeat process to verify it still works with multiple tries.
  fake_sharing_mojo_service()->Reset();

  reference = CreateReference();
  VerifyBound(reference.get());

  reference.reset();
  FireTimer();
  base::RunLoop().RunUntilIdle();
  VerifyNotBound();
  EXPECT_EQ(0u, num_process_stopped_calls());
  EXPECT_EQ(2, fake_deps_provider()->prepare_for_shutdown_count());
}

TEST_F(NearbyProcessManagerImplTest, MultipleReferences) {
  std::unique_ptr<NearbyProcessManager::NearbyProcessReference> reference1 =
      CreateReference();
  std::unique_ptr<NearbyProcessManager::NearbyProcessReference> reference2 =
      CreateReference();
  VerifyBound(reference1.get());
  VerifyBound(reference2.get());

  // Deleting one reference should still keep the other reference bound.
  reference1.reset();
  EXPECT_FALSE(IsTimerRunning());
  base::RunLoop().RunUntilIdle();
  VerifyBound(reference2.get());

  reference2.reset();
  FireTimer();
  base::RunLoop().RunUntilIdle();
  VerifyNotBound();
  EXPECT_EQ(0u, num_process_stopped_calls());
  EXPECT_EQ(1, fake_deps_provider()->prepare_for_shutdown_count());
}

TEST_F(NearbyProcessManagerImplTest, ProcessStopped) {
  std::unique_ptr<NearbyProcessManager::NearbyProcessReference> reference =
      CreateReference();
  VerifyBound(reference.get());

  // Reset, then wait for the disconnection to propagate.
  fake_sharing_mojo_service()->Reset();
  base::RunLoop().RunUntilIdle();

  VerifyNotBound();
  EXPECT_EQ(1u, num_process_stopped_calls());
  EXPECT_FALSE(IsTimerRunning());
}

TEST_F(NearbyProcessManagerImplTest,
       NewReferenceObtainedWhileWaitingToShutDown) {
  std::unique_ptr<NearbyProcessManager::NearbyProcessReference> reference =
      CreateReference();
  VerifyBound(reference.get());

  // Delete the reference; the timer should be running so that the process is
  // shut down after the cleanup timeout.
  reference.reset();
  EXPECT_TRUE(IsTimerRunning());

  // Obtain a new reference; the timer should have stopped.
  reference = CreateReference();
  VerifyBound(reference.get());
  EXPECT_FALSE(IsTimerRunning());

  // Delete the reference and let the timer fire to shut down the process.
  reference.reset();
  FireTimer();
  base::RunLoop().RunUntilIdle();
  VerifyNotBound();
  EXPECT_EQ(0u, num_process_stopped_calls());
}

}  // namespace nearby
}  // namespace ash
