// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/send_tab_to_self_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/user_events_helper.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_controller.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entity_builder.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "components/sync_user_events/user_event_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using send_tab_to_self_helper::GetFormFieldValueById;
using send_tab_to_self_helper::PopulateFormField;
using testing::AllOf;
using testing::AnyOf;
using testing::Eq;
using testing::Field;
using testing::HasSubstr;
using testing::Not;
using testing::Property;
using testing::UnorderedElementsAre;

constexpr char kGuid[] = "kGuid";
constexpr char kUrl[] = "https://www.example.com";
constexpr char kTitle[] = "example";
constexpr char kTargetDeviceSyncCacheGuid[] = "target_guid";
constexpr char kSenderDeviceName[] = "device_name";

constexpr char kAutofillTestFormPath[] = "/autofill/autofill_test_form.html";
constexpr char kScrollPagePath[] = "/send_tab_to_self/scroll.html";
constexpr char kEmptyPagePath[] = "/empty.html";

constexpr char kName[] = "John";
constexpr char kEmail[] = "john@example.com";

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

// Tests that a SendTabToSelf entry injected on the FakeServer is downloaded and
// added to the model when sync is enabled.
IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
                       DownloadWhenSyncEnabled) {
  const GURL url(kUrl);

  fake_server_->InjectEntity(
      send_tab_to_self::SendTabToSelfEntityBuilder(url).SetGuid(kGuid).Build());

  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlChecker(
          SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)), url)
          .Wait());
}

// Tests that form fields attached to an encrypted SendTabToSelf entry are
// received and correctly populate fields in the target web page.
IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
                       ShouldReceiveFormFields) {
  ASSERT_TRUE(SetupSync());

  const GURL url = embedded_test_server()->GetURL(kAutofillTestFormPath);

  fake_server_->InjectEntity(send_tab_to_self::SendTabToSelfEntityBuilder(url)
                                 .SetGuid(kGuid)
                                 .AddFormField("NAME_FIRST", kName)
                                 .AddFormField("EMAIL_ADDRESS", kEmail)
                                 .Build(fake_server_.get()));

  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0));
  ASSERT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlChecker(service, url).Wait());

  const send_tab_to_self::SendTabToSelfEntry* entry =
      service->GetSendTabToSelfModel()->GetEntryByGUID(kGuid);
  ASSERT_NE(nullptr, entry);

  EXPECT_FALSE(entry->GetPageContext().form_field_info.fields.empty());

  // Mimic the user opening the received tab.
  content::WebContents* web_contents =
      chrome::AddAndReturnTabAt(GetBrowser(0), url, -1, true);

  send_tab_to_self::FillWebContents(web_contents, url::Origin::Create(url),
                                    entry->GetPageContext());

  // Wait for filling to complete.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return GetFormFieldValueById(web_contents, "NAME_FIRST") == kName &&
           GetFormFieldValueById(web_contents, "EMAIL_ADDRESS") == kEmail;
  }));
}

IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
                       ShouldSendFormFields) {
  const GURL url = embedded_test_server()->GetURL(kAutofillTestFormPath);
  ASSERT_TRUE(SetupSync());

  // Open tab and fill form.
  content::WebContents* web_contents =
      chrome::AddAndReturnTabAt(GetBrowser(0), url, -1, true);
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
                        Eq(base::UTF8ToUTF16(std::string_view(kName))))),
            AllOf(Field(&send_tab_to_self::PageContext::FormField::id_attribute,
                        Eq(u"EMAIL_ADDRESS")),
                  Field(&send_tab_to_self::PageContext::FormField::value,
                        Eq(base::UTF8ToUTF16(std::string_view(kEmail)))))))
        << os.str();
  }

  // Trigger sending.
  send_tab_to_self::PageContext context;
  context.form_field_info =
      send_tab_to_self::ExtractFormFieldsFromWebContents(web_contents);
  SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
      ->GetSendTabToSelfModel()
      ->SendEntry(url, kTitle, kTargetDeviceSyncCacheGuid, context,
                  send_tab_to_self::NavigationHistory(), base::DoNothing(),
                  send_tab_to_self::ShareEntryPoint::kShareSheet);

  // Wait for the entry to be committed to the server.
  ASSERT_TRUE(
      ServerCountMatchStatusChecker(syncer::SEND_TAB_TO_SELF, 1).Wait());

  // Read the proto from the fake server and verify.
  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByDataType(syncer::SEND_TAB_TO_SELF);
  ASSERT_EQ(entities.size(), 1u);
  const sync_pb::SendTabToSelfSpecifics& specifics =
      entities[0].specifics().send_tab_to_self();

  ASSERT_EQ(specifics.url(), url.spec());
  ASSERT_EQ(specifics.target_device_sync_cache_guid(),
            kTargetDeviceSyncCacheGuid);
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

// Tests that deleting a shared entry is coordinated with history deletion
// for the corresponding URL.
IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
                       DeleteSharedEntryWithHistory) {
  const GURL url(kUrl);
  const base::Time navigation_time(base::Time::Now());

  fake_server_->InjectEntity(
      send_tab_to_self::SendTabToSelfEntityBuilder(url).SetGuid(kGuid).Build());

  ASSERT_TRUE(SetupSync());

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(GetProfile(0),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history_service->AddPage(url, navigation_time, history::SOURCE_SYNCED);

  ASSERT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlChecker(
          SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)), url)
          .Wait());

  history_service->DeleteURLs({url});

  EXPECT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlDeletedChecker(
          SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)), url)
          .Wait());
}

// An unconsented primary account is not supported on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
                       ShouldCleanupOnSignout) {
  const GURL url(kUrl);

  ASSERT_TRUE(SignIn());

  send_tab_to_self::SendTabToSelfModel* model =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0))
          ->GetSendTabToSelfModel();

  ASSERT_TRUE(model->SendEntry(
      url, kTitle, kTargetDeviceSyncCacheGuid, send_tab_to_self::PageContext(),
      send_tab_to_self::NavigationHistory(), base::DoNothing(),
      send_tab_to_self::ShareEntryPoint::kShareSheet));

  GetClient(0)->SignOutPrimaryAccount();

  EXPECT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlDeletedChecker(
          SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)), url)
          .Wait());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
                       ShouldNotUploadInSyncPausedState) {
  const GURL url(kUrl);

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
      url, kTitle, kTargetDeviceSyncCacheGuid, send_tab_to_self::PageContext(),
      send_tab_to_self::NavigationHistory(), base::DoNothing(),
      send_tab_to_self::ShareEntryPoint::kShareSheet));

  EXPECT_FALSE(send_tab_to_self::ShouldDisplayEntryPoint(
      GetBrowser(0)->tab_strip_model()->GetActiveWebContents()));

  // Clear the "Sync paused" state again.
  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  } else {
    GetClient(0)->ExitSignInPendingStateForPrimaryAccount();
  }
  ASSERT_FALSE(GetSyncService(0)->GetAuthError().IsPersistentError());

  // Just checking that the test event isn't seen isn't very convincing yet,
  // because it may simply not have reached the server yet. Send
  // something else through the system to wait on before checking.
  syncer::UserEventService* user_event_service =
      browser_sync::UserEventServiceFactory::GetForProfile(GetProfile(0));
  user_event_service->RecordUserEvent(
      std::make_unique<sync_pb::UserEventSpecifics>(
          user_events_helper::CreateTestEvent(base::Time::Now())));
  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::USER_EVENTS, 1).Wait());

  // Repurpose the deleted checker to ensure `url` wasn't added.
  EXPECT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlDeletedChecker(
          SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0)), url)
          .Wait());
}

// Tests that after sync is set up, context menu display reason is valid
// for HTTP/HTTPS URLs.
IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfSyncTest,
                       ContextMenuDisplayReasonIsValidForHttpUrl) {
  ASSERT_TRUE(SetupSync());

  GURL test_url = embedded_test_server()->GetURL(kEmptyPagePath);
  content::WebContents* web_contents =
      chrome::AddAndReturnTabAt(GetBrowser(0), test_url, -1, true);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  // Verify that GetEntryPointDisplayReason() returns kInformNoTargetDevice
  // (not nullopt) for HTTP/HTTPS URLs when Sync is enabled without target
  // devices, allowing the context menu promo item to be shown.
  std::optional<send_tab_to_self::EntryPointDisplayReason> reason =
      send_tab_to_self::GetEntryPointDisplayReason(web_contents, test_url);
  ASSERT_TRUE(reason.has_value());
  EXPECT_EQ(*reason,
            send_tab_to_self::EntryPointDisplayReason::kInformNoTargetDevice);
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
    BrowserWindowInterface* browser,
    const send_tab_to_self::SendTabToSelfEntry& entry) {
  send_tab_to_self::SendTabToSelfToolbarBubbleController* controller =
      send_tab_to_self::SendTabToSelfToolbarBubbleController::From(browser);

  if (!controller->IsBubbleShowing()) {
    PinnedToolbarActions* pinned_controller =
        BrowserWindow::FromBrowser(browser)->GetPinnedToolbarActions();
    pinned_controller->ShowActionEphemerallyInToolbar(kActionSendTabToSelf,
                                                      true);
    auto anchor = pinned_controller->GetBubbleAnchor(kActionSendTabToSelf);
    controller->ShowBubble(entry, anchor);
  }

  ASSERT_TRUE(controller->IsBubbleShowing());
  controller->bubble()->OpenInNewTab();
}

// Tests that text fragment directives attached to an encrypted SendTabToSelf
// entry are received and trigger scrolling on page load.
IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfTextFragmentSyncTest,
                       ShouldReceiveTextFragment) {
  ASSERT_TRUE(SetupSync());

  const GURL url = embedded_test_server()->GetURL(kScrollPagePath);
  constexpr char kTextStart[] = "quick brown fox";

  fake_server_->InjectEntity(send_tab_to_self::SendTabToSelfEntityBuilder(url)
                                 .SetGuid(kGuid)
                                 .SetTextFragment(kTextStart)
                                 .Build(fake_server_.get()));

  send_tab_to_self::SendTabToSelfSyncService* service =
      SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile(0));
  ASSERT_TRUE(
      send_tab_to_self_helper::SendTabToSelfUrlChecker(service, url).Wait());

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
      send_tab_to_self_helper::SendTabToSelfUrlOpenedChecker(service, url)
          .Wait());

  content::WebContents* web_contents =
      web_contents_added_observer.GetWebContents();
  content::WaitForLoadStop(web_contents);

  EXPECT_EQ(web_contents->GetLastCommittedURL(), url);

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

  GURL test_url = embedded_test_server()->GetURL(kScrollPagePath);

  content::WebContents* web_contents =
      chrome::AddAndReturnTabAt(GetBrowser(0), test_url, -1, true);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  // Scroll the page so the target element's vertical midpoint moves to 35% of
  // the viewport height, where the reading position hit-test occurs.
  EXPECT_TRUE(content::ExecJs(web_contents, R"(
      new Promise(r => {
        const target = document.getElementById('target');
        const rect = target.getBoundingClientRect();
        const currentMidpointY = rect.top + rect.height / 2;
        const desiredMidpointY = window.innerHeight * 0.35;
        window.scrollBy(0, currentMidpointY - desiredMidpointY);
        requestAnimationFrame(() => requestAnimationFrame(r));
      });
    )"));

  send_tab_to_self::SendTabToSelfBubbleController* controller =
      send_tab_to_self::SendTabToSelfBubbleController::
          GetOrCreateForWebContents(web_contents);
  // Increase the timeout to avoid flakiness on slow bots.
  controller->SetSelectorGenerationTimeoutForTesting(base::Seconds(2));

  controller->OnDeviceSelected(kTargetDeviceSyncCacheGuid, kSenderDeviceName);

  ASSERT_TRUE(
      ServerCountMatchStatusChecker(syncer::SEND_TAB_TO_SELF, 1).Wait());

  const std::vector<sync_pb::SyncEntity> entities =
      fake_server_->GetSyncEntitiesByDataType(syncer::SEND_TAB_TO_SELF);
  ASSERT_EQ(entities.size(), 1u);
  const sync_pb::SendTabToSelfSpecifics& specifics =
      entities[0].specifics().send_tab_to_self();

  ASSERT_EQ(specifics.url(), test_url.spec());
  ASSERT_EQ(specifics.target_device_sync_cache_guid(),
            kTargetDeviceSyncCacheGuid);
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
  EXPECT_THAT(tf.text_start(),
              AnyOf(HasSubstr("fox"), HasSubstr("jumps"), HasSubstr("dog")));
}

IN_PROC_BROWSER_TEST_P(SingleClientSendTabToSelfTextFragmentSyncTest,
                       ShouldSendEmptyPage) {
  ASSERT_TRUE(SetupSync());

  GURL test_url = embedded_test_server()->GetURL(kEmptyPagePath);
  content::WebContents* web_contents =
      chrome::AddAndReturnTabAt(GetBrowser(0), test_url, -1, true);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  send_tab_to_self::SendTabToSelfBubbleController* controller =
      send_tab_to_self::SendTabToSelfBubbleController::
          GetOrCreateForWebContents(web_contents);

  controller->OnDeviceSelected(kTargetDeviceSyncCacheGuid, kSenderDeviceName);

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
  EXPECT_THAT(specifics.url(), Not(HasSubstr("#:~:text=")));
  ASSERT_EQ(specifics.target_device_sync_cache_guid(),
            kTargetDeviceSyncCacheGuid);

  // No scroll position since there's no text on the empty page.
  EXPECT_FALSE(specifics.page_context().has_scroll_position());
}

}  // namespace
