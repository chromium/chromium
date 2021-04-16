// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/sync/test/integration/device_info_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/time.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/invalidations/switches.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/sync_device_info/device_info_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using testing::Contains;
using testing::ElementsAre;
using testing::IsSupersetOf;
using testing::UnorderedElementsAre;

MATCHER_P(HasCacheGuid, expected_cache_guid, "") {
  return arg.specifics().device_info().cache_guid() == expected_cache_guid;
}

MATCHER(HasFullHardwareClass, "") {
  return !arg.specifics().device_info().full_hardware_class().empty();
}

MATCHER(IsFullHardwareClassEmpty, "") {
  return arg.specifics().device_info().full_hardware_class().empty();
}

std::string CacheGuidForSuffix(int suffix) {
  return base::StringPrintf("cache guid %d", suffix);
}

std::string ClientNameForSuffix(int suffix) {
  return base::StringPrintf("client name %d", suffix);
}

std::string SyncUserAgentForSuffix(int suffix) {
  return base::StringPrintf("sync user agent %d", suffix);
}

std::string ChromeVersionForSuffix(int suffix) {
  return base::StringPrintf("chrome version %d", suffix);
}

std::string SigninScopedDeviceIdForSuffix(int suffix) {
  return base::StringPrintf("signin scoped device id %d", suffix);
}

sync_pb::DeviceInfoSpecifics CreateSpecifics(int suffix) {
  sync_pb::DeviceInfoSpecifics specifics;
  specifics.set_cache_guid(CacheGuidForSuffix(suffix));
  specifics.set_client_name(ClientNameForSuffix(suffix));
  specifics.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
  specifics.set_sync_user_agent(SyncUserAgentForSuffix(suffix));
  specifics.set_chrome_version(ChromeVersionForSuffix(suffix));
  specifics.set_signin_scoped_device_id(SigninScopedDeviceIdForSuffix(suffix));
  specifics.set_last_updated_timestamp(
      syncer::TimeToProtoTime(base::Time::Now()));
  return specifics;
}

class SingleClientDeviceInfoSyncTest : public SyncTest {
 public:
  SingleClientDeviceInfoSyncTest() : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientDeviceInfoSyncTest() override {}

  std::string GetLocalCacheGuid() {
    syncer::SyncTransportDataPrefs prefs(GetProfile(0)->GetPrefs());
    return prefs.GetCacheGuid();
  }

  // Injects a test DeviceInfo entity to the fake server.
  void InjectDeviceInfoEntityToServer(int suffix) {
    sync_pb::EntitySpecifics specifics;
    *specifics.mutable_device_info() = CreateSpecifics(suffix);
    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"",
            /*client_tag=*/
            syncer::DeviceInfoUtil::SpecificsToTag(specifics.device_info()),
            specifics,
            /*creation_time=*/0, /*last_modified_time=*/0));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientDeviceInfoSyncTest);
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       UmaEnabledSetFullHardwareClass) {
  bool uma_enabled = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &uma_enabled);
  ASSERT_TRUE(SetupSync());

  EXPECT_THAT(fake_server_->GetSyncEntitiesByModelType(syncer::DEVICE_INFO),
              Contains(HasFullHardwareClass()));

  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       UmaDisabledFullHardwareClassEmpty) {
  bool uma_enabled = false;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &uma_enabled);
  ASSERT_TRUE(SetupSync());

  EXPECT_THAT(fake_server_->GetSyncEntitiesByModelType(syncer::DEVICE_INFO),
              Contains(IsFullHardwareClassEmpty()));

  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}
#else
IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       UmaEnabledFullHardwareClassOnNonChromeOS) {
  bool uma_enabled = true;
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &uma_enabled);
  ASSERT_TRUE(SetupSync());

  EXPECT_THAT(fake_server_->GetSyncEntitiesByModelType(syncer::DEVICE_INFO),
              Contains(IsFullHardwareClassEmpty()));

  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest, CommitLocalDevice) {
  ASSERT_TRUE(SetupSync());

  // The local device should eventually be committed to the server.
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(
          GetFakeServer(), ElementsAre(HasCacheGuid(GetLocalCacheGuid())))
          .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest, DownloadRemoteDevices) {
  InjectDeviceInfoEntityToServer(/*suffix=*/1);
  InjectDeviceInfoEntityToServer(/*suffix=*/2);

  ASSERT_TRUE(SetupSync());

  // The local device may or may not already be committed at this point.
  EXPECT_THAT(fake_server_->GetSyncEntitiesByModelType(syncer::DEVICE_INFO),
              IsSupersetOf({HasCacheGuid(CacheGuidForSuffix(1)),
                            HasCacheGuid(CacheGuidForSuffix(2))}));
}

// CommitLocalDevice_TransportOnly and DownloadRemoteDevices_TransportOnly are
// flaky on Android.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       CommitLocalDevice_TransportOnly) {
  ASSERT_TRUE(SetupClients());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS, Sync-the-feature gets started automatically once a primary
  // account is signed in. To prevent that, explicitly set SyncRequested to
  // false.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Setup a primary account, but don't actually enable Sync-the-feature (so
  // that Sync will start in transport mode).
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::DEVICE_INFO));

  // The local device should eventually be committed to the server.
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(
          GetFakeServer(), ElementsAre(HasCacheGuid(GetLocalCacheGuid())))
          .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       DownloadRemoteDevices_TransportOnly) {
  InjectDeviceInfoEntityToServer(/*suffix=*/1);
  InjectDeviceInfoEntityToServer(/*suffix=*/2);

  ASSERT_TRUE(SetupClients());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS, Sync-the-feature gets started automatically once a primary
  // account is signed in. To prevent that, explicitly set SyncRequested to
  // false.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Setup a primary account, but don't actually enable Sync-the-feature (so
  // that Sync will start in transport mode).
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(syncer::DEVICE_INFO));

  EXPECT_THAT(fake_server_->GetSyncEntitiesByModelType(syncer::DEVICE_INFO),
              IsSupersetOf({HasCacheGuid(CacheGuidForSuffix(1)),
                            HasCacheGuid(CacheGuidForSuffix(2))}));
}
#endif  // !defined(OS_ANDROID)

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       ShouldSetTheOnlyClientFlag) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(
      ServerDeviceInfoMatchChecker(
          GetFakeServer(), ElementsAre(HasCacheGuid(GetLocalCacheGuid())))
          .Wait());

  sync_pb::ClientToServerMessage message;
  GetFakeServer()->GetLastCommitMessage(&message);

  EXPECT_TRUE(message.commit().config_params().single_client());
}

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       ShouldNotProvideTheOnlyClientFlag) {
  InjectDeviceInfoEntityToServer(/*suffix=*/1);

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(ServerDeviceInfoMatchChecker(
                  GetFakeServer(),
                  UnorderedElementsAre(HasCacheGuid(GetLocalCacheGuid()),
                                       HasCacheGuid(CacheGuidForSuffix(1))))
                  .Wait());

  sync_pb::ClientToServerMessage message;
  GetFakeServer()->GetLastCommitMessage(&message);

  EXPECT_FALSE(message.commit().config_params().single_client());
}

// PRE_* tests aren't supported on Android browser tests.
#if !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       PRE_ShouldNotSendDeviceInfoAfterBrowserRestart) {
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(
      ServerDeviceInfoMatchChecker(
          GetFakeServer(), ElementsAre(HasCacheGuid(GetLocalCacheGuid())))
          .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoSyncTest,
                       ShouldNotSendDeviceInfoAfterBrowserRestart) {
  const std::vector<sync_pb::SyncEntity> entities_before =
      fake_server_->GetSyncEntitiesByModelType(syncer::DEVICE_INFO);
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());

  bool has_local_changes = false;
  base::RunLoop run_loop;
  GetSyncService(0)->HasUnsyncedItemsForTest(
      base::BindLambdaForTesting([&has_local_changes, &run_loop](bool result) {
        has_local_changes = result;
        run_loop.Quit();
      }));
  run_loop.Run();

  const std::vector<sync_pb::SyncEntity> entities_after =
      fake_server_->GetSyncEntitiesByModelType(syncer::DEVICE_INFO);
  ASSERT_EQ(1U, entities_before.size());
  ASSERT_EQ(1U, entities_after.size());

  // Check that there are no local changes and nothing has been committed to the
  // server.
  EXPECT_FALSE(has_local_changes);
  EXPECT_EQ(entities_before.front().mtime(), entities_after.front().mtime());
}
#endif  // !defined(OS_ANDROID)

}  // namespace
