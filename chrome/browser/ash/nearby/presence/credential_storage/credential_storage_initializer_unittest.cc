// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/presence/credential_storage/credential_storage_initializer.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestCredentialStorageInitializer
    : public ash::nearby::presence::CredentialStorageInitializer {
 public:
  TestCredentialStorageInitializer(
      std::unique_ptr<
          ash::nearby::presence::NearbyPresenceCredentialStorageBase>
          nearby_presence_credential_storage)
      : ash::nearby::presence::CredentialStorageInitializer(
            std::move(nearby_presence_credential_storage)) {}
};

class FakeNearbyPresenceCredentialStorage
    : public ash::nearby::presence::NearbyPresenceCredentialStorageBase {
 public:
  FakeNearbyPresenceCredentialStorage(
      mojo::PendingReceiver<
          ash::nearby::presence::mojom::NearbyPresenceCredentialStorage>
          pending_receiver)
      : pending_receiver_(std::move(pending_receiver)) {}

  // TODO(b/306205385): When `CredentialStorageInitializer` supports
  // reinitialization allow the callback to return false.
  void Initialize(base::OnceCallback<void(bool)> on_initialized) override {
    ++initialization_calls_count_;

    receiver_.Bind(std::move(pending_receiver_));
    std::move(on_initialized).Run(/*initialization_success=*/true);
  }

  // mojom::NearbyPresenceCredentialStorage empty overrides:
  void SaveCredentials(
      std::vector<ash::nearby::presence::mojom::LocalCredentialPtr>
          local_credentials,
      std::vector<ash::nearby::presence::mojom::SharedCredentialPtr>
          shared_credentials,
      ash::nearby::presence::mojom::PublicCredentialType public_credential_type,
      SaveCredentialsCallback on_credentials_fully_saved_callback) override {}
  void GetPublicCredentials(
      ash::nearby::presence::mojom::PublicCredentialType public_credential_type,
      GetPublicCredentialsCallback callback) override {}
  void GetPrivateCredentials(GetPrivateCredentialsCallback callback) override {}
  void UpdateLocalCredential(
      ash::nearby::presence::mojom::LocalCredentialPtr local_credential,
      UpdateLocalCredentialCallback callback) override {}

  int GetInitializationCallsCount() { return initialization_calls_count_; }

 private:
  mojo::Receiver<ash::nearby::presence::mojom::NearbyPresenceCredentialStorage>
      receiver_{this};

  // Only bound after a call to `Initialize()`.
  mojo::PendingReceiver<
      ash::nearby::presence::mojom::NearbyPresenceCredentialStorage>
      pending_receiver_;

  int initialization_calls_count_ = 0;
};

}  // namespace

namespace ash::nearby::presence {

class CredentialStorageInitializerTest : public testing::Test {
 public:
  CredentialStorageInitializerTest() = default;

  ~CredentialStorageInitializerTest() override = default;

  void SetUp() override {
    mojo::PendingReceiver<mojom::NearbyPresenceCredentialStorage>
        pending_receiver = nearby_presence_credential_storage_remote_
                               .BindNewPipeAndPassReceiver();

    auto nearby_presence_credential_storage =
        std::make_unique<FakeNearbyPresenceCredentialStorage>(
            std::move(pending_receiver));

    nearby_presence_credential_storage_ =
        nearby_presence_credential_storage.get();

    credential_storage_initializer_ =
        std::make_unique<TestCredentialStorageInitializer>(
            std::move(nearby_presence_credential_storage));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment;

  std::unique_ptr<TestCredentialStorageInitializer>
      credential_storage_initializer_;
  // `credential_storage_initializer_` owns and outlives
  // `nearby_presence_credential_storage_`.
  raw_ptr<FakeNearbyPresenceCredentialStorage>
      nearby_presence_credential_storage_;

  mojo::Remote<mojom::NearbyPresenceCredentialStorage>
      nearby_presence_credential_storage_remote_;

  base::HistogramTester histogram_tester_;
};

TEST_F(CredentialStorageInitializerTest, Initialize) {
  EXPECT_EQ(0,
            nearby_presence_credential_storage_->GetInitializationCallsCount());

  credential_storage_initializer_->Initialize();

  EXPECT_EQ(1,
            nearby_presence_credential_storage_->GetInitializationCallsCount());
  histogram_tester_.ExpectBucketCount(
      "Nearby.Presence.Credentials.Storage.Initialization.Result",
      /*bucket: success=*/true, 1);

  EXPECT_TRUE(nearby_presence_credential_storage_remote_.is_bound());
  EXPECT_TRUE(nearby_presence_credential_storage_remote_.is_connected());
}

}  // namespace ash::nearby::presence
