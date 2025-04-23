// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/os_crypt/app_bound_encryption_win.h"

#include <string>

#include "base/win/windows_types.h"
#include "chrome/elevation_service/elevation_service_idl.h"
#include "chrome/elevation_service/elevator.h"
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

}  // namespace

}  // namespace os_crypt
