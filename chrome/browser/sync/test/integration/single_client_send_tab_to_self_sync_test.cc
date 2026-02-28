// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/callback_list.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/send_tab_to_self_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/user_events_helper.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync_user_events/user_event_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using send_tab_to_self_helper::GetFormFieldValueById;
using testing::Eq;

class SingleClientSendTabToSelfSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientSendTabToSelfSyncTest() : SyncTest(SINGLE_CLIENT) {
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kReplaceSyncPromosWithSignInPromos);
    }
  }

  SingleClientSendTabToSelfSyncTest(const SingleClientSendTabToSelfSyncTest&) =
      delete;
  SingleClientSendTabToSelfSyncTest& operator=(
      const SingleClientSendTabToSelfSyncTest&) = delete;

  ~SingleClientSendTabToSelfSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  void SetUpInProcessBrowserTestFixture() override {
    SyncTest::SetUpInProcessBrowserTestFixture();
    test_signin_client_subscription_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription test_signin_client_subscription_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientSendTabToSelfSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
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

// TODO(crbug.com/485145029): Remove this test once the flakiness issue with
// content::WaitForHitTestData() is resolved.
IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
                       ShouldWaitForHitTestData) {
  const GURL kUrl =
      embedded_test_server()->GetURL("/autofill/autofill_test_form.html");
  ASSERT_TRUE(SetupSync());

  // Show the browser window. Otherwise, the rendering pipeline might not
  // initialize or produce frames (e.g., on Wayland headless bots), which
  // can cause tests relying on hit test data or visual state to time out.
  // TODO(crbug.com/485145029): Move this to a common codepath in SyncTest as
  // other tests may run into similar issues.
  GetBrowser(0)->window()->Show();

  // Open tab and fill form.
  content::WebContents* web_contents =
      chrome::AddAndReturnTabAt(GetBrowser(0), kUrl, -1, true);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  // TODO(crbug.com/485145029): Add a second call to WaitForHitTestData() to
  // verify it doesn't time out.
  content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(web_contents);

  // Ensure that WaitForHitTestData() completes reliably. This was added
  // temporarily to debug some test flakiness that motivated the revert
  // https://crrev.com/c/7604051.
  content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
}

IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
                       ShouldReceiveFormFields) {
  const std::string kName = "John";
  const std::string kEmail = "john@example.com";
  const GURL kUrl =
      embedded_test_server()->GetURL("/autofill/autofill_test_form.html");
  const std::string kGuid = "kGuid";

  sync_pb::EntitySpecifics specifics;
  sync_pb::SendTabToSelfSpecifics* send_tab_to_self =
      specifics.mutable_send_tab_to_self();
  send_tab_to_self->set_url(kUrl.spec());
  send_tab_to_self->set_guid(kGuid);
  send_tab_to_self->set_shared_time_usec(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  {
    sync_pb::FormField* field = send_tab_to_self->mutable_page_context()
                                    ->mutable_form_field_info()
                                    ->add_fields();
    field->set_id_attribute("NAME_FIRST");
    field->set_name_attribute("");
    field->set_form_control_type("text");
    field->set_value(kName);
  }
  {
    sync_pb::FormField* field = send_tab_to_self->mutable_page_context()
                                    ->mutable_form_field_info()
                                    ->add_fields();
    field->set_id_attribute("EMAIL_ADDRESS");
    field->set_name_attribute("");
    field->set_form_control_type("text");
    field->set_value(kEmail);
  }

  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name", kGuid, specifics,
          /*creation_time=*/syncer::TimeToProtoTime(base::Time::Now()),
          /*last_modified_time=*/syncer::TimeToProtoTime(base::Time::Now())));

  ASSERT_TRUE(SetupSync());

  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0));
  ASSERT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlChecker(service, kUrl).Wait());

  const send_tab_to_self::SendTabToSelfEntry* entry =
      service->GetSendTabToSelfModel()->GetEntryByGUID(kGuid);
  ASSERT_NE(nullptr, entry);

  // Mimic the user opening the received tab.
  content::WebContents* web_contents =
      chrome::AddAndReturnTabAt(GetBrowser(0), kUrl, -1, true);

  send_tab_to_self::FillWebContents(web_contents, url::Origin::Create(kUrl),
                                    entry->GetPageContext());

  // Wait for filling to complete.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetFormFieldValueById(web_contents, "NAME_FIRST") == kName &&
           GetFormFieldValueById(web_contents, "EMAIL_ADDRESS") == kEmail;
  }));
}

IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest, IsActive) {
  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfActiveChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)))
                  .Wait());
}

IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
                       HasValidTargetDevice) {
  ASSERT_TRUE(SetupSync());

  EXPECT_FALSE(SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
                   ->GetSendTabToSelfModel()
                   ->HasValidTargetDevice());
}

IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
                       ShouldDisplayEntryPoint) {
  ASSERT_TRUE(SetupSync());

  EXPECT_FALSE(send_tab_to_self::ShouldDisplayEntryPoint(
      GetBrowser(0)->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
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
IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
                       ShouldCleanupOnSignout) {
  const GURL kUrl("https://www.example.com");
  const std::string kTitle("example");
  const std::string kTargetDeviceSyncCacheGuid("target");

  ASSERT_TRUE(SetupClients());
  secondary_account_helper::SignInUnconsentedAccount(
      GetProfile(0), &test_url_loader_factory_, "user@gmail.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());

  send_tab_to_self::SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel();

  ASSERT_TRUE(model->AddEntry(kUrl, kTitle, kTargetDeviceSyncCacheGuid,
                              send_tab_to_self::PageContext()));

  secondary_account_helper::SignOut(GetProfile(0), &test_url_loader_factory_);

  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfUrlDeletedChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)),
                  GURL(kUrl))
                  .Wait());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
                       ShouldNotUploadInSyncPausedState) {
  const GURL kUrl("https://www.example.com");
  const std::string kTitle("example");
  const std::string kTargetDeviceSyncCacheGuid("target");

  ASSERT_TRUE(SetupSync());

  // Enter the sync paused state.
  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  } else {
    GetClient(0)->EnterSignInPendingStateForPrimaryAccount();
  }
  ASSERT_TRUE(GetSyncService(0)->GetAuthError().IsPersistentError());

  send_tab_to_self::SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel();

  ASSERT_FALSE(model->AddEntry(kUrl, kTitle, kTargetDeviceSyncCacheGuid,
                               send_tab_to_self::PageContext()));

  EXPECT_FALSE(send_tab_to_self::ShouldDisplayEntryPoint(
      GetBrowser(0)->tab_strip_model()->GetActiveWebContents()));

  // Clear the "Sync paused" state again.
  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  } else {
    GetClient(0)->ExitSignInPendingStateForPrimaryAccount();
  }
  ASSERT_FALSE(GetSyncService(0)->GetAuthError().IsPersistentError());

  // Just checking that we don't see test_event isn't very convincing yet,
  // because it may simply not have reached the server yet. So let's send
  // something else through the system that we can wait on before checking.
  syncer::UserEventService* user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));
  user_event_service->RecordUserEvent(
      std::make_unique<sync_pb::UserEventSpecifics>(
          user_events_helper::CreateTestEvent(base::Time::Now())));
  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::USER_EVENTS, 1).Wait());

  // Repurpose the deleted checker to ensure url wasn't added.
  EXPECT_TRUE(send_tab_to_self_helper::SendTabToSelfUrlDeletedChecker(
                  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)),
                  GURL(kUrl))
                  .Wait());
}

}  // namespace
