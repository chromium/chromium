// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/android_notification_handler.h"

#include <memory>
#include <string>

#include "base/android/application_status_listener.h"
#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace send_tab_to_self {

namespace {

constexpr char kExampleUrl[] = "https://www.example.com/";
constexpr char kDeviceId[] = "device_id";

std::unique_ptr<KeyedService> BuildStubSendTabToSelfSyncService(
    content::BrowserContext* context) {
  return std::make_unique<StubSendTabToSelfSyncService>();
}

}  // namespace

class AndroidNotificationHandlerBrowserTest : public AndroidBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    AndroidBrowserTest::SetUpOnMainThread();
    // Wait for the default tab and its WebContents to be fully initialized.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return GetTabListInterface() && GetTabListInterface()->GetActiveTab() &&
             GetTabListInterface()->GetActiveTab()->GetContents();
    }));
    model()->SetLocalCacheGuid(kDeviceId);
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildStubSendTabToSelfSyncService));
  }

  FakeSendTabToSelfModel* model() {
    return static_cast<StubSendTabToSelfSyncService*>(
               SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile()))
        ->GetFakeSendTabToSelfModel();
  }

  void WaitForTabCount(int expected_count) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return GetTabListInterface()->GetTabCount() == expected_count;
    }));
  }

 private:
  base::test::ScopedFeatureList feature_list_{kSendTabToSelfAutoOpen};
};

IN_PROC_BROWSER_TEST_F(AndroidNotificationHandlerBrowserTest,
                       AutoOpenWhenBroughtToForeground) {
  // Simulating application running in background (e.g. user is in another app).
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  GetTabListInterface()->GetTab(0)->GetContents()->WasHidden();

  const int initial_tab_count = GetTabListInterface()->GetTabCount();

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely({.url = GURL(kExampleUrl),
                                 .title = "Title",
                                 .target_device_cache_guid = kDeviceId});
  const std::string guid = entry->GetGUID();

  EXPECT_FALSE(model()->GetEntryByGUID(guid)->IsOpened());

  // Simulate application coming to foreground.
  GetTabListInterface()->GetTab(0)->GetContents()->WasShown();
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);

  WaitForTabCount(initial_tab_count + 1);

  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());
  EXPECT_EQ(initial_tab_count + 1, GetTabListInterface()->GetTabCount());
  EXPECT_EQ(GURL(kExampleUrl), GetTabListInterface()
                                   ->GetTab(initial_tab_count)
                                   ->GetContents()
                                   ->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(AndroidNotificationHandlerBrowserTest,
                       AutoOpenWhenReceivedInForeground) {
  AndroidNotificationHandler* handler =
      static_cast<AndroidNotificationHandler*>(
          SendTabToSelfClientServiceFactory::GetForProfile(GetProfile())
              ->GetReceivingUiHandler());
  ASSERT_TRUE(handler);

  // Simulating application already running in foreground
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);

  const int initial_tab_count = GetTabListInterface()->GetTabCount();

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely({.url = GURL(kExampleUrl),
                                 .title = "Title",
                                 .target_device_cache_guid = kDeviceId});
  const std::string guid = entry->GetGUID();

  WaitForTabCount(initial_tab_count + 1);

  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());
  EXPECT_EQ(initial_tab_count + 1, GetTabListInterface()->GetTabCount());
  EXPECT_EQ(GURL(kExampleUrl), GetTabListInterface()
                                   ->GetTab(initial_tab_count)
                                   ->GetContents()
                                   ->GetVisibleURL());
}

class AndroidNotificationHandlerModelNotReadyBrowserTest
    : public AndroidNotificationHandlerBrowserTest {
 public:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          auto service = std::make_unique<StubSendTabToSelfSyncService>();
          FakeSendTabToSelfModel* model = service->GetFakeSendTabToSelfModel();
          model->SetIsReady(false);
          model->SetLocalCacheGuid(kDeviceId);
          return service;
        }));
  }
};

IN_PROC_BROWSER_TEST_F(AndroidNotificationHandlerModelNotReadyBrowserTest,
                       AutoOpenWhenModelReadyLater) {
  // Simulating application already running in foreground
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);

  ASSERT_FALSE(model()->IsReady());

  // Add entry while model is not ready.
  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely({.url = GURL(kExampleUrl),
                                 .title = "Title",
                                 .target_device_cache_guid = kDeviceId});
  const std::string guid = entry->GetGUID();

  const int initial_tab_count = GetTabListInterface()->GetTabCount();

  // Should not open because model is not ready.
  EXPECT_FALSE(model()->GetEntryByGUID(guid)->IsOpened());
  EXPECT_EQ(initial_tab_count, GetTabListInterface()->GetTabCount());

  // Now make model ready. This should trigger auto-open.
  model()->SetIsReady(true);

  WaitForTabCount(initial_tab_count + 1);

  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());
  EXPECT_EQ(initial_tab_count + 1, GetTabListInterface()->GetTabCount());
  EXPECT_EQ(GURL(kExampleUrl), GetTabListInterface()
                                   ->GetTab(initial_tab_count)
                                   ->GetContents()
                                   ->GetVisibleURL());
}

class AndroidNotificationHandlerWithoutTabGridAutoOpenSupportBrowserTest
    : public AndroidNotificationHandlerBrowserTest {
 public:
  AndroidNotificationHandlerWithoutTabGridAutoOpenSupportBrowserTest() {
    feature_list_.InitWithFeatureState(kSendTabToSelfSupportAutoOpenInTabGrid,
                                       false);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    AndroidNotificationHandlerWithoutTabGridAutoOpenSupportBrowserTest,
    NoAutoOpenInTabSwitcher) {
  // Simulating application running in foreground with no active visible tab.
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);

  // Close the active tab so tab model has no active visible web contents.
  static_cast<TabModel*>(GetTabListInterface())->CloseTabAt(0);
  ASSERT_EQ(0, GetTabListInterface()->GetTabCount());
  const int initial_tab_count = GetTabListInterface()->GetTabCount();

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely({.url = GURL(kExampleUrl),
                                 .title = "Title",
                                 .target_device_cache_guid = kDeviceId});
  const std::string guid = entry->GetGUID();

  // Since there is no active visible web contents and flag is disabled, it
  // should NOT auto-open.
  EXPECT_FALSE(model()->GetEntryByGUID(guid)->IsOpened());
  EXPECT_EQ(initial_tab_count, GetTabListInterface()->GetTabCount());
}

class AndroidNotificationHandlerWithTabGridAutoOpenSupportBrowserTest
    : public AndroidNotificationHandlerBrowserTest {
  base::test::ScopedFeatureList feature_list_{
      kSendTabToSelfSupportAutoOpenInTabGrid};
};

IN_PROC_BROWSER_TEST_F(
    AndroidNotificationHandlerWithTabGridAutoOpenSupportBrowserTest,
    AutoOpenWhenReceivedInForegroundInTabSwitcher) {
  // Simulating application already running in foreground
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);

  // Hide the active tab's web contents to simulate tab switcher open.
  content::WebContents* active_contents =
      GetTabListInterface()->GetTab(0)->GetContents();
  active_contents->WasHidden();
  ASSERT_EQ(content::Visibility::HIDDEN, active_contents->GetVisibility());

  const int initial_tab_count = GetTabListInterface()->GetTabCount();

  const SendTabToSelfEntry* entry =
      model()->AddEntryRemotely({.url = GURL(kExampleUrl),
                                 .title = "Title",
                                 .target_device_cache_guid = kDeviceId});
  const std::string guid = entry->GetGUID();

  WaitForTabCount(initial_tab_count + 1);

  EXPECT_TRUE(model()->GetEntryByGUID(guid)->IsOpened());
  EXPECT_EQ(initial_tab_count + 1, GetTabListInterface()->GetTabCount());
  EXPECT_EQ(GURL(kExampleUrl), GetTabListInterface()
                                   ->GetTab(initial_tab_count)
                                   ->GetContents()
                                   ->GetVisibleURL());
}

}  // namespace send_tab_to_self
