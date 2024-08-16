// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/ui/sync/browser_synced_tab_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"

namespace sync_sessions {

class StartSyncFlareMock {
 public:
  StartSyncFlareMock() = default;
  ~StartSyncFlareMock() = default;

  void StartSyncFlare(syncer::DataType type) { was_run_ = true; }

  bool was_run() { return was_run_; }

 private:
  bool was_run_ = false;
};

class SyncSessionsWebContentsRouterTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SyncSessionsWebContentsRouterTest(const SyncSessionsWebContentsRouterTest&) =
      delete;
  SyncSessionsWebContentsRouterTest& operator=(
      const SyncSessionsWebContentsRouterTest&) = delete;

 protected:
  SyncSessionsWebContentsRouterTest() = default;
  ~SyncSessionsWebContentsRouterTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    router_ =
        SyncSessionsWebContentsRouterFactory::GetInstance()->GetForProfile(
            profile());
  }

  SyncSessionsWebContentsRouter* router() { return router_; }

 private:
  raw_ptr<SyncSessionsWebContentsRouter, DanglingUntriaged> router_ = nullptr;
};

// Disabled on android due to complexity of creating a full TabAndroid object
// for a unit test. The logic being tested here isn't directly affected by
// platform-specific peculiarities.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(SyncSessionsWebContentsRouterTest, FlareNotRun) {
  StartSyncFlareMock mock;
  router()->InjectStartSyncFlare(base::BindRepeating(
      &StartSyncFlareMock::StartSyncFlare, base::Unretained(&mock)));

  // There's no delegate for the tab, so the flare shouldn't run.
  router()->NotifyTabModified(web_contents(), false);
  EXPECT_FALSE(mock.was_run());

  BrowserSyncedTabDelegate::CreateForWebContents(web_contents());

  // There's a delegate for the tab, but it's not a load completed event, so the
  // flare still shouldn't run.
  router()->NotifyTabModified(web_contents(), false);
  EXPECT_FALSE(mock.was_run());
}

// Make sure we don't crash when there's not a flare.
TEST_F(SyncSessionsWebContentsRouterTest, FlareNotSet) {
  BrowserSyncedTabDelegate::CreateForWebContents(web_contents());
  router()->NotifyTabModified(web_contents(), false);
}

TEST_F(SyncSessionsWebContentsRouterTest, FlareRunsForLoadCompleted) {
  BrowserSyncedTabDelegate::CreateForWebContents(web_contents());

  StartSyncFlareMock mock;
  router()->InjectStartSyncFlare(base::BindRepeating(
      &StartSyncFlareMock::StartSyncFlare, base::Unretained(&mock)));

  // There's a delegate for the tab, and it's a load completed event, so the
  // flare should run.
  router()->NotifyTabModified(web_contents(), true);
  EXPECT_TRUE(mock.was_run());
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace sync_sessions
