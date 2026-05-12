// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/base64.h"
#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_scroll_observer.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/send_tab_to_self_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/user_events_helper.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_toolbar_icon_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/loopback_server/persistent_unique_client_entity.h"
#include "components/sync/nigori/cryptographer_impl.h"
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
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using send_tab_to_self_helper::GetFormFieldValueById;
using send_tab_to_self_helper::PopulateFormField;
using testing::AllOf;
using testing::Eq;
using testing::Field;
using testing::HasSubstr;
using testing::Property;
using testing::UnorderedElementsAre;

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
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

IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
                       ShouldReceiveFormFields) {
  ASSERT_TRUE(SetupSync());

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
  ASSERT_TRUE(
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          base::Base64Encode(fake_server_->GetKeystoreKeys().back()),
          syncer::KeyDerivationParams::CreateForPbkdf2())
          ->Encrypt(send_tab_to_self->page_context(),
                    send_tab_to_self->mutable_encrypted_page_context()));
  send_tab_to_self->clear_page_context();

  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name", kGuid, specifics,
          /*creation_time=*/syncer::TimeToProtoTime(base::Time::Now()),
          /*last_modified_time=*/syncer::TimeToProtoTime(base::Time::Now())));

  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0));
  ASSERT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlChecker(service, kUrl).Wait());

  const send_tab_to_self::SendTabToSelfEntry* entry =
      service->GetSendTabToSelfModel()->GetEntryByGUID(kGuid);
  ASSERT_NE(nullptr, entry);

  EXPECT_FALSE(entry->GetPageContext().form_field_info.fields.empty());

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

IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
                       ShouldSendFormFields) {
  const std::string kName = "John";
  const std::string kEmail = "john@example.com";
  const GURL kUrl =
      embedded_test_server()->GetURL("/autofill/autofill_test_form.html");
  ASSERT_TRUE(SetupSync());

  // Open tab and fill form.
  content::WebContents* web_contents =
      chrome::AddAndReturnTabAt(GetBrowser(0), kUrl, -1, true);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  // Wait for Autofill to cache the form fields.
  ASSERT_TRUE(send_tab_to_self_helper::AutofillFieldsSeenChecker(
                  web_contents, {{"NAME_FIRST", ""}, {"EMAIL_ADDRESS", ""}})
                  .Wait());

  ASSERT_TRUE(PopulateFormField(web_contents, "NAME_FIRST", kName));
  ASSERT_TRUE(PopulateFormField(web_contents, "EMAIL_ADDRESS", kEmail));

  // Wait for Autofill to catch up with the values.
  ASSERT_TRUE(
      send_tab_to_self_helper::AutofillFieldsSeenChecker(
          web_contents, {{"NAME_FIRST", kName}, {"EMAIL_ADDRESS", kEmail}})
          .Wait());

  // Verify the behavior of form field extraction, ahead of exercising the
  // "real" sending logic below.
  {
    std::stringstream os;
    EXPECT_THAT(
        send_tab_to_self::ExtractFormFieldsFromWebContentsForTesting(
            web_contents, os)
            .fields,
        UnorderedElementsAre(
            AllOf(Field(&send_tab_to_self::PageContext::FormField::id_attribute,
                        Eq(u"NAME_FIRST")),
                  Field(&send_tab_to_self::PageContext::FormField::value,
                        Eq(base::UTF8ToUTF16(kName)))),
            AllOf(Field(&send_tab_to_self::PageContext::FormField::id_attribute,
                        Eq(u"EMAIL_ADDRESS")),
                  Field(&send_tab_to_self::PageContext::FormField::value,
                        Eq(base::UTF8ToUTF16(kEmail))))))
        << os.str();
  }

  // Trigger sending.
  const std::string target_guid = "target_guid";
  send_tab_to_self::PageContext context;
  context.form_field_info =
      send_tab_to_self::ExtractFormFieldsFromWebContents(web_contents);
  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
      ->GetSendTabToSelfModel()
      ->SendEntry(kUrl, "example", target_guid, context,
                  send_tab_to_self::NavigationHistory(), base::DoNothing());

  // Wait for the entry to be committed to the server.
  ASSERT_TRUE(
      ServerCountMatchStatusChecker(syncer::SEND_TAB_TO_SELF, 1).Wait());

  // Read the proto from the fake server and verify.
  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByDataType(syncer::SEND_TAB_TO_SELF);
  ASSERT_EQ(entities.size(), 1u);
  const sync_pb::SendTabToSelfSpecifics& specifics =
      entities[0].specifics().send_tab_to_self();

  ASSERT_EQ(specifics.url(), kUrl.spec());
  ASSERT_EQ(specifics.target_device_sync_cache_guid(), target_guid);
  EXPECT_FALSE(specifics.has_page_context());
  ASSERT_TRUE(specifics.has_encrypted_page_context());

  sync_pb::PageContext decrypted_context;
  ASSERT_TRUE(
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          base::Base64Encode(fake_server_->GetKeystoreKeys().back()),
          syncer::KeyDerivationParams::CreateForPbkdf2())
          ->Decrypt(specifics.encrypted_page_context(), &decrypted_context));

  ASSERT_TRUE(decrypted_context.has_form_field_info());

  const sync_pb::FormFieldInfo& form_field_info =
      decrypted_context.form_field_info();

  EXPECT_THAT(
      form_field_info.fields(),
      UnorderedElementsAre(
          AllOf(Property(&sync_pb::FormField::id_attribute, Eq("NAME_FIRST")),
                Property(&sync_pb::FormField::value, Eq(kName))),
          AllOf(
              Property(&sync_pb::FormField::id_attribute, Eq("EMAIL_ADDRESS")),
              Property(&sync_pb::FormField::value, Eq(kEmail)))));
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

  ASSERT_TRUE(SignIn());

  send_tab_to_self::SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel();

  ASSERT_TRUE(model->SendEntry(
      kUrl, kTitle, kTargetDeviceSyncCacheGuid, send_tab_to_self::PageContext(),
      send_tab_to_self::NavigationHistory(), base::DoNothing()));

  GetClient(0)->SignOutPrimaryAccount();

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

  ASSERT_FALSE(model->SendEntry(
      kUrl, kTitle, kTargetDeviceSyncCacheGuid, send_tab_to_self::PageContext(),
      send_tab_to_self::NavigationHistory(), base::DoNothing()));

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

class SingleClientSendTabToSelfTextFragmentSyncTest
    : public SingleClientSendTabToSelfSyncTest {
 public:
  SingleClientSendTabToSelfTextFragmentSyncTest() {
    text_fragment_feature_list_.InitAndEnableFeature(
        send_tab_to_self::kSendTabToSelfPropagateScrollPosition);
  }

 private:
  base::test::ScopedFeatureList text_fragment_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         SingleClientSendTabToSelfTextFragmentSyncTest,
                         GetSyncTestModes(),
                         testing::PrintToStringParamName());

void SimulateOpeningReceivedTab(
    Browser* browser,
    const send_tab_to_self::SendTabToSelfEntry& entry) {
  send_tab_to_self::SendTabToSelfToolbarBubbleController* controller =
      send_tab_to_self::SendTabToSelfToolbarBubbleController::From(browser);

  if (!controller->IsBubbleShowing()) {
    PinnedToolbarActions* pinned_controller =
        browser->browser_window_features()->pinned_toolbar_actions();
    pinned_controller->ShowActionEphemerallyInToolbar(kActionSendTabToSelf,
                                                      true);
    auto anchor = pinned_controller->GetBubbleAnchor(kActionSendTabToSelf);
    controller->ShowBubble(entry, anchor);
  }

  ASSERT_TRUE(controller->IsBubbleShowing());
  controller->bubble()->OpenInNewTab();
}

IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfTextFragmentSyncTest,
                       ShouldReceiveTextFragment) {
  ASSERT_TRUE(SetupSync());

  const GURL kUrl =
      embedded_test_server()->GetURL("/send_tab_to_self/scroll.html");
  constexpr char kGuid[] = "kGuid";
  constexpr char kTextStart[] = "quick brown fox";

  sync_pb::EntitySpecifics specifics;
  sync_pb::SendTabToSelfSpecifics* send_tab_to_self =
      specifics.mutable_send_tab_to_self();
  send_tab_to_self->set_url(kUrl.spec());
  send_tab_to_self->set_guid(kGuid);
  send_tab_to_self->set_shared_time_usec(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  sync_pb::ScrollPosition* scroll_position =
      send_tab_to_self->mutable_page_context()->mutable_scroll_position();
  sync_pb::TextFragmentData* text_fragment =
      scroll_position->mutable_text_fragment();
  text_fragment->set_text_start(kTextStart);

  ASSERT_TRUE(
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          base::Base64Encode(fake_server_->GetKeystoreKeys().back()),
          syncer::KeyDerivationParams::CreateForPbkdf2())
          ->Encrypt(send_tab_to_self->page_context(),
                    send_tab_to_self->mutable_encrypted_page_context()));
  send_tab_to_self->clear_page_context();

  fake_server_->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name", kGuid, specifics,
          /*creation_time=*/syncer::TimeToProtoTime(base::Time::Now()),
          /*last_modified_time=*/syncer::TimeToProtoTime(base::Time::Now())));

  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0));
  ASSERT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlChecker(service, kUrl).Wait());

  const send_tab_to_self::SendTabToSelfEntry* entry =
      service->GetSendTabToSelfModel()->GetEntryByGUID(kGuid);
  ASSERT_NE(nullptr, entry);

  const send_tab_to_self::TextFragmentData& received_fragment =
      entry->GetPageContext().scroll_position.text_fragment;
  EXPECT_EQ(kTextStart, received_fragment.text_start);

  content::WebContentsAddedObserver web_contents_added_observer;

  SimulateOpeningReceivedTab(GetBrowser(0), *entry);

  // Wait until the entry is marked opened in the model.
  ASSERT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlOpenedChecker(service, kUrl)
          .Wait());

  content::WebContents* web_contents =
      web_contents_added_observer.GetWebContents();
  content::WaitForLoadStop(web_contents);

  EXPECT_EQ(web_contents->GetLastCommittedURL(), kUrl);

  // Wait for the scroll to be applied and verify it.
  // The text fragment is in the middle of a very long test page.
  // Check that it's within the viewport.
  ASSERT_TRUE(send_tab_to_self_helper::SendTabToSelfScrollChecker(web_contents,
                                                                  "target")
                  .Wait());
}

IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfTextFragmentSyncTest,
                       ShouldSendTextFragment) {
  ASSERT_TRUE(SetupSync());

  GURL test_url =
      embedded_test_server()->GetURL("/send_tab_to_self/scroll.html");

  content::WebContents* web_contents =
      chrome::AddAndReturnTabAt(GetBrowser(0), test_url, -1, true);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  // Scroll to the content so it's precisely in the center of the viewport.
  EXPECT_TRUE(content::ExecJs(web_contents, R"(
      new Promise(r => {
        document.getElementById('target').scrollIntoView({
          behavior: 'instant',
          block: 'center',
          inline: 'center'
        });
        requestAnimationFrame(() => requestAnimationFrame(r));
      });
    )"));

  send_tab_to_self::SendTabToSelfBubbleController* controller =
      send_tab_to_self::SendTabToSelfBubbleController::
          GetOrCreateForWebContents(web_contents);
  // Increase the timeout to avoid flakiness on slow bots.
  controller->SetSelectorGenerationTimeoutForTesting(base::Seconds(2));

  constexpr char kTargetGuid[] = "target_guid";
  controller->OnDeviceSelected(kTargetGuid, "device_name");

  ASSERT_TRUE(
      ServerCountMatchStatusChecker(syncer::SEND_TAB_TO_SELF, 1).Wait());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByDataType(syncer::SEND_TAB_TO_SELF);
  ASSERT_EQ(entities.size(), 1u);
  const sync_pb::SendTabToSelfSpecifics& specifics =
      entities[0].specifics().send_tab_to_self();

  ASSERT_EQ(specifics.url(), test_url.spec());
  ASSERT_EQ(specifics.target_device_sync_cache_guid(), kTargetGuid);
  EXPECT_FALSE(specifics.has_page_context());
  ASSERT_TRUE(specifics.has_encrypted_page_context());

  sync_pb::PageContext decrypted_context;
  ASSERT_TRUE(
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          base::Base64Encode(fake_server_->GetKeystoreKeys().back()),
          syncer::KeyDerivationParams::CreateForPbkdf2())
          ->Decrypt(specifics.encrypted_page_context(), &decrypted_context));

  ASSERT_TRUE(decrypted_context.has_scroll_position());
  ASSERT_TRUE(decrypted_context.scroll_position().has_text_fragment());

  // Text fragment generation can be non-deterministic depending on the exact
  // viewport size and layout on different platforms/bots.
  const sync_pb::TextFragmentData& tf =
      decrypted_context.scroll_position().text_fragment();
  EXPECT_THAT(tf.text_start(), testing::AnyOf(testing::HasSubstr("fox"),
                                              testing::HasSubstr("jumps"),
                                              testing::HasSubstr("dog")));
}

IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfTextFragmentSyncTest,
                       ShouldSendEmptyPage) {
  ASSERT_TRUE(SetupSync());

  GURL test_url = embedded_test_server()->GetURL("/empty.html");
  content::WebContents* web_contents =
      chrome::AddAndReturnTabAt(GetBrowser(0), test_url, -1, true);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  send_tab_to_self::SendTabToSelfBubbleController* controller =
      send_tab_to_self::SendTabToSelfBubbleController::
          GetOrCreateForWebContents(web_contents);

  constexpr char kTargetGuid[] = "target_guid";
  controller->OnDeviceSelected(kTargetGuid, "device_name");

  ASSERT_TRUE(
      ServerCountMatchStatusChecker(syncer::SEND_TAB_TO_SELF, 1).Wait());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByDataType(syncer::SEND_TAB_TO_SELF);
  ASSERT_EQ(entities.size(), 1u);
  const sync_pb::SendTabToSelfSpecifics& specifics =
      entities[0].specifics().send_tab_to_self();

  ASSERT_EQ(specifics.url(), test_url.spec());
  // Verify that no text fragment data is erroneously generated or appended
  // to the URL.
  EXPECT_THAT(specifics.url(), testing::Not(testing::HasSubstr("#:~:text=")));
  ASSERT_EQ(specifics.target_device_sync_cache_guid(), kTargetGuid);

  // No scroll position since there's no text on the empty page.
  EXPECT_FALSE(specifics.page_context().has_scroll_position());
}

}  // namespace
