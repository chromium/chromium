// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/android_notification_handler.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_test_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
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

using base::Bucket;
using base::BucketsAre;
using testing::Eq;
using testing::NiceMock;
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
               int opened_tab_count,
               content::WebContents* web_contents),
              (override));
  MOCK_METHOD(bool, OpenInNativeAppIfPossible, (const GURL& url), (override));
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

    handler_ =
        std::make_unique<NiceMock<MockAndroidNotificationHandler>>(model());
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
  base::HistogramTester histogram_tester;
  // Attach the tab model to simulate an active browser window.
  TabModelList::AddTabModel(tab_model_.get());

  // Add a remote entry to the model first.
  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely({.url = GURL(kExampleUrl),
                                 .title = "Title",
                                 .target_device_cache_guid = kDeviceId});
  const std::string guid = entry->GetGUID();

  // Ensure no system notification is shown since Chrome is active.
  EXPECT_CALL(*handler(), ShowNotification).Times(0);
  // Expect the message banner to be displayed on the active WebContents.
  EXPECT_CALL(*handler(),
              ShowMessageBanner(kRemoteDeviceName, /*opened_tab_count=*/1,
                                web_contents()));

  // Trigger the addition of a new entry using the public ReceivingUiHandler
  // interface.
  handler()->DisplayNewEntries({entry});

  // Verify that the model was notified to mark the entry as opened.
  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      AutoOpenOutcome::kTabsOpenedImmediatelyInBackground, 1);

  // Clean up the tab model from the global list.
  TabModelList::RemoveTabModel(tab_model_.get());
}

TEST_F(AndroidNotificationHandlerTest, ShouldNotAutoOpenNewEntriesIfNotActive) {
  base::HistogramTester histogram_tester;
  // Do NOT add tab_model_ to TabModelList (simulating Chrome running in
  // background or not started).
  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely({.url = GURL(kExampleUrl),
                                 .title = "Title",
                                 .target_device_cache_guid = kDeviceId});
  const std::string guid = entry->GetGUID();

  // Expect a standard system notification to be shown with the correct GUID.
  EXPECT_CALL(*handler(), ShowNotification(Property(
                              &SendTabToSelfEntry::GetGUID, Eq(guid))));
  // Ensure no in-app message banner is displayed.
  EXPECT_CALL(*handler(), ShowMessageBanner).Times(0);

  // Trigger the addition of a new entry.
  handler()->DisplayNewEntries({entry});

  // Verify that the entry is NOT marked as opened yet.
  EXPECT_FALSE(model()->GetEntryByGUID(guid)->IsOpened());

  histogram_tester.ExpectUniqueSample("Sharing.SendTabToSelf.AutoOpenOutcome2",
                                      AutoOpenOutcome::kUnopenedImmediately, 1);
}

TEST_F(AndroidNotificationHandlerTest,
       ShouldAutoOpenPendingEntriesInBackgroundOnActivation) {
  base::HistogramTester histogram_tester;
  // Simulate multiple unread entries stored in the model.
  const SendTabToSelfEntry* entry1 =
      model()->AddEntryRemotely({.url = GURL("https://www.google.com/"),
                                 .title = "Google",
                                 .target_device_cache_guid = kDeviceId});
  const SendTabToSelfEntry* entry2 =
      model()->AddEntryRemotely({.url = GURL("https://www.youtube.com/"),
                                 .title = "YouTube",
                                 .target_device_cache_guid = kDeviceId});

  const std::string guid1 = entry1->GetGUID();
  const std::string guid2 = entry2->GetGUID();

  // Ensure no new system notifications are shown during activation.
  EXPECT_CALL(*handler(), ShowNotification).Times(0);
  // Expect existing system notifications for both pending entries to be hidden.
  EXPECT_CALL(*handler(), HideNotification(guid1));
  // Expect the message banner to be displayed for the opened entries.
  EXPECT_CALL(*handler(), HideNotification(guid2));
  EXPECT_CALL(*handler(),
              ShowMessageBanner(kRemoteDeviceName, /*opened_tab_count=*/2,
                                web_contents()));

  // Adding the tab model triggers OnTabModelAdded which executes auto-open on
  // all unread entries.
  TabModelList::AddTabModel(tab_model_.get());

  // Verify that both entries are marked as opened.
  EXPECT_TRUE(model()->GetEntryByGUID(guid1)->IsOpened());
  EXPECT_TRUE(model()->GetEntryByGUID(guid2)->IsOpened());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      AutoOpenOutcome::kTabsOpenedInBackgroundUponActivation, 2);

  // Clean up the tab model.
  TabModelList::RemoveTabModel(tab_model_.get());
}

TEST_F(AndroidNotificationHandlerTest,
       ShouldNotAutoOpenInOffTheRecordTabModel) {
  base::HistogramTester histogram_tester;
  // Create an OffTheRecord (incognito) tab model.
  TestTabModel otr_tab_model(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  otr_tab_model.SetWebContentsList({web_contents()});
  otr_tab_model.SetIsActiveModel(true);

  // Attach the OTR tab model to the global list.
  TabModelList::AddTabModel(&otr_tab_model);

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely({.url = GURL(kExampleUrl),
                                 .title = "Title",
                                 .target_device_cache_guid = kDeviceId});
  const std::string guid = entry->GetGUID();

  // Expect a system notification because OTR tab models are ignored for
  // auto-open.
  EXPECT_CALL(*handler(), ShowNotification(Property(
                              &SendTabToSelfEntry::GetGUID, Eq(guid))));
  // Ensure no message banner is shown in the OTR WebContents.
  EXPECT_CALL(*handler(), ShowMessageBanner).Times(0);

  // Trigger the addition of a new entry.
  handler()->DisplayNewEntries({entry});

  // Verify that the entry is NOT marked as opened.
  EXPECT_FALSE(model()->GetEntryByGUID(guid)->IsOpened());

  histogram_tester.ExpectUniqueSample("Sharing.SendTabToSelf.AutoOpenOutcome2",
                                      AutoOpenOutcome::kUnopenedImmediately, 1);

  // Clean up the OTR tab model.
  TabModelList::RemoveTabModel(&otr_tab_model);
}

TEST_F(AndroidNotificationHandlerTest, ShouldEnqueueMessageBannerOnAutoOpen) {
  base::HistogramTester histogram_tester;
  // Attach the tab model to make it active.
  TabModelList::AddTabModel(tab_model_.get());

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely({.url = GURL(kExampleUrl),
                                 .title = "Title",
                                 .target_device_cache_guid = kDeviceId});
  const std::string guid = entry->GetGUID();

  // Expect the message banner to be shown upon auto-opening the entry.
  EXPECT_CALL(*handler(),
              ShowMessageBanner(kRemoteDeviceName, /*opened_tab_count=*/1,
                                web_contents()));

  // Trigger the addition of a new entry.
  handler()->DisplayNewEntries({entry});

  // Verify that the entry is marked as opened.
  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      AutoOpenOutcome::kTabsOpenedImmediatelyInBackground, 1);

  // Clean up the tab model.
  TabModelList::RemoveTabModel(tab_model_.get());
}

class AndroidNotificationHandlerModelNotReadyTest
    : public AndroidNotificationHandlerTest {
 public:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
        SendTabToSelfSyncServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          auto service = std::make_unique<StubSendTabToSelfSyncService>();
          service->GetFakeSendTabToSelfModel()->SetIsReady(false);
          return service;
        })}};
  }
};

TEST_F(AndroidNotificationHandlerModelNotReadyTest,
       ShouldAutoOpenPendingEntriesWhenModelBecomesReady) {
  base::HistogramTester histogram_tester;
  // Attach the tab model to simulate an active browser window.
  TabModelList::AddTabModel(tab_model_.get());

  // Add a remote entry to the model.
  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely({.url = GURL(kExampleUrl),
                                 .title = "Title",
                                 .target_device_cache_guid = kDeviceId});
  const std::string guid = entry->GetGUID();
  EXPECT_FALSE(model()->GetEntryByGUID(guid)->IsOpened());

  // Ensure no system notification is shown since Chrome is active.
  EXPECT_CALL(*handler(), ShowNotification).Times(0);
  // Expect the message banner to be displayed on the active WebContents.
  EXPECT_CALL(*handler(),
              ShowMessageBanner(kRemoteDeviceName, /*opened_tab_count=*/1,
                                web_contents()));

  // Mark the model as ready. This should trigger the auto-open of the pending
  // entry.
  model()->SetIsReady(true);

  // Verify that the model was notified to mark the entry as opened.
  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      AutoOpenOutcome::kTabsOpenedInBackgroundUponActivation, 1);

  // Clean up the tab model from the global list.
  TabModelList::RemoveTabModel(tab_model_.get());
}

TEST_F(AndroidNotificationHandlerTest,
       ShouldAutoOpenPendingEntriesOnDelayedTabInitialization) {
  base::HistogramTester histogram_tester;
  // Simulate pending entries.
  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely({.url = GURL(kExampleUrl),
                                 .title = "Title",
                                 .target_device_cache_guid = kDeviceId});
  const std::string guid = entry->GetGUID();

  // Create an EMPTY owning tab model. It automatically adds itself to
  // TabModelList.
  auto empty_tab_model = std::make_unique<OwningTestTabModel>(profile());
  empty_tab_model->SetIsActiveModel(true);

  // Expect NOT to open immediately because model is empty.
  EXPECT_CALL(*handler(), ShowMessageBanner).Times(0);
  EXPECT_CALL(*handler(), HideNotification).Times(0);

  // Verify it didn't open.
  EXPECT_FALSE(model()->GetEntryByGUID(guid)->IsOpened());
  histogram_tester.ExpectTotalCount("Sharing.SendTabToSelf.AutoOpenOutcome2",
                                    0);

  // Create a new WebContents to simulate tab initialization.
  std::unique_ptr<content::WebContents> new_web_contents =
      CreateTestWebContents();
  content::WebContents* raw_web_contents = new_web_contents.get();
  tabs::TabLookupFromWebContents::CreateForWebContents(raw_web_contents,
                                                       &mock_tab_interface_);

  // Expect it to be opened eventually.
  EXPECT_CALL(*handler(), HideNotification(guid));
  EXPECT_CALL(*handler(),
              ShowMessageBanner(kRemoteDeviceName, /*opened_tab_count=*/1,
                                raw_web_contents));

  // Now simulate tab initialization (adding WebContents).
  empty_tab_model->AddTabFromWebContents(std::move(new_web_contents), 0,
                                         /*select=*/true);

  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());
  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      AutoOpenOutcome::kTabsOpenedInBackgroundUponActivation, 1);
}

TEST_F(AndroidNotificationHandlerTest,
       ShouldNotAutoOpenIfTabModelRemovedBeforeTabAdded) {
  // Simulate pending entries.
  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely({.url = GURL(kExampleUrl),
                                 .title = "Title",
                                 .target_device_cache_guid = kDeviceId});
  const std::string guid = entry->GetGUID();

  // Create an EMPTY owning tab model.
  auto empty_tab_model = std::make_unique<OwningTestTabModel>(profile());
  empty_tab_model->SetIsActiveModel(true);

  // Expect NOT to open.
  EXPECT_CALL(*handler(), ShowMessageBanner).Times(0);
  EXPECT_CALL(*handler(), HideNotification).Times(0);

  // Destroy the empty tab model.
  empty_tab_model.reset();

  // Verify it didn't open.
  EXPECT_FALSE(model()->GetEntryByGUID(guid)->IsOpened());
}

TEST_F(AndroidNotificationHandlerTest,
       ShouldAutoOpenOnFirstTabAddedWithMultipleEmptyModels) {
  // Simulate pending entries.
  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely({.url = GURL(kExampleUrl),
                                 .title = "Title",
                                 .target_device_cache_guid = kDeviceId});
  const std::string guid = entry->GetGUID();

  // Create two EMPTY owning tab models.
  auto empty_model1 = std::make_unique<OwningTestTabModel>(profile());
  empty_model1->SetIsActiveModel(true);
  auto empty_model2 = std::make_unique<OwningTestTabModel>(profile());

  // Expect NOT to open immediately.
  EXPECT_CALL(*handler(), ShowMessageBanner).Times(0);
  EXPECT_CALL(*handler(), HideNotification).Times(0);
  EXPECT_FALSE(model()->GetEntryByGUID(guid)->IsOpened());

  // Create a new WebContents to simulate tab initialization.
  std::unique_ptr<content::WebContents> new_web_contents =
      CreateTestWebContents();
  content::WebContents* raw_web_contents = new_web_contents.get();
  tabs::TabLookupFromWebContents::CreateForWebContents(raw_web_contents,
                                                       &mock_tab_interface_);

  // Expect it to be opened eventually.
  EXPECT_CALL(*handler(), HideNotification(guid));
  EXPECT_CALL(*handler(),
              ShowMessageBanner(kRemoteDeviceName, /*opened_tab_count=*/1,
                                raw_web_contents));

  // Now simulate tab initialization on the first model.
  empty_model1->AddTabFromWebContents(std::move(new_web_contents), 0,
                                      /*select=*/true);

  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());
}

TEST_F(AndroidNotificationHandlerTest,
       ShouldPreferActiveModelOverInactiveModelWithTabs) {
  // Create an INACTIVE model with a tab FIRST so it appears earlier in
  // TabModelList.
  auto inactive_model = std::make_unique<OwningTestTabModel>(profile());
  inactive_model->SetIsActiveModel(false);
  std::unique_ptr<content::WebContents> inactive_web_contents =
      CreateTestWebContents();
  tabs::TabLookupFromWebContents::CreateForWebContents(
      inactive_web_contents.get(), &mock_tab_interface_);
  inactive_model->AddTabFromWebContents(std::move(inactive_web_contents), 0,
                                        /*select=*/true);

  // Add the ACTIVE tab_model_ to TabModelList.
  TabModelList::AddTabModel(tab_model_.get());

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely(GURL(kExampleUrl), "Title", kDeviceId,
                                PageContext(), NavigationHistory());
  const std::string guid = entry->GetGUID();

  // Expect message banner to be shown on active web_contents(), NOT on
  // inactive_model's tab.
  EXPECT_CALL(*handler(), ShowNotification).Times(0);
  EXPECT_CALL(*handler(),
              ShowMessageBanner(kRemoteDeviceName, /*opened_tab_count=*/1,
                                web_contents()));

  handler()->DisplayNewEntries({entry});

  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());

  TabModelList::RemoveTabModel(tab_model_.get());
}

TEST_F(AndroidNotificationHandlerTest,
       ShouldFallbackToInactiveModelWithActiveWebContentsIfNoActiveModel) {
  auto inactive_model = std::make_unique<OwningTestTabModel>(profile());
  inactive_model->SetIsActiveModel(false);
  std::unique_ptr<content::WebContents> inactive_web_contents =
      CreateTestWebContents();
  content::WebContents* raw_web_contents = inactive_web_contents.get();
  tabs::TabLookupFromWebContents::CreateForWebContents(raw_web_contents,
                                                       &mock_tab_interface_);
  inactive_model->AddTabFromWebContents(std::move(inactive_web_contents), 0,
                                        /*select=*/true);

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely(GURL(kExampleUrl), "Title", kDeviceId,
                                PageContext(), NavigationHistory());
  const std::string guid = entry->GetGUID();

  EXPECT_CALL(*handler(), ShowNotification).Times(0);
  EXPECT_CALL(*handler(),
              ShowMessageBanner(kRemoteDeviceName, /*opened_tab_count=*/1,
                                raw_web_contents));

  handler()->DisplayNewEntries({entry});

  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());
}

class AndroidNotificationHandlerWithoutTabGridAutoOpenSupportTest
    : public AndroidNotificationHandlerTest {
 public:
  AndroidNotificationHandlerWithoutTabGridAutoOpenSupportTest() {
    feature_list_.InitWithFeatureState(kSendTabToSelfSupportAutoOpenInTabGrid,
                                       false);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class AndroidNotificationHandlerWithTabGridAutoOpenSupportTest
    : public AndroidNotificationHandlerTest {
  base::test::ScopedFeatureList feature_list_{
      kSendTabToSelfSupportAutoOpenInTabGrid};
};

TEST_F(AndroidNotificationHandlerWithoutTabGridAutoOpenSupportTest,
       ShouldNotAutoOpenInTabSwitcher) {
  base::HistogramTester histogram_tester;
  TabModelList::AddTabModel(tab_model_.get());

  // Hide the active WebContents (simulating tab switcher open).
  web_contents()->WasHidden();
  ASSERT_EQ(content::Visibility::HIDDEN, web_contents()->GetVisibility());

  // Should fallback to notification.
  EXPECT_CALL(*handler(), ShowNotification(Property(&SendTabToSelfEntry::GetURL,
                                                    Eq(GURL(kExampleUrl)))));
  EXPECT_CALL(*handler(), ShowMessageBanner).Times(0);

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely(GURL(kExampleUrl), "Title", kDeviceId,
                                PageContext(), NavigationHistory());

  handler()->DisplayNewEntries({entry});

  EXPECT_FALSE(model()->GetEntryByGUID(entry->GetGUID())->IsOpened());

  histogram_tester.ExpectUniqueSample("Sharing.SendTabToSelf.AutoOpenOutcome2",
                                      AutoOpenOutcome::kUnopenedImmediately, 1);

  TabModelList::RemoveTabModel(tab_model_.get());
}

TEST_F(AndroidNotificationHandlerWithTabGridAutoOpenSupportTest,
       ShouldAutoOpenInTabSwitcher) {
  base::HistogramTester histogram_tester;
  // Attach the tab model.
  TabModelList::AddTabModel(tab_model_.get());

  // Hide the active WebContents (simulating tab switcher open).
  web_contents()->WasHidden();
  ASSERT_EQ(content::Visibility::HIDDEN, web_contents()->GetVisibility());

  // Ensure application status is foreground.
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  base::RunLoop().RunUntilIdle();

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely(GURL(kExampleUrl), "Title", kDeviceId,
                                PageContext(), NavigationHistory());
  const std::string guid = entry->GetGUID();

  EXPECT_CALL(*handler(), ShowNotification).Times(0);
  EXPECT_CALL(*handler(),
              ShowMessageBanner(kRemoteDeviceName, /*opened_tab_count=*/1,
                                web_contents()));

  handler()->DisplayNewEntries({entry});

  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      AutoOpenOutcome::kTabsOpenedImmediatelyInBackground, 1);

  TabModelList::RemoveTabModel(tab_model_.get());
}

class AndroidNotificationHandlerWithNativeAppSupportTest
    : public AndroidNotificationHandlerTest {
  base::test::ScopedFeatureList feature_list{kSendTabToSelfOpenNativeApp};
};

TEST_F(AndroidNotificationHandlerWithNativeAppSupportTest,
       ShouldAutoOpenWithNativeAppFlagEnabled) {
  base::HistogramTester histogram_tester;
  TabModelList::AddTabModel(tab_model_.get());

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely(GURL(kExampleUrl), "Title", kDeviceId,
                                PageContext(), NavigationHistory());
  const std::string guid = entry->GetGUID();

  EXPECT_CALL(*handler(), ShowNotification).Times(0);
  EXPECT_CALL(*handler(),
              ShowMessageBanner(kRemoteDeviceName, /*opened_tab_count=*/1,
                                web_contents()));

  handler()->DisplayNewEntries({entry});

  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      AutoOpenOutcome::kTabsOpenedImmediatelyInBackground, 1);

  TabModelList::RemoveTabModel(tab_model_.get());
}

TEST_F(AndroidNotificationHandlerWithNativeAppSupportTest,
       ShouldOpenInNativeAppImmediately) {
  base::HistogramTester histogram_tester;
  TabModelList::AddTabModel(tab_model_.get());

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely(GURL(kExampleUrl), "Title", kDeviceId,
                                PageContext(), NavigationHistory());
  const std::string guid = entry->GetGUID();

  // There should be no notification nor message banner because it opened in the
  // native app.
  EXPECT_CALL(*handler(), ShowNotification).Times(0);
  EXPECT_CALL(*handler(), ShowMessageBanner).Times(0);
  EXPECT_CALL(*handler(), OpenInNativeAppIfPossible(GURL(kExampleUrl)))
      .WillOnce(testing::Return(true));

  handler()->DisplayNewEntries({entry});

  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      AutoOpenOutcome::kOpenedInNativeAppImmediately, 1);

  TabModelList::RemoveTabModel(tab_model_.get());
}

TEST_F(AndroidNotificationHandlerTest,
       ShouldOpenOnlyFirstInNativeAppAndNotifyForSecond) {
  base::test::ScopedFeatureList feature_list(kSendTabToSelfOpenNativeApp);

  base::HistogramTester histogram_tester;
  TabModelList::AddTabModel(tab_model_.get());

  const SendTabToSelfEntry* entry1 =
      model()->AddEntryRemotely(GURL("https://www.example.com/app1"), "Title 1",
                                kDeviceId, PageContext(), NavigationHistory());
  const SendTabToSelfEntry* entry2 =
      model()->AddEntryRemotely(GURL("https://www.example.com/app2"), "Title 2",
                                kDeviceId, PageContext(), NavigationHistory());
  const std::string guid1 = entry1->GetGUID();
  const std::string guid2 = entry2->GetGUID();

  // The first entry should open in the native app immediately; the second
  // should show a notification. There should be no message banners.
  EXPECT_CALL(*handler(),
              OpenInNativeAppIfPossible(GURL("https://www.example.com/app1")))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*handler(),
              OpenInNativeAppIfPossible(GURL("https://www.example.com/app2")))
      .Times(0);
  EXPECT_CALL(*handler(), ShowNotification(Property(
                              &SendTabToSelfEntry::GetGUID, Eq(guid2))));
  EXPECT_CALL(*handler(), ShowMessageBanner).Times(0);

  handler()->DisplayNewEntries({entry1, entry2});

  EXPECT_TRUE(model()->GetEntryByGUID(guid1)->IsOpened());
  EXPECT_FALSE(model()->GetEntryByGUID(guid2)->IsOpened());

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Sharing.SendTabToSelf.AutoOpenOutcome2"),
      BucketsAre(Bucket(AutoOpenOutcome::kOpenedInNativeAppImmediately, 1),
                 Bucket(AutoOpenOutcome::kUnopenedImmediately, 1)));

  TabModelList::RemoveTabModel(tab_model_.get());
}

class AndroidNotificationHandlerModelNotReadyWithNativeAppSupportTest
    : public AndroidNotificationHandlerModelNotReadyTest {
  base::test::ScopedFeatureList feature_list{kSendTabToSelfOpenNativeApp};
};

TEST_F(AndroidNotificationHandlerModelNotReadyWithNativeAppSupportTest,
       ShouldOpenInNativeAppOnModelReady) {
  base::HistogramTester histogram_tester;
  TabModelList::AddTabModel(tab_model_.get());

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely(GURL(kExampleUrl), "Title", kDeviceId,
                                PageContext(), NavigationHistory());
  const std::string guid = entry->GetGUID();

  EXPECT_CALL(*handler(), ShowNotification).Times(0);
  EXPECT_CALL(*handler(), ShowMessageBanner).Times(0);
  EXPECT_CALL(*handler(), OpenInNativeAppIfPossible(GURL(kExampleUrl)))
      .WillOnce(testing::Return(true));

  // Mark the model as ready. This should trigger the auto-open of the pending
  // entry.
  model()->SetIsReady(true);

  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.AutoOpenOutcome2",
      AutoOpenOutcome::kOpenedInNativeAppUponActivation, 1);

  TabModelList::RemoveTabModel(tab_model_.get());
}
}  // namespace
}  // namespace send_tab_to_self
