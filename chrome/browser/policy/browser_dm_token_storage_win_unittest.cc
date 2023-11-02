// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage_win.h"

#include <tuple>

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
constexpr char kDMToken2[] = "fake-dm-token-2";

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

  // Sets the preferred policy (in ...\CloudManagement).
  bool SetPreferredEnrollmentTokenPolicy(const wchar_t* enrollment_token) {
    auto paths = InstallUtil::GetCloudManagementEnrollmentTokenRegistryPaths();
    const auto& key_and_value = paths.front();
    base::win::RegKey key;
    return key.Create(HKEY_LOCAL_MACHINE, key_and_value.first.c_str(),
                      KEY_SET_VALUE) == ERROR_SUCCESS &&
           key.WriteValue(key_and_value.second.c_str(), enrollment_token) ==
               ERROR_SUCCESS;
  }

  // Sets the Chrome policy (in ...\Chrome).
  bool SetSecondaryEnrollmentTokenPolicy(const wchar_t* enrollment_token) {
    auto paths = InstallUtil::GetCloudManagementEnrollmentTokenRegistryPaths();
    EXPECT_GE(paths.size(), size_t{2});
    const auto& key_and_value = paths[1];
    base::win::RegKey key;
    return key.Create(HKEY_LOCAL_MACHINE, key_and_value.first.c_str(),
                      KEY_SET_VALUE) == ERROR_SUCCESS &&
           key.WriteValue(key_and_value.second.c_str(), enrollment_token) ==
               ERROR_SUCCESS;
  }

  // Sets a DM token in either the app-neutral or browser-specific location in
  // the registry.
  bool SetDMToken(const std::string& dm_token,
                  InstallUtil::BrowserLocation browser_location) {
    base::win::RegKey key;
    std::wstring dm_token_value_name;
    std::tie(key, dm_token_value_name) =
        InstallUtil::GetCloudManagementDmTokenLocation(
            InstallUtil::ReadOnly(false), browser_location);
    return key.Valid() &&
           key.WriteValue(dm_token_value_name.c_str(), dm_token.c_str(),
                          dm_token.size(), REG_BINARY) == ERROR_SUCCESS;
  }

  content::BrowserTaskEnvironment task_environment_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

TEST_F(BrowserDMTokenStorageWinTest, InitClientId) {
  ASSERT_TRUE(SetMachineGuid(kClientId1));
  BrowserDMTokenStorageWin storage;
  EXPECT_EQ(base::WideToUTF8(kClientId1), storage.InitClientId());
}

TEST_F(BrowserDMTokenStorageWinTest, InitEnrollmentTokenFromPreferred) {
  ASSERT_TRUE(SetMachineGuid(kClientId1));
  ASSERT_TRUE(SetPreferredEnrollmentTokenPolicy(kEnrollmentToken1));
  ASSERT_TRUE(SetSecondaryEnrollmentTokenPolicy(kEnrollmentToken2));

  BrowserDMTokenStorageWin storage;
  EXPECT_EQ(base::WideToUTF8(kEnrollmentToken1), storage.InitEnrollmentToken());
}

TEST_F(BrowserDMTokenStorageWinTest, InitEnrollmentTokenFromSecondary) {
  ASSERT_TRUE(SetMachineGuid(kClientId1));
  ASSERT_TRUE(SetSecondaryEnrollmentTokenPolicy(kEnrollmentToken2));

  BrowserDMTokenStorageWin storage;
  EXPECT_EQ(base::WideToUTF8(kEnrollmentToken2), storage.InitEnrollmentToken());
}

TEST_F(BrowserDMTokenStorageWinTest, InitDMToken) {
  ASSERT_TRUE(SetMachineGuid(kClientId1));

  // The app-neutral location should be preferred.
  ASSERT_TRUE(SetDMToken(kDMToken1, InstallUtil::BrowserLocation(false)));
  ASSERT_TRUE(SetDMToken(kDMToken2, InstallUtil::BrowserLocation(true)));
  BrowserDMTokenStorageWin storage;
  EXPECT_EQ(std::string(kDMToken1), storage.InitDMToken());
}

TEST_F(BrowserDMTokenStorageWinTest, InitDMTokenFromBrowserLocation) {
  ASSERT_TRUE(SetMachineGuid(kClientId1));

  // If there's no app-neutral token, the browser-specific one should be used.
  ASSERT_TRUE(SetDMToken(kDMToken2, InstallUtil::BrowserLocation(true)));
  BrowserDMTokenStorageWin storage;
  EXPECT_EQ(std::string(kDMToken2), storage.InitDMToken());
}

}  // namespace policy
