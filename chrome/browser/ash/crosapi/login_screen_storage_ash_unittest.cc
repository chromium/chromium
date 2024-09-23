// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/login_screen_storage_ash.h"

#include "base/test/test_future.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/crosapi/mojom/login_screen_storage.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArgs;

MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

namespace {

constexpr char kKey1[] = "prefix_aaaaa";
constexpr char kKey2[] = "prefix_bbbbb";

constexpr char kData[] = "data";
constexpr char kError[] = "error";

}  // namespace

namespace crosapi {

namespace {

void EvaluateRetrieveResult(
    mojom::LoginScreenStorageRetrieveResultPtr found,
    mojom::LoginScreenStorageRetrieveResultPtr expected) {
  ASSERT_EQ(expected->which(), found->which());
  if (expected->which() ==
      mojom::LoginScreenStorageRetrieveResult::Tag::kErrorMessage) {
    ASSERT_EQ(expected->get_error_message(), found->get_error_message());
  } else {
    ASSERT_EQ(expected->get_data(), found->get_data());
  }
}

void LoginScreenStorageStoreSuccess(
    ash::FakeSessionManagerClient::LoginScreenStorageStoreCallback callback) {
  std::move(callback).Run(/*error_message=*/std::nullopt);
}

void LoginScreenStorageStoreError(
    ash::FakeSessionManagerClient::LoginScreenStorageStoreCallback callback) {
  std::move(callback).Run(kError);
}

void LoginScreenStorageRetrieveSuccess(
    ash::FakeSessionManagerClient::LoginScreenStorageRetrieveCallback
        callback) {
  std::move(callback).Run(kData, /*error_message=*/std::nullopt);
}

void LoginScreenStorageRetrieveError(
    ash::FakeSessionManagerClient::LoginScreenStorageRetrieveCallback
        callback) {
  std::move(callback).Run(/*data=*/std::nullopt, kError);
}
}  // namespace

class LoginScreenStorageAshTest : public testing::Test {
 public:
  // A mock around FakeSessionManagerClient for tracking the D-Bus calls.
  class MockSessionManagerClient : public ash::FakeSessionManagerClient {
   public:
    MockSessionManagerClient() = default;
    ~MockSessionManagerClient() override = default;

    MOCK_METHOD4(LoginScreenStorageStore,
                 void(const std::string& key,
                      const login_manager::LoginScreenStorageMetadata& metadata,
                      const std::string& data,
                      LoginScreenStorageStoreCallback callback));

    MOCK_METHOD2(LoginScreenStorageRetrieve,
                 void(const std::string& key,
                      LoginScreenStorageRetrieveCallback callback));
  };

  LoginScreenStorageAshTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~LoginScreenStorageAshTest() override = default;

  void SetUp() override {
    login_screen_storage_ash_ = std::make_unique<LoginScreenStorageAsh>();
    login_screen_storage_ash_->BindReceiver(
        login_screen_storage_remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override { login_screen_storage_ash_.reset(); }

 protected:
  testing::StrictMock<MockSessionManagerClient> session_manager_client_;

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;

  mojo::Remote<mojom::LoginScreenStorage> login_screen_storage_remote_;
  std::unique_ptr<LoginScreenStorageAsh> login_screen_storage_ash_;
};

TEST_F(LoginScreenStorageAshTest, StorePersistentDataSuccess) {
  bool clear_on_session_exit = false;

  crosapi::mojom::LoginScreenStorageMetadataPtr metadata_mojo =
      crosapi::mojom::LoginScreenStorageMetadata::New();
  metadata_mojo->clear_on_session_exit = clear_on_session_exit;

  login_manager::LoginScreenStorageMetadata metadata_dbus;
  metadata_dbus.set_clear_on_session_exit(clear_on_session_exit);

  EXPECT_CALL(
      session_manager_client_,
      LoginScreenStorageStore(kKey1, EqualsProto(metadata_dbus), kData, _))
      .WillOnce(WithArgs<3>(Invoke(LoginScreenStorageStoreSuccess)));

  base::test::TestFuture<const std::optional<std::string>&> future;
  login_screen_storage_remote_->Store({kKey1}, std::move(metadata_mojo), kData,
                                      future.GetCallback());

  ASSERT_EQ(future.Take(), std::nullopt);
}

TEST_F(LoginScreenStorageAshTest, StoreNonPersistentDataSuccess) {
  bool clear_on_session_exit = true;

  crosapi::mojom::LoginScreenStorageMetadataPtr metadata_mojo =
      crosapi::mojom::LoginScreenStorageMetadata::New();
  metadata_mojo->clear_on_session_exit = clear_on_session_exit;

  login_manager::LoginScreenStorageMetadata metadata_dbus;
  metadata_dbus.set_clear_on_session_exit(clear_on_session_exit);

  EXPECT_CALL(
      session_manager_client_,
      LoginScreenStorageStore(kKey1, EqualsProto(metadata_dbus), kData, _))
      .WillOnce(WithArgs<3>(Invoke(LoginScreenStorageStoreSuccess)));

  base::test::TestFuture<const std::optional<std::string>&> future;
  login_screen_storage_remote_->Store({kKey1}, std::move(metadata_mojo), kData,
                                      future.GetCallback());

  ASSERT_EQ(future.Take(), std::nullopt);
}

TEST_F(LoginScreenStorageAshTest, StoreForMultipleKeysSuccess) {
  EXPECT_CALL(session_manager_client_,
              LoginScreenStorageStore(kKey1, _, kData, _))
      .WillOnce(WithArgs<3>(Invoke(LoginScreenStorageStoreSuccess)));
  EXPECT_CALL(session_manager_client_,
              LoginScreenStorageStore(kKey2, _, kData, _))
      .WillOnce(WithArgs<3>(Invoke(LoginScreenStorageStoreSuccess)));

  crosapi::mojom::LoginScreenStorageMetadataPtr metadata_mojo =
      crosapi::mojom::LoginScreenStorageMetadata::New();
  metadata_mojo->clear_on_session_exit = false;

  std::vector<std::string> keys = {kKey1, kKey2};
  base::test::TestFuture<const std::optional<std::string>&> future;
  login_screen_storage_remote_->Store(keys, std::move(metadata_mojo), kData,
                                      future.GetCallback());

  ASSERT_EQ(future.Take(), std::nullopt);
}

TEST_F(LoginScreenStorageAshTest, StoreError) {
  EXPECT_CALL(session_manager_client_, LoginScreenStorageStore(_, _, kData, _))
      .WillRepeatedly(WithArgs<3>(Invoke(LoginScreenStorageStoreError)));

  crosapi::mojom::LoginScreenStorageMetadataPtr metadata_mojo =
      crosapi::mojom::LoginScreenStorageMetadata::New();
  metadata_mojo->clear_on_session_exit = false;

  base::test::TestFuture<const std::optional<std::string>&> future;
  login_screen_storage_remote_->Store({kKey1, kKey2}, std::move(metadata_mojo),
                                      kData, future.GetCallback());

  ASSERT_EQ(future.Take(), kError);
}

TEST_F(LoginScreenStorageAshTest, RetrieveSuccess) {
  EXPECT_CALL(session_manager_client_, LoginScreenStorageRetrieve(kKey1, _))
      .WillOnce(WithArgs<1>(Invoke(LoginScreenStorageRetrieveSuccess)));

  base::test::TestFuture<mojom::LoginScreenStorageRetrieveResultPtr> future;
  login_screen_storage_remote_->Retrieve(kKey1, future.GetCallback());

  EvaluateRetrieveResult(
      future.Take(), mojom::LoginScreenStorageRetrieveResult::NewData(kData));
}

TEST_F(LoginScreenStorageAshTest, RetrieveError) {
  EXPECT_CALL(session_manager_client_, LoginScreenStorageRetrieve(kKey1, _))
      .WillOnce(WithArgs<1>(Invoke(LoginScreenStorageRetrieveError)));

  base::test::TestFuture<mojom::LoginScreenStorageRetrieveResultPtr> future;
  login_screen_storage_remote_->Retrieve(kKey1, future.GetCallback());

  EvaluateRetrieveResult(
      future.Take(),
      mojom::LoginScreenStorageRetrieveResult::NewErrorMessage(kError));
}

}  // namespace crosapi
