// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/os_crypt/app_bound_encryption_win.h"

#include <string>

#include "base/test/test_reg_util_win.h"
#include "base/win/windows_types.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/elevation_service/elevator.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InvokeWithoutArgs;
using ::testing::StrictMock;

namespace os_crypt {

namespace {

class MockAppBoundEncryptionOverrides
    : public AppBoundEncryptionOverridesForTesting {
 public:
  MOCK_METHOD(HRESULT,
              EncryptAppBoundString,
              (ProtectionLevel level,
               const std::string& plaintext,
               std::string& ciphertext,
               DWORD& last_error,
               elevation_service::EncryptFlags* flags),
              (override));

  MOCK_METHOD(HRESULT,
              DecryptAppBoundString,
              (const std::string& ciphertext,
               std::string& plaintext,
               ProtectionLevel protection_level,
               std::optional<std::string>& new_ciphertext,
               DWORD& last_error,
               elevation_service::EncryptFlags* flags),
              (override));

  MOCK_METHOD(SupportLevel,
              GetAppBoundEncryptionSupportLevel,
              (PrefService * local_state),
              (override));
};

TEST(AppBoundEncryptionOverrides, Function) {
  ::testing::StrictMock<MockAppBoundEncryptionOverrides> mock;

  SetOverridesForTesting(&mock);

  EXPECT_CALL(mock, GetAppBoundEncryptionSupportLevel)
      .WillOnce(::testing::Return(SupportLevel::kSupported));

  ASSERT_EQ(SupportLevel::kSupported,
            GetAppBoundEncryptionSupportLevel(nullptr));

  SetOverridesForTesting(nullptr);
}

using AppBoundEncryptionTest = ::testing::TestWithParam<bool>;

TEST_P(AppBoundEncryptionTest, FSLogixRoaming) {
  registry_util::RegistryOverrideManager overrides;
  ASSERT_NO_FATAL_FAILURE(overrides.OverrideRegistry(HKEY_LOCAL_MACHINE));
  if (GetParam()) {
    EXPECT_EQ(ERROR_SUCCESS,
              base::win::RegKey{}.Create(HKEY_LOCAL_MACHINE,
                                         L"SOFTWARE\\FSLogix", KEY_WRITE));
  }
  install_static::ScopedInstallDetails fake_system_install(
      /*system_level=*/true);
  chrome::SetUsingDefaultUserDataDirectoryForTesting(true);
  TestingPrefServiceSimple prefs;

  EXPECT_EQ(GetParam() ? SupportLevel::kDisabledByRoamingWindowsProfile
                       : SupportLevel::kSupported,
            GetAppBoundEncryptionSupportLevel(&prefs));
}

INSTANTIATE_TEST_SUITE_P(, AppBoundEncryptionTest, ::testing::Bool());

}  // namespace

}  // namespace os_crypt
