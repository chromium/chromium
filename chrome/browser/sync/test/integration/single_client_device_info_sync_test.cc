// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>
#include <string>

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/time.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/sync_device_info/device_info_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using testing::ElementsAre;
using testing::IsSupersetOf;

MATCHER_P(HasCacheGuid, expected_cache_guid, "") {
  return arg.specifics().device_info().cache_guid() == expected_cache_guid;
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

class ServerDeviceInfoMatchChecker : public StatusChangeChecker,
                                     fake_server::FakeServer::Observer {
 public:
  using Matcher = testing::Matcher<std::vector<sync_pb::SyncEntity>>;

  ServerDeviceInfoMatchChecker(fake_server::FakeServer* fake_server,
                               const Matcher& matcher)
      : fake_server_(fake_server), matcher_(matcher) {
    fake_server->AddObserver(this);
  }

  ~ServerDeviceInfoMatchChecker() override {
    fake_server_->RemoveObserver(this);
  }

  // FakeServer::Observer overrides.
  void OnCommit(const std::string& committer_id,
                syncer::ModelTypeSet committed_model_types) override {
    if (committed_model_types.Has(syncer::DEVICE_INFO)) {
      CheckExitCondition();
    }
  }

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    std::vector<sync_pb::SyncEntity> entities =
        fake_server_->GetSyncEntitiesByModelType(syncer::DEVICE_INFO);

    testing::StringMatchResultListener result_listener;
    const bool matches =
        testing::ExplainMatchResult(matcher_, entities, &result_listener);
    *os << result_listener.str();
    return matches;
  }

 private:
  fake_server::FakeServer* const fake_server_;
  const Matcher matcher_;

  DISALLOW_COPY_AND_ASSIGN(ServerDeviceInfoMatchChecker);
};

class SingleClientDeviceInfoSyncTest : public SyncTest {
 public:
  SingleClientDeviceInfoSyncTest() : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientDeviceInfoSyncTest() override {}

  std::string GetLocalCacheGuid() {
    syncer::SyncPrefs prefs(GetProfile(0)->GetPrefs());
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

class SingleClientDeviceInfoWithTransportModeSyncTest
    : public SingleClientDeviceInfoSyncTest {
 public:
  SingleClientDeviceInfoWithTransportModeSyncTest() {
    scoped_list_.InitAndEnableFeature(switches::kSyncDeviceInfoInTransportMode);
  }

  ~SingleClientDeviceInfoWithTransportModeSyncTest() override {}

 private:
  base::test::ScopedFeatureList scoped_list_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientDeviceInfoWithTransportModeSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoWithTransportModeSyncTest,
                       CommitLocalDevice) {
  ASSERT_TRUE(SetupClients());

#if defined(OS_CHROMEOS)
  // On ChromeOS, Sync-the-feature gets started automatically once a primary
  // account is signed in. To prevent that, explicitly set SyncRequested to
  // false.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);
#endif  // defined(OS_CHROMEOS)

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

IN_PROC_BROWSER_TEST_F(SingleClientDeviceInfoWithTransportModeSyncTest,
                       DownloadRemoteDevices) {
  InjectDeviceInfoEntityToServer(/*suffix=*/1);
  InjectDeviceInfoEntityToServer(/*suffix=*/2);

  ASSERT_TRUE(SetupClients());

#if defined(OS_CHROMEOS)
  // On ChromeOS, Sync-the-feature gets started automatically once a primary
  // account is signed in. To prevent that, explicitly set SyncRequested to
  // false.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);
#endif  // defined(OS_CHROMEOS)

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

}  // namespace
