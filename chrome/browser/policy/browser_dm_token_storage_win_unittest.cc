// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage_win.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/installer/util/install_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::IsEmpty;

namespace policy {

namespace {

constexpr wchar_t kClientId1[] = L"fake-client-id-1";
constexpr wchar_t kEnrollmentToken1[] = L"fake-enrollment-token-1";
constexpr wchar_t kEnrollmentToken2[] = L"fake-enrollment-token-2";
constexpr char kDMToken1[] = "fake-dm-token-1";

}  // namespace

class BrowserDMTokenStorageWinTest : public testing::Test {
 protected:
  BrowserDMTokenStorageWinTest() {}
  ~BrowserDMTokenStorageWinTest() override {}

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

  bool SetMachineGuid(const wchar_t* machine_guid) {
    base::win::RegKey key;
    return (key.Create(HKEY_LOCAL_MACHINE, L"software\\microsoft\\cryptography",
                       KEY_SET_VALUE) == ERROR_SUCCESS) &&
           (key.WriteValue(L"MachineGuid", machine_guid) == ERROR_SUCCESS);
  }

  bool SetEnrollmentToken(const wchar_t* enrollment_token) {
    base::string16 key_path;
    base::string16 value_name;
    base::string16 old_value_name;
    InstallUtil::GetMachineLevelUserCloudPolicyEnrollmentTokenRegistryPath(
        &key_path, &value_name, &old_value_name);
    base::win::RegKey key;
    return (key.Create(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_SET_VALUE) ==
            ERROR_SUCCESS) &&
           (key.WriteValue(value_name.c_str(), enrollment_token) ==
            ERROR_SUCCESS);
  }

  // TODO(crbug.com/907589): Remove once no longer in use.
  bool SetOldEnrollmentToken(const wchar_t* enrollment_token) {
    base::string16 key_path;
    base::string16 value_name;
    base::string16 old_value_name;
    InstallUtil::GetMachineLevelUserCloudPolicyEnrollmentTokenRegistryPath(
        &key_path, &value_name, &old_value_name);
    base::win::RegKey key;
    return (key.Create(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_SET_VALUE) ==
            ERROR_SUCCESS) &&
           (key.WriteValue(old_value_name.c_str(), enrollment_token) ==
            ERROR_SUCCESS);
  }

  bool SetDMToken(const std::string& dm_token) {
    base::win::RegKey key;
    base::string16 dm_token_key_path;
    base::string16 dm_token_value_name;
    InstallUtil::GetMachineLevelUserCloudPolicyDMTokenRegistryPath(
        &dm_token_key_path, &dm_token_value_name);
    return (key.Create(HKEY_LOCAL_MACHINE, dm_token_key_path.c_str(),
                       KEY_SET_VALUE | KEY_WOW64_64KEY) == ERROR_SUCCESS) &&
           (key.WriteValue(dm_token_value_name.c_str(), dm_token.c_str(),
                           dm_token.size(), REG_BINARY) == ERROR_SUCCESS);
  }

  content::BrowserTaskEnvironment task_environment_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

TEST_F(BrowserDMTokenStorageWinTest, InitClientId) {
  ASSERT_TRUE(SetMachineGuid(kClientId1));
  BrowserDMTokenStorageWin storage;
  EXPECT_EQ(base::WideToUTF8(kClientId1), storage.InitClientId());
}

TEST_F(BrowserDMTokenStorageWinTest, InitEnrollmentToken) {
  ASSERT_TRUE(SetMachineGuid(kClientId1));
  ASSERT_TRUE(SetEnrollmentToken(kEnrollmentToken1));

  BrowserDMTokenStorageWin storage;
  EXPECT_EQ(base::WideToUTF8(kEnrollmentToken1), storage.InitEnrollmentToken());
}

// TODO(crbug.com/907589): Remove once no longer in use.
TEST_F(BrowserDMTokenStorageWinTest, InitOldEnrollmentToken) {
  ASSERT_TRUE(SetMachineGuid(kClientId1));
  ASSERT_TRUE(SetOldEnrollmentToken(kEnrollmentToken1));

  BrowserDMTokenStorageWin storage;
  EXPECT_EQ(base::WideToUTF8(kEnrollmentToken1), storage.InitEnrollmentToken());
}

// This test ensures that the new location has precedence over the old one, if
// both are set.
// TODO(crbug.com/907589): Remove once no longer in use.
TEST_F(BrowserDMTokenStorageWinTest, InitOldEnrollmentTokenPriority) {
  ASSERT_TRUE(SetMachineGuid(kClientId1));

  base::string16 key_path;
  base::string16 value_name;
  base::string16 old_value_name;
  InstallUtil::GetMachineLevelUserCloudPolicyEnrollmentTokenRegistryPath(
      &key_path, &value_name, &old_value_name);

  base::win::RegKey key;
  bool success1 = (key.Create(HKEY_LOCAL_MACHINE, key_path.c_str(),
                              KEY_SET_VALUE) == ERROR_SUCCESS) &&
                  (key.WriteValue(L"CloudManagementEnrollmentToken",
                                  kEnrollmentToken1) == ERROR_SUCCESS);

  ASSERT_TRUE(success1);

  bool success2 = (key.Create(HKEY_LOCAL_MACHINE, key_path.c_str(),
                              KEY_SET_VALUE) == ERROR_SUCCESS) &&
                  (key.WriteValue(L"MachineLevelUserCloudPolicyEnrollmentToken",
                                  kEnrollmentToken2) == ERROR_SUCCESS);

  ASSERT_TRUE(success2);

  BrowserDMTokenStorageWin storage;
  EXPECT_EQ(base::WideToUTF8(kEnrollmentToken1), storage.InitEnrollmentToken());
}

TEST_F(BrowserDMTokenStorageWinTest, InitDMToken) {
  ASSERT_TRUE(SetMachineGuid(kClientId1));

  ASSERT_TRUE(SetDMToken(kDMToken1));
  BrowserDMTokenStorageWin storage;
  EXPECT_EQ(std::string(kDMToken1), storage.InitDMToken());
}

}  // namespace policy
