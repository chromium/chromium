// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/browser_management/platform_management_status_provider_android.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/util/android_enterprise_info.h"
#include "components/policy/core/common/management/management_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class AndroidManagementStatusProviderParameterizedTest
    : public ::testing::TestWithParam<
          std::tuple<bool, bool, EnterpriseManagementAuthority>> {
 protected:
  AndroidManagementStatusProviderParameterizedTest() {
    instance_ = enterprise_util::AndroidEnterpriseInfo::GetInstance();
    instance_->set_skip_jni_call_for_testing(true);
  }

  raw_ptr<enterprise_util::AndroidEnterpriseInfo> instance_;
  base::test::TaskEnvironment task_environment_;
};

TEST_P(AndroidManagementStatusProviderParameterizedTest, CheckAuthority) {
  const auto& [device_owned, profile_owned, expected_authority] = GetParam();

  AndroidManagementStatusProvider provider;
  base::test::TestFuture<std::pair<ManagementStatusProvider*,
                                   EnterpriseManagementAuthority>>
      test_future;

  provider.FetchAuthorityAsync(test_future.GetCallback());

  instance_->ServiceCallbacksForTesting(device_owned, profile_owned);

  EXPECT_EQ(test_future.Get().second, expected_authority);
}

INSTANTIATE_TEST_SUITE_P(
    AllCases,
    AndroidManagementStatusProviderParameterizedTest,
    ::testing::Values(
        std::make_tuple(true, true, EnterpriseManagementAuthority::CLOUD),
        std::make_tuple(true, false, EnterpriseManagementAuthority::CLOUD),
        std::make_tuple(false, true, EnterpriseManagementAuthority::CLOUD),
        std::make_tuple(false, false, EnterpriseManagementAuthority::NONE)));

}  // namespace policy
