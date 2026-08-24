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
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

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
#if !BUILDFLAG(IS_ANDROID)
    // Associate the contents with the tab so that delegate lookups can find
    // it. In production this association is maintained by TabModel.
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(), &tab_);
    ON_CALL(tab_, GetUnownedUserDataHost())
        .WillByDefault(::testing::ReturnRef(user_data_host_));
#endif
  }

#if !BUILDFLAG(IS_ANDROID)
  void CreateDelegate() {
    delegate_ =
        std::make_unique<BrowserSyncedTabDelegate>(tab_, web_contents());
  }
#endif

  void TearDown() override {
#if !BUILDFLAG(IS_ANDROID)
    delegate_.reset();
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

  SyncSessionsWebContentsRouter* router() { return router_; }

 private:
  raw_ptr<SyncSessionsWebContentsRouter, DanglingUntriaged> router_ = nullptr;
#if !BUILDFLAG(IS_ANDROID)
  ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface tab_;
  std::unique_ptr<BrowserSyncedTabDelegate> delegate_;
#endif
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

  CreateDelegate();

  // There's a delegate for the tab, but it's not a load completed event, so the
  // flare still shouldn't run.
  router()->NotifyTabModified(web_contents(), false);
  EXPECT_FALSE(mock.was_run());
}

// Make sure we don't crash when there's not a flare.
TEST_F(SyncSessionsWebContentsRouterTest, FlareNotSet) {
  CreateDelegate();
  router()->NotifyTabModified(web_contents(), false);
}

TEST_F(SyncSessionsWebContentsRouterTest, FlareRunsForLoadCompleted) {
  CreateDelegate();

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
