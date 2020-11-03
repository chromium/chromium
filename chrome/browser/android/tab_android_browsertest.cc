// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_android.h"

#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/sync_sessions/session_sync_service_impl.h"
#include "components/sync_sessions/sync_sessions_client.h"
#include "components/sync_sessions/synced_tab_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

class TabAndroidBrowserTest : public AndroidBrowserTest {
 public:
  // AndroidBrowserTest:
  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(TabAndroidBrowserTest,
                       SetHideFutureNavigationsPropagatesToSyncedTabDelegate) {
  auto* tab = TabAndroid::FromWebContents(
      chrome_test_utils::GetActiveWebContents(this));
  ASSERT_TRUE(tab);
  // ShouldSync() returns false with about:blank. Load something to prevent
  // that.
  const GURL url =
      embedded_test_server()->GetURL("localhost", "/simple_page.html");
  ASSERT_TRUE(content::NavigateToURL(tab->web_contents(), url));
  auto* sync_service = static_cast<sync_sessions::SessionSyncServiceImpl*>(
      SessionSyncServiceFactory::GetForProfile(tab->GetProfile()));
  ASSERT_TRUE(sync_service);
  sync_sessions::SyncSessionsClient* sessions_client =
      sync_service->GetSessionsClientForTest();
  ASSERT_TRUE(sessions_client);
  sync_sessions::SyncedTabDelegate* synced_tab_delegate =
      tab->GetSyncedTabDelegate();
  ASSERT_TRUE(synced_tab_delegate);
  // Default is sync is enabled.
  EXPECT_TRUE(synced_tab_delegate->ShouldSync(sessions_client));

  // When navigations are hidden, sync should be disabled.
  tab->SetHideFutureNavigations(/* env */ nullptr, /* hide */ true);
  EXPECT_FALSE(synced_tab_delegate->ShouldSync(sessions_client));

  // Disable hiding navigations, and sync should be enabled.
  tab->SetHideFutureNavigations(/* env */ nullptr, /* hide */ false);
  EXPECT_TRUE(synced_tab_delegate->ShouldSync(sessions_client));
}
