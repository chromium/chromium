// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/presence/credential_storage/nearby_presence_credential_storage.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/internal/proto/credential.pb.h"
#include "third_party/nearby/internal/proto/local_credential.pb.h"

namespace {

class TestNearbyPresenceCredentialStorage
    : public ash::nearby::presence::NearbyPresenceCredentialStorage {
 public:
  TestNearbyPresenceCredentialStorage(
      std::unique_ptr<leveldb_proto::ProtoDatabase<
          ::nearby::internal::LocalCredential>> private_db,
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<::nearby::internal::SharedCredential>>
          public_db)
      : ash::nearby::presence::NearbyPresenceCredentialStorage(
            std::move(private_db),
            std::move(public_db)) {}
};

}  // namespace

namespace ash::nearby::presence {

class NearbyPresenceCredentialStorageTest : public testing::Test {
 public:
  NearbyPresenceCredentialStorageTest() = default;

  ~NearbyPresenceCredentialStorageTest() override = default;

  // testing::Test:
  void SetUp() override {
    auto private_db = std::make_unique<
        leveldb_proto::test::FakeDB<::nearby::internal::LocalCredential>>(
        &private_db_entries_);
    auto public_db = std::make_unique<
        leveldb_proto::test::FakeDB<::nearby::internal::SharedCredential>>(
        &public_db_entries_);

    private_db_ = private_db.get();
    public_db_ = public_db.get();

    credential_storage_ = std::make_unique<TestNearbyPresenceCredentialStorage>(
        std::move(private_db), std::move(public_db));
  }

  void TearDown() override {
    // Reset the raw pointer to prevent a dangling pointer when
    // NearbyPresenceCredentialStorageTest is deconstructed.
    private_db_ = nullptr;
    public_db_ = nullptr;

    credential_storage_.reset();
  }

  void InitializeCredentialStorage(base::RunLoop& run_loop,
                                   bool expected_success) {
    credential_storage_->Initialize(
        base::BindLambdaForTesting([expected_success, &run_loop](bool success) {
          EXPECT_EQ(expected_success, success);
          run_loop.Quit();
        }));
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment;

  raw_ptr<leveldb_proto::test::FakeDB<::nearby::internal::LocalCredential>>
      private_db_;
  raw_ptr<leveldb_proto::test::FakeDB<::nearby::internal::SharedCredential>>
      public_db_;
  std::map<std::string, ::nearby::internal::LocalCredential>
      private_db_entries_;
  std::map<std::string, ::nearby::internal::SharedCredential>
      public_db_entries_;
  std::unique_ptr<NearbyPresenceCredentialStorage> credential_storage_;
};

TEST_F(NearbyPresenceCredentialStorageTest, InitializeDatabases_Successful) {
  base::RunLoop run_loop;

  InitializeCredentialStorage(run_loop, /*expected_success=*/true);

  private_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  public_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);

  run_loop.Run();
}

TEST_F(NearbyPresenceCredentialStorageTest, InitializeDatabases_PrivateFails) {
  base::RunLoop run_loop;

  InitializeCredentialStorage(run_loop, /*expected_success=*/false);

  // Only the private status callback is set, as the public callback will
  // never be bound.
  private_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kCorrupt);

  run_loop.Run();
}

TEST_F(NearbyPresenceCredentialStorageTest, InitializeDatabases_PublicFails) {
  base::RunLoop run_loop;

  InitializeCredentialStorage(run_loop, /*expected_success=*/false);

  private_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  public_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kCorrupt);

  run_loop.Run();
}

}  // namespace ash::nearby::presence
