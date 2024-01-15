// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_storage/login_screen_storage_api.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArgs;

namespace {

constexpr char kExtensionName[] = "test extension";
constexpr char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";

constexpr char kExtensionId1[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kExtensionId2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

constexpr char kData[] = "data";
constexpr char kError[] = "error";

constexpr char kPersistentDataKeyPrefix[] = "persistent_data_";
constexpr char kCredentialsKeyPrefix[] = "credentials_";

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

}  // namespace

namespace extensions {

class LoginScreenStorageApiUnittest : public ExtensionApiUnittest {
 public:
  LoginScreenStorageApiUnittest() {}

  LoginScreenStorageApiUnittest(const LoginScreenStorageApiUnittest&) = delete;
  LoginScreenStorageApiUnittest& operator=(
      const LoginScreenStorageApiUnittest&) = delete;

  ~LoginScreenStorageApiUnittest() override = default;

  void SetUp() override {
    ExtensionApiUnittest::SetUp();

    scoped_refptr<const Extension> extension =
        ExtensionBuilder(kExtensionName).SetID(kExtensionId).Build();
    set_extension(extension);
  }

 protected:
  testing::StrictMock<MockSessionManagerClient> session_manager_client_;
};

TEST_F(LoginScreenStorageApiUnittest, StorePersistentDataSuccess) {
  const std::string expected_key1 = base::StrCat(
      {kPersistentDataKeyPrefix, kExtensionId, "_", kExtensionId1});
  const std::string expected_key2 = base::StrCat(
      {kPersistentDataKeyPrefix, kExtensionId, "_", kExtensionId2});
  EXPECT_CALL(session_manager_client_,
              LoginScreenStorageStore(expected_key1, _, kData, _))
      .WillOnce(WithArgs<3>(Invoke(LoginScreenStorageStoreSuccess)));
  EXPECT_CALL(session_manager_client_,
              LoginScreenStorageStore(expected_key2, _, kData, _))
      .WillOnce(WithArgs<3>(Invoke(LoginScreenStorageStoreSuccess)));

  auto function =
      base::MakeRefCounted<LoginScreenStorageStorePersistentDataFunction>();
  std::string args = base::StringPrintf(R"([["%s", "%s"],  "%s"])",
                                        kExtensionId1, kExtensionId2, kData);
  EXPECT_FALSE(RunFunctionAndReturnValue(function.get(), args));
}

TEST_F(LoginScreenStorageApiUnittest, StorePersistentDataError) {
  const std::string expected_key = base::StrCat(
      {kPersistentDataKeyPrefix, kExtensionId, "_", kExtensionId2});
  EXPECT_CALL(session_manager_client_,
              LoginScreenStorageStore(expected_key, _, kData, _))
      .WillRepeatedly(WithArgs<3>(Invoke(LoginScreenStorageStoreError)));

  auto function =
      base::MakeRefCounted<LoginScreenStorageStorePersistentDataFunction>();
  std::string args = base::StringPrintf(R"([["%s", "%s"],  "%s"])",
                                        kExtensionId1, kExtensionId2, kData);
  EXPECT_EQ(kError, RunFunctionAndReturnError(function.get(), args));
}

TEST_F(LoginScreenStorageApiUnittest, RetrievePersistentDataSuccess) {
  const std::string expected_key = base::StrCat(
      {kPersistentDataKeyPrefix, kExtensionId1, "_", kExtensionId});
  EXPECT_CALL(session_manager_client_,
              LoginScreenStorageRetrieve(expected_key, _))
      .WillOnce(WithArgs<1>(Invoke(LoginScreenStorageRetrieveSuccess)));

  auto function =
      base::MakeRefCounted<LoginScreenStorageRetrievePersistentDataFunction>();
  std::string args = base::StringPrintf(R"(["%s"])", kExtensionId1);
  EXPECT_EQ(kData,
            RunFunctionAndReturnValue(function.get(), args)->GetString());
}

TEST_F(LoginScreenStorageApiUnittest, RetrievePersistentDataError) {
  const std::string expected_key = base::StrCat(
      {kPersistentDataKeyPrefix, kExtensionId1, "_", kExtensionId});
  EXPECT_CALL(session_manager_client_,
              LoginScreenStorageRetrieve(expected_key, _))
      .WillOnce(WithArgs<1>(Invoke(LoginScreenStorageRetrieveError)));

  auto function =
      base::MakeRefCounted<LoginScreenStorageRetrievePersistentDataFunction>();
  std::string args = base::StringPrintf(R"(["%s"])", kExtensionId1);
  EXPECT_EQ(kError, RunFunctionAndReturnError(function.get(), args));
}

TEST_F(LoginScreenStorageApiUnittest, StoreCredentialsSuccess) {
  const std::string expected_key =
      base::StrCat({kCredentialsKeyPrefix, kExtensionId1});
  EXPECT_CALL(session_manager_client_,
              LoginScreenStorageStore(expected_key, _, kData, _))
      .WillOnce(WithArgs<3>(Invoke(LoginScreenStorageStoreSuccess)));

  auto function =
      base::MakeRefCounted<LoginScreenStorageStoreCredentialsFunction>();
  std::string args =
      base::StringPrintf(R"(["%s", "%s"])", kExtensionId1, kData);
  EXPECT_FALSE(RunFunctionAndReturnValue(function.get(), args));
}

TEST_F(LoginScreenStorageApiUnittest, StoreCredentialsError) {
  const std::string expected_key =
      base::StrCat({kCredentialsKeyPrefix, kExtensionId1});
  EXPECT_CALL(session_manager_client_,
              LoginScreenStorageStore(expected_key, _, kData, _))
      .WillRepeatedly(WithArgs<3>(Invoke(LoginScreenStorageStoreError)));

  auto function =
      base::MakeRefCounted<LoginScreenStorageStoreCredentialsFunction>();
  std::string args =
      base::StringPrintf(R"(["%s", "%s"])", kExtensionId1, kData);
  EXPECT_EQ(kError, RunFunctionAndReturnError(function.get(), args));
}

TEST_F(LoginScreenStorageApiUnittest, RetrieveCredentialsSuccess) {
  const std::string expected_key =
      base::StrCat({kCredentialsKeyPrefix, kExtensionId});
  EXPECT_CALL(session_manager_client_,
              LoginScreenStorageRetrieve(expected_key, _))
      .WillOnce(WithArgs<1>(Invoke(LoginScreenStorageRetrieveSuccess)));

  auto function =
      base::MakeRefCounted<LoginScreenStorageRetrieveCredentialsFunction>();
  EXPECT_EQ(kData,
            RunFunctionAndReturnValue(function.get(), "[]")->GetString());
}

TEST_F(LoginScreenStorageApiUnittest, RetrieveCredentialsError) {
  const std::string expected_key =
      base::StrCat({kCredentialsKeyPrefix, kExtensionId});
  EXPECT_CALL(session_manager_client_,
              LoginScreenStorageRetrieve(expected_key, _))
      .WillOnce(WithArgs<1>(Invoke(LoginScreenStorageRetrieveError)));

  auto function =
      base::MakeRefCounted<LoginScreenStorageRetrieveCredentialsFunction>();
  EXPECT_EQ(kError, RunFunctionAndReturnError(function.get(), "[]"));
}

}  // namespace extensions
