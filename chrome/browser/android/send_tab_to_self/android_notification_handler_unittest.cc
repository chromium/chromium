// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/android_notification_handler.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/android/send_tab_to_self/android_notification_handler_test_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace send_tab_to_self {
namespace {

using testing::Eq;
using testing::Property;

constexpr char kExampleUrl[] = "https://www.example.com/";
constexpr char kDeviceId[] = "device_id";
// TODO(crbug.com/488072250): Allow setting the remote device name in
// FakeSendTabToSelfModel.
constexpr char kRemoteDeviceName[] = "remote_device";

std::unique_ptr<KeyedService> BuildStubSendTabToSelfSyncService(
    content::BrowserContext* context) {
  return std::make_unique<StubSendTabToSelfSyncService>();
}

// Test double of AndroidNotificationHandler that overrides JNI notification
// calls to avoid hitting the JVM.
class MockAndroidNotificationHandler : public AndroidNotificationHandler {
 public:
  using AndroidNotificationHandler::AndroidNotificationHandler;

  MOCK_METHOD(void,
              ShowNotification,
              (const SendTabToSelfEntry& entry),
              (override));
  MOCK_METHOD(void, HideNotification, (const std::string& guid), (override));
  MOCK_METHOD(void,
              ShowMessageBanner,
              (std::string_view device_name,
               content::WebContents* web_contents),
              (override));
};

class AndroidNotificationHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  AndroidNotificationHandlerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AndroidNotificationHandlerTest() override = default;

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        SendTabToSelfSyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildStubSendTabToSelfSyncService)}};
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    tab_model_ = std::make_unique<TestTabModel>(profile());
    tab_model_->SetWebContentsList({web_contents()});
    tab_model_->SetIsActiveModel(true);

    // It is expected that web_contents() has an associated TabInterface.
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         &mock_tab_interface_);

    handler_ = std::make_unique<MockAndroidNotificationHandler>(model());
    model()->SetLocalCacheGuid(kDeviceId);
  }

  void TearDown() override {
    handler_.reset();
    tab_model_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  FakeSendTabToSelfModel* model() {
    return static_cast<StubSendTabToSelfSyncService*>(
               SendTabToSelfSyncServiceFactory::GetForProfile(profile()))
        ->GetFakeSendTabToSelfModel();
  }

  MockAndroidNotificationHandler* handler() { return handler_.get(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{kSendTabToSelfAutoOpen};
  std::unique_ptr<TestTabModel> tab_model_;
  tabs::MockTabInterface mock_tab_interface_;
  std::unique_ptr<MockAndroidNotificationHandler> handler_;
};

TEST_F(AndroidNotificationHandlerTest,
       ShouldAutoOpenNewEntriesInBackgroundIfActive) {
  // Attach the tab model to simulate an active browser window.
  TabModelList::AddTabModel(tab_model_.get());

  // Add a remote entry to the model first.
  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely(GURL(kExampleUrl), "Title", kDeviceId,
                                PageContext(), NavigationHistory());
  const std::string guid = entry->GetGUID();

  EntryOpenedWaiter waiter(model());

  // Ensure no system notification is shown since Chrome is active.
  EXPECT_CALL(*handler(), ShowNotification).Times(0);
  // Expect the message banner to be displayed on the active WebContents.
  EXPECT_CALL(*handler(), ShowMessageBanner(kRemoteDeviceName, web_contents()));

  // Trigger the addition of a new entry using the public ReceivingUiHandler
  // interface.
  static_cast<ReceivingUiHandler*>(handler())->DisplayNewEntries({entry});

  // Wait for the asynchronous UI thread task and OnTabNavigated to complete.
  waiter.Wait();

  // Verify that the model was notified to mark the entry as opened.
  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());

  // Clean up the tab model from the global list.
  TabModelList::RemoveTabModel(tab_model_.get());
}

TEST_F(AndroidNotificationHandlerTest, ShouldNotAutoOpenNewEntriesIfNotActive) {
  // Do NOT add tab_model_ to TabModelList (simulating Chrome running in
  // background or not started).
  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely(GURL(kExampleUrl), "Title", kDeviceId,
                                PageContext(), NavigationHistory());
  const std::string guid = entry->GetGUID();

  base::RunLoop run_loop;
  // Expect a standard system notification to be shown with the correct GUID.
  EXPECT_CALL(*handler(), ShowNotification(
                              Property(&SendTabToSelfEntry::GetGUID, Eq(guid))))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  // Ensure no in-app message banner is displayed.
  EXPECT_CALL(*handler(), ShowMessageBanner).Times(0);

  // Trigger the addition of a new entry.
  static_cast<ReceivingUiHandler*>(handler())->DisplayNewEntries({entry});
  // Wait for the posted task to execute and show the notification.
  run_loop.Run();

  // Verify that the entry is NOT marked as opened yet.
  EXPECT_FALSE(model()->GetEntryByGUID(guid)->IsOpened());
}

TEST_F(AndroidNotificationHandlerTest,
       ShouldAutoOpenPendingEntriesInBackgroundOnActivation) {
  // Simulate multiple unread entries stored in the model.
  const SendTabToSelfEntry* entry1 =
      model()->AddEntryRemotely(GURL("https://www.google.com/"), "Google",
                                kDeviceId, PageContext(), NavigationHistory());
  const SendTabToSelfEntry* entry2 =
      model()->AddEntryRemotely(GURL("https://www.youtube.com/"), "YouTube",
                                kDeviceId, PageContext(), NavigationHistory());

  const std::string guid1 = entry1->GetGUID();
  const std::string guid2 = entry2->GetGUID();

  EntryOpenedWaiter waiter(model(), /*expected_count=*/2);

  // Ensure no new system notifications are shown during activation.
  EXPECT_CALL(*handler(), ShowNotification).Times(0);
  // Expect existing system notifications for both pending entries to be hidden.
  EXPECT_CALL(*handler(), HideNotification(guid1));
  EXPECT_CALL(*handler(), HideNotification(guid2));
  // Expect the message banner to be displayed for the opened entries.
  EXPECT_CALL(*handler(), ShowMessageBanner(kRemoteDeviceName, web_contents()));

  // Adding the tab model triggers OnTabModelAdded which executes auto-open on
  // all unread entries.
  TabModelList::AddTabModel(tab_model_.get());

  waiter.Wait();

  // Verify that both entries are marked as opened.
  EXPECT_TRUE(model()->GetEntryByGUID(guid1)->IsOpened());
  EXPECT_TRUE(model()->GetEntryByGUID(guid2)->IsOpened());

  // Clean up the tab model.
  TabModelList::RemoveTabModel(tab_model_.get());
}

TEST_F(AndroidNotificationHandlerTest,
       ShouldNotAutoOpenInOffTheRecordTabModel) {
  // Create an OffTheRecord (incognito) tab model.
  TestTabModel otr_tab_model(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  otr_tab_model.SetWebContentsList({web_contents()});
  otr_tab_model.SetIsActiveModel(true);

  // Attach the OTR tab model to the global list.
  TabModelList::AddTabModel(&otr_tab_model);

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely(GURL(kExampleUrl), "Title", kDeviceId,
                                PageContext(), NavigationHistory());
  const std::string guid = entry->GetGUID();

  base::RunLoop run_loop;
  // Expect a system notification because OTR tab models are ignored for
  // auto-open.
  EXPECT_CALL(*handler(), ShowNotification(
                              Property(&SendTabToSelfEntry::GetGUID, Eq(guid))))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  // Ensure no message banner is shown in the OTR WebContents.
  EXPECT_CALL(*handler(), ShowMessageBanner).Times(0);

  // Trigger the addition of a new entry.
  static_cast<ReceivingUiHandler*>(handler())->DisplayNewEntries({entry});
  // Wait for the notification task to complete.
  run_loop.Run();

  // Verify that the entry is NOT marked as opened.
  EXPECT_FALSE(model()->GetEntryByGUID(guid)->IsOpened());

  // Clean up the OTR tab model.
  TabModelList::RemoveTabModel(&otr_tab_model);
}

TEST_F(AndroidNotificationHandlerTest, ShouldEnqueueMessageBannerOnAutoOpen) {
  // Attach the tab model to make it active.
  TabModelList::AddTabModel(tab_model_.get());

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely(GURL(kExampleUrl), "Title", kDeviceId,
                                PageContext(), NavigationHistory());
  const std::string guid = entry->GetGUID();

  EntryOpenedWaiter waiter(model());

  // Expect the message banner to be shown upon auto-opening the entry.
  EXPECT_CALL(*handler(), ShowMessageBanner(kRemoteDeviceName, web_contents()));

  // Trigger the addition of a new entry.
  static_cast<ReceivingUiHandler*>(handler())->DisplayNewEntries({entry});

  waiter.Wait();

  // Verify that the entry is marked as opened.
  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());

  // Clean up the tab model.
  TabModelList::RemoveTabModel(tab_model_.get());
}

}  // namespace
}  // namespace send_tab_to_self
