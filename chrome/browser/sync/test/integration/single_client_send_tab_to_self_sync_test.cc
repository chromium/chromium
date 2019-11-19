// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/send_tab_to_self_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {

class SingleClientSendTabToSelfSyncTest : public SyncTest {
 public:
  SingleClientSendTabToSelfSyncTest() : SyncTest(SINGLE_CLIENT) {
  }

  ~SingleClientSendTabToSelfSyncTest() override {}

 private:
  base::test::ScopedFeatureList scoped_list_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientSendTabToSelfSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest,
                       DownloadWhenSyncEnabled) {
  const std::string kUrl("https://www.example.com");
  const std::string kGuid("kGuid");
  sync_pb::EntitySpecifics specifics;
  sync_pb::SendTabToSelfSpecifics* send_tab_to_self =
      specifics.mutable_send_tab_to_self();
  send_tab_to_self->set_url(kUrl);
  send_tab_to_self->set_guid(kGuid);
  send_tab_to_self->set_shared_time_usec(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name", kGuid, specifics,
          /*creation_time=*/base::Time::Now().ToTimeT(),
          /*last_modified_time=*/base::Time::Now().ToTimeT()));

  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfUrlChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)),
                  GURL(kUrl))
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest, IsActive) {
  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfActiveChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)))
                  .Wait());
  EXPECT_TRUE(send_tab_to_self::IsUserSyncTypeActive(GetProfile(0)));
}

IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest,
                       HasValidTargetDevice) {
  ASSERT_TRUE(SetupSync());

  EXPECT_FALSE(send_tab_to_self::HasValidTargetDevice(GetProfile(0)));
}

IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest, ShouldOfferFeature) {
  ASSERT_TRUE(SetupSync());

  EXPECT_FALSE(send_tab_to_self::ShouldOfferFeature(
      GetBrowser(0)->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest,
                       DeleteSharedEntryWithHistory) {
  const std::string kUrl("https://www.example.com");
  const std::string kGuid("kGuid");
  const base::Time kNavigationTime(base::Time::Now());

  sync_pb::EntitySpecifics specifics;
  sync_pb::SendTabToSelfSpecifics* send_tab_to_self =
      specifics.mutable_send_tab_to_self();
  send_tab_to_self->set_url(kUrl);
  send_tab_to_self->set_guid(kGuid);
  send_tab_to_self->set_navigation_time_usec(
      kNavigationTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  send_tab_to_self->set_shared_time_usec(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name", kGuid, specifics,
          /*creation_time=*/base::Time::Now().ToTimeT(),
          /*last_modified_time=*/base::Time::Now().ToTimeT()));

  ASSERT_TRUE(SetupSync());

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(GetProfile(0),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history_service->AddPage(GURL(kUrl), kNavigationTime, history::SOURCE_SYNCED);

  ASSERT_TRUE(send_tab_to_self_helper::SendTabToSelfUrlChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)),
                  GURL(kUrl))
                  .Wait());

  history_service->DeleteURLs({GURL(kUrl)});

  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfUrlDeletedChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)),
                  GURL(kUrl))
                  .Wait());
}

IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest,
                       ShouldCleanupOnDisable) {
  const GURL kUrl("https://www.example.com");
  const std::string kTitle("example");
  const base::Time kTime = base::Time::FromDoubleT(1);
  const std::string kTargetDeviceSyncCacheGuid("target");

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  send_tab_to_self::SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel();

  ASSERT_TRUE(model->AddEntry(kUrl, kTitle, kTime, kTargetDeviceSyncCacheGuid));

  GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kTabs);

  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfUrlDeletedChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)),
                  GURL(kUrl))
                  .Wait());
}

}  // namespace
