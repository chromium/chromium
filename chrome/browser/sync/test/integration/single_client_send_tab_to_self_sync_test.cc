// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/send_tab_to_self_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace {

class SingleClientSendTabToSelfSyncTest : public SyncTest {
 public:
  SingleClientSendTabToSelfSyncTest() : SyncTest(SINGLE_CLIENT) {}

  SingleClientSendTabToSelfSyncTest(const SingleClientSendTabToSelfSyncTest&) =
      delete;
  SingleClientSendTabToSelfSyncTest& operator=(
      const SingleClientSendTabToSelfSyncTest&) = delete;

  ~SingleClientSendTabToSelfSyncTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    SyncTest::SetUpInProcessBrowserTestFixture();
    test_signin_client_subscription_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);
  }

 private:
  base::CallbackListSubscription test_signin_client_subscription_;
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
          /*creation_time=*/syncer::TimeToProtoTime(base::Time::Now()),
          /*last_modified_time=*/syncer::TimeToProtoTime(base::Time::Now())));

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
}

IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest,
                       HasValidTargetDevice) {
  ASSERT_TRUE(SetupSync());

  EXPECT_FALSE(SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
                   ->GetSendTabToSelfModel()
                   ->HasValidTargetDevice());
}

IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest,
                       ShouldDisplayEntryPoint) {
  ASSERT_TRUE(SetupSync());

  EXPECT_FALSE(send_tab_to_self::ShouldDisplayEntryPoint(
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
  send_tab_to_self->set_shared_time_usec(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name", kGuid, specifics,
          /*creation_time=*/syncer::TimeToProtoTime(base::Time::Now()),
          /*last_modified_time=*/syncer::TimeToProtoTime(base::Time::Now())));

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

// An unconsented primary account is not supported on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest,
                       ShouldCleanupOnSignout) {
  const GURL kUrl("https://www.example.com");
  const std::string kTitle("example");
  const std::string kTargetDeviceSyncCacheGuid("target");

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  secondary_account_helper::SignInUnconsentedAccount(
      GetProfile(0), &test_url_loader_factory_, "user@g.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  send_tab_to_self::SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel();

  ASSERT_TRUE(model->AddEntry(kUrl, kTitle, kTargetDeviceSyncCacheGuid));

  secondary_account_helper::SignOut(GetProfile(0), &test_url_loader_factory_);

  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfUrlDeletedChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)),
                  GURL(kUrl))
                  .Wait());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(SingleClientSendTabToSelfSyncTest,
                       ShouldNotUploadInSyncPausedState) {
  const GURL kUrl("https://www.example.com");
  const std::string kTitle("example");
  const std::string kTargetDeviceSyncCacheGuid("target");

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // Enter the sync paused state.
  GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  ASSERT_TRUE(GetSyncService(0)->GetAuthError().IsPersistentError());

  send_tab_to_self::SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel();

  ASSERT_FALSE(model->AddEntry(kUrl, kTitle, kTargetDeviceSyncCacheGuid));

  EXPECT_FALSE(send_tab_to_self::ShouldDisplayEntryPoint(
      GetBrowser(0)->tab_strip_model()->GetActiveWebContents()));

  // Clear the "Sync paused" state again.
  GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());

  // Just checking that we don't see test_event isn't very convincing yet,
  // because it may simply not have reached the server yet. So let's send
  // something else through the system that we can wait on before checking.
  ASSERT_TRUE(
      bookmarks_helper::AddURL(0, "What are you syncing about?",
                               GURL("https://google.com/synced-bookmark-1")));
  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::BOOKMARKS, 1).Wait());

  // Repurpose the deleted checker to ensure url wasnt added.
  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfUrlDeletedChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)),
                  GURL(kUrl))
                  .Wait());
}

}  // namespace
