// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/nearby/nearby_process_manager_impl.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/nearby/bluetooth_adapter_manager.h"
#include "chrome/browser/chromeos/nearby/nearby_connections_dependencies_provider.h"
#include "chrome/browser/chromeos/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/services/sharing/nearby/test_support/fake_adapter.h"
#include "chrome/services/sharing/nearby/test_support/mock_webrtc_dependencies.h"
#include "chromeos/services/nearby/public/cpp/mock_nearby_connections.h"
#include "chromeos/services/nearby/public/cpp/mock_nearby_sharing_decoder.h"
#include "chromeos/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "chromeos/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "chromeos/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "chromeos/services/nearby/public/mojom/sharing.mojom.h"
#include "chromeos/services/nearby/public/mojom/webrtc.mojom.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
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
  }

 private:
  // mojom::Sharing:
  void Connect(
      location::nearby::connections::mojom::NearbyConnectionsDependenciesPtr
          deps,
      mojo::PendingReceiver<NearbyConnectionsMojom> connections_receiver,
      mojo::PendingReceiver<sharing::mojom::NearbySharingDecoder>
          decoder_receiver) override {
    EXPECT_FALSE(mock_connections_);
    EXPECT_FALSE(mock_decoder_);

    mock_connections_ =
        std::make_unique<chromeos::nearby::MockNearbyConnections>();
    mock_connections_->BindInterface(std::move(connections_receiver));

    mock_decoder_ =
        std::make_unique<chromeos::nearby::MockNearbySharingDecoder>();
    mock_decoder_->BindInterface(std::move(decoder_receiver));
  }

  void ShutDown(ShutDownCallback callback) override {
    mock_connections_.reset();
    mock_decoder_.reset();
    std::move(callback).Run();
  }

  std::unique_ptr<chromeos::nearby::MockNearbyConnections> mock_connections_;
  std::unique_ptr<chromeos::nearby::MockNearbySharingDecoder> mock_decoder_;
  mojo::Receiver<sharing::mojom::Sharing> receiver_{this};
};

}  // namespace

class NearbyProcessManagerImplTest : public testing::Test {
 public:
  class FakeNearbyConnectionsDependenciesProvider
      : public NearbyConnectionsDependenciesProvider {
   public:
    FakeNearbyConnectionsDependenciesProvider() = default;
    ~FakeNearbyConnectionsDependenciesProvider() override = default;

    // NearbyConnectionsDependenciesProvider:
    location::nearby::connections::mojom::NearbyConnectionsDependenciesPtr
    GetDependencies() override {
      fake_adapter_ = std::make_unique<bluetooth::FakeAdapter>();
      webrtc_dependencies_ =
          std::make_unique<sharing::MockWebRtcDependencies>();

      return location::nearby::connections::mojom::
          NearbyConnectionsDependencies::New(
              fake_adapter_->adapter_.BindNewPipeAndPassRemote(),
              location::nearby::connections::mojom::WebRtcDependencies::New(
                  webrtc_dependencies_->socket_manager_
                      .BindNewPipeAndPassRemote(),
                  webrtc_dependencies_->mdns_responder_factory_
                      .BindNewPipeAndPassRemote(),
                  webrtc_dependencies_->ice_config_fetcher_
                      .BindNewPipeAndPassRemote(),
                  webrtc_dependencies_->messenger_.BindNewPipeAndPassRemote()),
              location::nearby::api::LogMessage::Severity::kInfo);
    }

    void PrepareForShutdown() override { prepare_for_shutdown_count_++; }

    int prepare_for_shutdown_count() { return prepare_for_shutdown_count_; }

   private:
    std::unique_ptr<bluetooth::FakeAdapter> fake_adapter_;
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

  FakeNearbyConnectionsDependenciesProvider* fake_deps_provider() {
    return &fake_deps_provider_;
  }

  void VerifyBound(
      const NearbyProcessManager::NearbyProcessReference* reference) {
    EXPECT_TRUE(GetImpl()->sharing_.is_bound());
    EXPECT_TRUE(reference->GetNearbyConnections().is_bound());
    EXPECT_TRUE(reference->GetNearbySharingDecoder().is_bound());
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
  FakeNearbyConnectionsDependenciesProvider fake_deps_provider_;

  std::unique_ptr<NearbyProcessManager> nearby_process_manager_;

  base::MockOneShotTimer* mock_timer_ = nullptr;
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
}  // namespace chromeos
