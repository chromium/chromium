// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_android_util.h"

#include <limits>
#include <memory>

#include "base/android/device_info.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/android/mock_password_manager_util_bridge.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using password_manager::GetSplitStoresUpmMinVersion;
using testing::Return;

namespace password_manager_android_util {
namespace {

class PasswordManagerAndroidUtilTest : public testing::Test {
 public:
  PasswordManagerAndroidUtilTest() {
    // Most tests check the modern GmsCore case.
    base::android::device_info::set_gms_version_code_for_test(
        base::NumberToString(GetSplitStoresUpmMinVersion()));
  }

  std::unique_ptr<MockPasswordManagerUtilBridge>
  GetMockBridgeWithBackendPresent() {
    auto mock_bridge = std::make_unique<MockPasswordManagerUtilBridge>();
    ON_CALL(*mock_bridge, IsInternalBackendPresent).WillByDefault(Return(true));
    return mock_bridge;
  }
};

TEST_F(PasswordManagerAndroidUtilTest,
       PasswordManagerNotAvailableNoInternalBackend) {
  // Make sure all the other criteria are fulfilled.
  base::android::device_info::set_gms_version_code_for_test(
      base::NumberToString(GetSplitStoresUpmMinVersion()));

  std::unique_ptr<MockPasswordManagerUtilBridge> mock_util_bridge =
      std::make_unique<MockPasswordManagerUtilBridge>();
  EXPECT_CALL(*mock_util_bridge, IsInternalBackendPresent)
      .WillOnce(Return(false));
  EXPECT_FALSE(IsPasswordManagerAvailable(std::move(mock_util_bridge)));
}

TEST_F(PasswordManagerAndroidUtilTest,
       PasswordManagerNotAvailableGmsVersionTooLow) {
  std::unique_ptr<MockPasswordManagerUtilBridge> mock_util_bridge =
      std::make_unique<MockPasswordManagerUtilBridge>();
  EXPECT_CALL(*mock_util_bridge, IsInternalBackendPresent)
      .WillOnce(Return(true));

  // Set a GMS Core version that is lower than the min required version.
  base::android::device_info::set_gms_version_code_for_test(
      base::NumberToString(GetSplitStoresUpmMinVersion() - 1));

  EXPECT_FALSE(IsPasswordManagerAvailable(std::move(mock_util_bridge)));
}

TEST_F(PasswordManagerAndroidUtilTest, PasswordManagerAvailable) {
  std::unique_ptr<MockPasswordManagerUtilBridge> mock_util_bridge =
      std::make_unique<MockPasswordManagerUtilBridge>();
  EXPECT_CALL(*mock_util_bridge, IsInternalBackendPresent)
      .WillOnce(Return(true));

  base::android::device_info::set_gms_version_code_for_test(
      base::NumberToString(GetSplitStoresUpmMinVersion()));

  EXPECT_TRUE(IsPasswordManagerAvailable(std::move(mock_util_bridge)));
}

}  // namespace
}  // namespace password_manager_android_util
