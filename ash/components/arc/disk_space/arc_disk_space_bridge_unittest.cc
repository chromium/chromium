// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/disk_space/arc_disk_space_bridge.h"

#include <vector>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_disk_space_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"
#include "chromeos/ash/components/dbus/spaced/spaced.pb.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

constexpr int64_t kGiB = 1024 * 1024 * 1024;
constexpr int64_t kInitialFreeDiskSpace = 30 * kGiB;

base::test::ScopedChromeOSVersionInfo SetArcAndroidSdkVersionForTesting(
    int version) {
  return base::test::ScopedChromeOSVersionInfo(
      base::StringPrintf("CHROMEOS_ARC_ANDROID_SDK_VERSION=%d", version),
      base::Time::Now());
}

class ArcDiskSpaceBridgeTest : public testing::Test {
 protected:
  ArcDiskSpaceBridgeTest() = default;
  ArcDiskSpaceBridgeTest(const ArcDiskSpaceBridgeTest&) = delete;
  ArcDiskSpaceBridgeTest& operator=(const ArcDiskSpaceBridgeTest&) = delete;
  ~ArcDiskSpaceBridgeTest() override = default;

  const FakeDiskSpaceInstance* disk_space_instance() const {
    return &disk_space_instance_;
  }
  ArcDiskSpaceBridge* bridge() { return bridge_.get(); }

  void SetUp() override {
    ash::SpacedClient::InitializeFake();
    ash::FakeSpacedClient::Get()->set_free_disk_space(kInitialFreeDiskSpace);
    ash::UserDataAuthClient::InitializeFake();

    bridge_ = std::make_unique<ArcDiskSpaceBridge>(
        &context_, arc_service_manager_.arc_bridge_service());

    ArcServiceManager::Get()->arc_bridge_service()->disk_space()->SetInstance(
        &disk_space_instance_);
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->disk_space());
  }

  void TearDown() override {
    bridge_.reset();
    ash::UserDataAuthClient::Shutdown();
    ash::SpacedClient::Shutdown();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  FakeDiskSpaceInstance disk_space_instance_;
  user_prefs::TestBrowserContextWithPrefs context_;
  std::unique_ptr<ArcDiskSpaceBridge> bridge_;
};

TEST_F(ArcDiskSpaceBridgeTest, IsQuotaSupported_Supported) {
  ash::FakeSpacedClient::Get()->set_quota_supported(true);
  ash::FakeUserDataAuthClient::TestApi::Get()->set_arc_quota_supported(true);

  base::test::TestFuture<bool> future;
  bridge()->IsQuotaSupported(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ArcDiskSpaceBridgeTest, IsQuotaSupported_NotSupportedInSpaced) {
  ash::FakeSpacedClient::Get()->set_quota_supported(false);
  ash::FakeUserDataAuthClient::TestApi::Get()->set_arc_quota_supported(true);

  base::test::TestFuture<bool> future;
  bridge()->IsQuotaSupported(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ArcDiskSpaceBridgeTest, IsQuotaSupported_NotSupportedInCryptohome) {
  ash::FakeSpacedClient::Get()->set_quota_supported(true);
  ash::FakeUserDataAuthClient::TestApi::Get()->set_arc_quota_supported(false);

  base::test::TestFuture<bool> future;
  bridge()->IsQuotaSupported(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(ArcDiskSpaceBridgeTest, GetQuotaCurrentSpaceForGid_Success) {
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
    bridge()->GetQuotaCurrentSpaceForGid(gid, future.GetCallback());
    EXPECT_EQ(future.Get(), space);
  }
}

TEST_F(ArcDiskSpaceBridgeTest, GetQuotaCurrentSpaceForGid_InvalidId) {
  constexpr uint32_t kInvalidAndroidGid = kAndroidGidEnd + 1;

  base::test::TestFuture<int64_t> future;
  bridge()->GetQuotaCurrentSpaceForGid(kInvalidAndroidGid,
                                       future.GetCallback());
  EXPECT_EQ(future.Get(), -1);
}

class ArcDiskSpaceBridgeWithArcVersionTest
    : public ArcDiskSpaceBridgeTest,
      public ::testing::WithParamInterface<int> {};

TEST_P(ArcDiskSpaceBridgeWithArcVersionTest,
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
    bridge()->GetQuotaCurrentSpaceForUid(uid, future.GetCallback());
    EXPECT_EQ(future.Get(), space);
  }
}

TEST_P(ArcDiskSpaceBridgeWithArcVersionTest,
       GetQuotaCurrentSpaceForUid_InvalidId) {
  const int arc_sdk_version = GetParam();
  const auto scoped_version_info =
      SetArcAndroidSdkVersionForTesting(arc_sdk_version);
  const uint32_t kAndroidUidEnd = arc_sdk_version < kArcVersionT
                                      ? kAndroidUidEndBeforeT
                                      : kAndroidUidEndAfterT;

  const uint32_t kInvalidAndroidUid = kAndroidUidEnd + 1;

  base::test::TestFuture<int64_t> future;
  bridge()->GetQuotaCurrentSpaceForUid(kInvalidAndroidUid,
                                       future.GetCallback());
  EXPECT_EQ(future.Get(), -1);
}

TEST_P(ArcDiskSpaceBridgeWithArcVersionTest,
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
    bridge()->GetQuotaCurrentSpaceForProjectId(project_id,
                                               future.GetCallback());
    EXPECT_EQ(future.Get(), space);
  }
}

TEST_P(ArcDiskSpaceBridgeWithArcVersionTest,
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
    bridge()->GetQuotaCurrentSpaceForProjectId(project_id,
                                               future.GetCallback());
    EXPECT_EQ(future.Get(), -1);
  }
}

TEST_P(ArcDiskSpaceBridgeWithArcVersionTest, GetQuotaCurrentSpacesForIds) {
  const int arc_sdk_version = GetParam();
  const auto scoped_version_info =
      SetArcAndroidSdkVersionForTesting(arc_sdk_version);
  const uint32_t kAndroidUidEnd = arc_sdk_version < kArcVersionT
                                      ? kAndroidUidEndBeforeT
                                      : kAndroidUidEndAfterT;
  const uint32_t kProjectIdForAndroidAppsEnd =
      arc_sdk_version < kArcVersionT ? kProjectIdForAndroidAppsEndBeforeT
                                     : kProjectIdForAndroidAppsEndAfterT;

  const std::vector<std::pair<uint32_t, int64_t>>
      valid_android_uid_and_expected_space = {
          {kAndroidUidStart, 100},
          {(kAndroidUidStart + kAndroidUidEnd) / 2, 200},
          {kAndroidUidEnd, 300},
      };
  const std::vector<std::pair<uint32_t, int64_t>>
      valid_android_gid_and_expected_space = {
          {kAndroidGidStart, 100},
          {(kAndroidGidStart + kAndroidGidEnd) / 2, 200},
          {kAndroidGidEnd, 300},
      };
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

  std::vector<uint32_t> valid_uids;
  std::vector<uint32_t> valid_gids;
  std::vector<uint32_t> valid_project_ids;
  for (const auto& [uid, space] : valid_android_uid_and_expected_space) {
    valid_uids.push_back(uid);
    ash::FakeSpacedClient::Get()->set_quota_current_space_uid(
        uid + kArcUidShift, space);
  }
  for (const auto& [gid, space] : valid_android_gid_and_expected_space) {
    valid_gids.push_back(gid);
    ash::FakeSpacedClient::Get()->set_quota_current_space_gid(
        gid + kArcGidShift, space);
  }
  for (const auto& [project_id, space] :
       valid_android_project_id_and_expected_space) {
    valid_project_ids.push_back(project_id);
    ash::FakeSpacedClient::Get()->set_quota_current_space_project_id(project_id,
                                                                     space);
  }

  // Check that GetQuotaCurrentSpacesForIds returns null when there is an
  // invalid ID in any of the input lists of IDs.
  std::vector<uint32_t> invalid_uids(valid_uids);
  invalid_uids.push_back(kAndroidUidEnd + 1);
  base::test::TestFuture<mojom::QuotaSpacesPtr> invalid_uid_future;
  bridge()->GetQuotaCurrentSpacesForIds(invalid_uids, valid_gids,
                                        valid_project_ids,
                                        invalid_uid_future.GetCallback());
  EXPECT_TRUE(invalid_uid_future.Get().is_null());

  std::vector<uint32_t> invalid_gids(valid_gids);
  invalid_gids.push_back(kAndroidGidEnd + 1);
  base::test::TestFuture<mojom::QuotaSpacesPtr> invalid_gid_future;
  bridge()->GetQuotaCurrentSpacesForIds(valid_uids, invalid_gids,
                                        valid_project_ids,
                                        invalid_gid_future.GetCallback());
  EXPECT_TRUE(invalid_gid_future.Get().is_null());

  std::vector<uint32_t> invalid_project_ids(valid_project_ids);
  invalid_project_ids.push_back(kProjectIdForAndroidAppsEnd + 1);
  base::test::TestFuture<mojom::QuotaSpacesPtr> invalid_project_id_future;
  bridge()->GetQuotaCurrentSpacesForIds(
      valid_uids, valid_gids, invalid_project_ids,
      invalid_project_id_future.GetCallback());
  EXPECT_TRUE(invalid_project_id_future.Get().is_null());

  // Check the behavior of GetQuotaCurrentSpacesForIds for the lists of all
  // valid IDs.
  base::test::TestFuture<mojom::QuotaSpacesPtr> success_future;
  bridge()->GetQuotaCurrentSpacesForIds(
      valid_uids, valid_gids, valid_project_ids, success_future.GetCallback());
  EXPECT_FALSE(success_future.Get().is_null());
  for (size_t i = 0; i < valid_uids.size(); i++) {
    EXPECT_EQ(success_future.Get()->curspaces_for_uids[i],
              valid_android_uid_and_expected_space[i].second);
  }
  for (size_t i = 0; i < valid_gids.size(); i++) {
    EXPECT_EQ(success_future.Get()->curspaces_for_gids[i],
              valid_android_gid_and_expected_space[i].second);
  }
  for (size_t i = 0; i < valid_project_ids.size(); i++) {
    EXPECT_EQ(success_future.Get()->curspaces_for_project_ids[i],
              valid_android_project_id_and_expected_space[i].second);
  }
}

INSTANTIATE_TEST_SUITE_P(ArcDiskSpaceBridgeTestForR,
                         ArcDiskSpaceBridgeWithArcVersionTest,
                         testing::Values(kArcVersionR));

INSTANTIATE_TEST_SUITE_P(ArcDiskSpaceBridgeTestForT,
                         ArcDiskSpaceBridgeWithArcVersionTest,
                         testing::Values(kArcVersionT));

TEST_F(ArcDiskSpaceBridgeTest, GetApplicationsSize) {
  ASSERT_NE(nullptr, bridge());
  base::test::TestFuture<bool, mojom::ApplicationsSizePtr> future;
  EXPECT_TRUE(bridge()->GetApplicationsSize(future.GetCallback()));
  EXPECT_EQ(1u, disk_space_instance()->num_get_applications_size_called());
  EXPECT_TRUE(future.Get<0>());
}

TEST_F(ArcDiskSpaceBridgeTest, SendResizeStorageBalloon) {
  // After the instance is ready, the bridge sets the initial balloon size.
  EXPECT_EQ(disk_space_instance()->free_space_bytes(),
            kInitialFreeDiskSpace - kStorageBalloonFreeSpaceBufferSizeInBytes);

  // Chrome receives StatefulDiskSpaceUpdate D-Bus signal with an updated
  // host-side free disk space.
  constexpr int64_t kNewFreeDiskSpace = 29 * kGiB;
  spaced::StatefulDiskSpaceUpdate update;
  update.set_free_space_bytes(kNewFreeDiskSpace);
  update.set_state(spaced::StatefulDiskSpaceState::NORMAL);
  ash::FakeSpacedClient::Get()->SendStatefulDiskSpaceUpdate(update);

  // After receiving the signal, the balloon size is updated accordingly.
  EXPECT_EQ(disk_space_instance()->free_space_bytes(),
            kNewFreeDiskSpace - kStorageBalloonFreeSpaceBufferSizeInBytes);
}

}  // namespace
}  // namespace arc
