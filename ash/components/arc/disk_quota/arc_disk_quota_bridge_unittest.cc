// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/disk_quota/arc_disk_quota_bridge.h"

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

base::test::ScopedChromeOSVersionInfo SetArcAndroidSdkVersionForTesting(
    int version) {
  return base::test::ScopedChromeOSVersionInfo(
      base::StringPrintf("CHROMEOS_ARC_ANDROID_SDK_VERSION=%d", version),
      base::Time::Now());
}

class ArcDiskQuotaBridgeTest : public testing::Test {
 protected:
  ArcDiskQuotaBridgeTest()
      : bridge_(ArcDiskQuotaBridge::GetForBrowserContextForTesting(&context_)) {
  }
  ArcDiskQuotaBridgeTest(const ArcDiskQuotaBridgeTest&) = delete;
  ArcDiskQuotaBridgeTest& operator=(const ArcDiskQuotaBridgeTest&) = delete;
  ~ArcDiskQuotaBridgeTest() override = default;

  ArcDiskQuotaBridge* bridge() { return bridge_; }

  void SetUp() override {
    ash::SpacedClient::InitializeFake();
    ash::UserDataAuthClient::InitializeFake();
  }

  void TearDown() override {
    ash::UserDataAuthClient::Shutdown();
    ash::SpacedClient::Shutdown();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  user_prefs::TestBrowserContextWithPrefs context_;
  const raw_ptr<ArcDiskQuotaBridge, ExperimentalAsh> bridge_;
};

TEST_F(ArcDiskQuotaBridgeTest, IsQuotaSupported_Supported) {
  ash::FakeSpacedClient::Get()->set_quota_supported(true);
  ash::FakeUserDataAuthClient::TestApi::Get()->set_arc_quota_supported(true);

  base::test::TestFuture<bool> future;
  bridge()->IsQuotaSupported(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ArcDiskQuotaBridgeTest, IsQuotaSupported_NotSupportedInSpaced) {
  ash::FakeSpacedClient::Get()->set_quota_supported(false);
  ash::FakeUserDataAuthClient::TestApi::Get()->set_arc_quota_supported(true);

  base::test::TestFuture<bool> future;
  bridge()->IsQuotaSupported(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ArcDiskQuotaBridgeTest, IsQuotaSupported_NotSupportedInCryptohome) {
  ash::FakeSpacedClient::Get()->set_quota_supported(true);
  ash::FakeUserDataAuthClient::TestApi::Get()->set_arc_quota_supported(false);

  base::test::TestFuture<bool> future;
  bridge()->IsQuotaSupported(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ArcDiskQuotaBridgeTest, GetQuotaCurrentSpaceForGid_Success) {
  const std::vector<std::pair<uint32_t, int64_t>>
      valid_android_gid_and_expected_space = {
          {kAndroidGidStart, 100},
          {(kAndroidGidStart + kAndroidGidEnd) / 2, 200},
          {kAndroidGidEnd, 300},
      };
  for (const auto& [gid, space] : valid_android_gid_and_expected_space) {
    ash::FakeSpacedClient::Get()->set_quota_current_space_gid(
        gid + kArcGidShift, space);
  }

  for (const auto& [gid, space] : valid_android_gid_and_expected_space) {
    base::test::TestFuture<int64_t> future;
    bridge()->GetCurrentSpaceForGid(gid, future.GetCallback());
    EXPECT_EQ(future.Get(), space);
  }
}

TEST_F(ArcDiskQuotaBridgeTest, GetQuotaCurrentSpaceForGid_InvalidId) {
  constexpr uint32_t kInvalidAndroidGid = kAndroidGidEnd + 1;

  base::test::TestFuture<int64_t> future;
  bridge()->GetCurrentSpaceForGid(kInvalidAndroidGid, future.GetCallback());
  EXPECT_EQ(future.Get(), -1);
}

class ArcDiskQuotaBridgeWithArcVersionTest
    : public ArcDiskQuotaBridgeTest,
      public ::testing::WithParamInterface<int> {};

TEST_P(ArcDiskQuotaBridgeWithArcVersionTest,
       GetQuotaCurrentSpaceForUid_Success) {
  const int arc_sdk_version = GetParam();
  const auto scoped_version_info =
      SetArcAndroidSdkVersionForTesting(arc_sdk_version);
  const uint32_t kAndroidUidEnd = arc_sdk_version < kArcVersionT
                                      ? kAndroidUidEndBeforeT
                                      : kAndroidUidEndAfterT;

  const std::vector<std::pair<uint32_t, int64_t>>
      valid_android_uid_and_expected_space = {
          {kAndroidUidStart, 100},
          {(kAndroidUidStart + kAndroidUidEnd) / 2, 200},
          {kAndroidUidEnd, 300},
      };
  for (const auto& [uid, space] : valid_android_uid_and_expected_space) {
    ash::FakeSpacedClient::Get()->set_quota_current_space_uid(
        uid + kArcUidShift, space);
  }

  for (const auto& [uid, space] : valid_android_uid_and_expected_space) {
    base::test::TestFuture<int64_t> future;
    bridge()->GetCurrentSpaceForUid(uid, future.GetCallback());
    EXPECT_EQ(future.Get(), space);
  }
}

TEST_P(ArcDiskQuotaBridgeWithArcVersionTest,
       GetQuotaCurrentSpaceForUid_InvalidId) {
  const int arc_sdk_version = GetParam();
  const auto scoped_version_info =
      SetArcAndroidSdkVersionForTesting(arc_sdk_version);
  const uint32_t kAndroidUidEnd = arc_sdk_version < kArcVersionT
                                      ? kAndroidUidEndBeforeT
                                      : kAndroidUidEndAfterT;

  const uint32_t kInvalidAndroidUid = kAndroidUidEnd + 1;

  base::test::TestFuture<int64_t> future;
  bridge()->GetCurrentSpaceForUid(kInvalidAndroidUid, future.GetCallback());
  EXPECT_EQ(future.Get(), -1);
}

TEST_P(ArcDiskQuotaBridgeWithArcVersionTest,
       GetQuotaCurrentSpaceForProjectId_Success) {
  const int arc_sdk_version = GetParam();
  const auto scoped_version_info =
      SetArcAndroidSdkVersionForTesting(arc_sdk_version);
  const uint32_t kProjectIdForAndroidAppsEnd =
      arc_sdk_version < kArcVersionT ? kProjectIdForAndroidAppsEndBeforeT
                                     : kProjectIdForAndroidAppsEndAfterT;

  const std::vector<std::pair<uint32_t, int64_t>>
      valid_android_project_id_and_expected_space = {
          {kProjectIdForAndroidFilesStart, 100},
          {(kProjectIdForAndroidFilesStart + kProjectIdForAndroidFilesEnd) / 2,
           200},
          {kProjectIdForAndroidFilesEnd, 300},
          {kProjectIdForAndroidAppsStart, 400},
          {(kProjectIdForAndroidAppsStart + kProjectIdForAndroidAppsEnd) / 2,
           500},
          {kProjectIdForAndroidAppsEnd, 600},
      };
  for (const auto& [project_id, space] :
       valid_android_project_id_and_expected_space) {
    ash::FakeSpacedClient::Get()->set_quota_current_space_project_id(project_id,
                                                                     space);
  }

  for (const auto& [project_id, space] :
       valid_android_project_id_and_expected_space) {
    base::test::TestFuture<int64_t> future;
    bridge()->GetCurrentSpaceForProjectId(project_id, future.GetCallback());
    EXPECT_EQ(future.Get(), space);
  }
}

TEST_P(ArcDiskQuotaBridgeWithArcVersionTest,
       GetQuotaCurrentSpaceForProjectId_Invalid) {
  const int arc_sdk_version = GetParam();
  const auto scoped_version_info =
      SetArcAndroidSdkVersionForTesting(arc_sdk_version);
  const uint32_t kProjectIdForAndroidAppsEnd =
      arc_sdk_version < kArcVersionT ? kProjectIdForAndroidAppsEndBeforeT
                                     : kProjectIdForAndroidAppsEndAfterT;

  const std::vector<uint32_t> invalid_android_project_id = {
      kProjectIdForAndroidFilesStart - 1,
      kProjectIdForAndroidFilesEnd + 1,
      kProjectIdForAndroidAppsStart - 1,
      kProjectIdForAndroidAppsEnd + 1,
  };

  for (const auto project_id : invalid_android_project_id) {
    base::test::TestFuture<int64_t> future;
    bridge()->GetCurrentSpaceForProjectId(project_id, future.GetCallback());
    EXPECT_EQ(future.Get(), -1);
  }
}

INSTANTIATE_TEST_SUITE_P(ArcDiskQuotaBridgeTestForR,
                         ArcDiskQuotaBridgeWithArcVersionTest,
                         testing::Values(kArcVersionR));

INSTANTIATE_TEST_SUITE_P(ArcDiskQuotaBridgeTestForT,
                         ArcDiskQuotaBridgeWithArcVersionTest,
                         testing::Values(kArcVersionT));

}  // namespace
}  // namespace arc
