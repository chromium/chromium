// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/android_notification_handler.h"

#include <memory>
#include <string>

#include "base/android/application_status_listener.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/android/send_tab_to_self/android_notification_handler_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
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

std::unique_ptr<KeyedService> BuildStubSendTabToSelfSyncService(
    content::BrowserContext* context) {
  return std::make_unique<StubSendTabToSelfSyncService>();
}

}  // namespace

class AndroidNotificationHandlerBrowserTest : public AndroidBrowserTest {
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildStubSendTabToSelfSyncService));
  }

  base::test::ScopedFeatureList feature_list_{kSendTabToSelfAutoOpen};
};

IN_PROC_BROWSER_TEST_F(AndroidNotificationHandlerBrowserTest,
                       AutoOpenWhenBroughtToForeground) {
  FakeSendTabToSelfModel* model =
      static_cast<StubSendTabToSelfSyncService*>(
          SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile()))
          ->GetFakeSendTabToSelfModel();
  ASSERT_TRUE(model);

  const int initial_tab_count = GetTabListInterface()->GetTabCount();
  EntryOpenedWaiter waiter(model);

  const SendTabToSelfEntry* entry =
      model->AddEntryRemotely(GURL(kExampleUrl), "Title", "device_id",
                              PageContext(), NavigationHistory());
  const std::string guid = entry->GetGUID();

  EXPECT_FALSE(model->GetEntryByGUID(guid)->IsOpened());

  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);

  waiter.Wait();

  EXPECT_TRUE(model->GetEntryByGUID(guid)->IsOpened());
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

  FakeSendTabToSelfModel* model =
      static_cast<StubSendTabToSelfSyncService*>(
          SendTabToSelfSyncServiceFactory::GetForProfile(GetProfile()))
          ->GetFakeSendTabToSelfModel();
  ASSERT_TRUE(model);

  // Simulating application already running in foreground
  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);

  const int initial_tab_count = GetTabListInterface()->GetTabCount();
  EntryOpenedWaiter waiter(model);

  const SendTabToSelfEntry* entry =
      model->AddEntryRemotely(GURL(kExampleUrl), "Title", "device_id",
                              PageContext(), NavigationHistory());
  const std::string guid = entry->GetGUID();

  waiter.Wait();

  EXPECT_TRUE(model->GetEntryByGUID(guid)->IsOpened());
  EXPECT_EQ(initial_tab_count + 1, GetTabListInterface()->GetTabCount());
  EXPECT_EQ(GURL(kExampleUrl), GetTabListInterface()
                                   ->GetTab(initial_tab_count)
                                   ->GetContents()
                                   ->GetVisibleURL());
}

}  // namespace send_tab_to_self
