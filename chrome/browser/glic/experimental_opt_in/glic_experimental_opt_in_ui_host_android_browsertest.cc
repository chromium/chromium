// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui_host.h"

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui_host_android.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class MockGlicExperimentalOptInUIHostDelegate
    : public GlicExperimentalOptInUIHost::Delegate {
 public:
  MOCK_METHOD(void, OnUIClosed, (bool accepted), (override));
};

}  // namespace

class GlicExperimentalOptInUIHostAndroidBrowserTest : public GlicBrowserTest {
 public:
  GlicExperimentalOptInUIHostAndroidBrowserTest() = default;
  ~GlicExperimentalOptInUIHostAndroidBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInUIHostAndroidBrowserTest,
                       ShowAndSimulateAccept) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);

  testing::StrictMock<MockGlicExperimentalOptInUIHostDelegate> delegate;

  auto host = GlicExperimentalOptInUIHost::Create(GetProfile(), &delegate);
  ASSERT_TRUE(host);

  // 1. Show the UI.
  host->Show(tab->GetContents());

  // 2. We expect the delegate to be notified with `accepted = true` after
  // the simulated close completes.
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnUIClosed(true)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });

  // 3. Manually trigger the close (simulating user accepting the dialog).
  host->Close(/*accepted=*/true);

  // 4. Run the loop to wait for the PostTask to bounce back to the delegate.
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInUIHostAndroidBrowserTest,
                       ShowAndSimulateReject) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);

  testing::StrictMock<MockGlicExperimentalOptInUIHostDelegate> delegate;
  auto host = GlicExperimentalOptInUIHost::Create(GetProfile(), &delegate);
  ASSERT_TRUE(host);

  // 1. Show the UI.
  host->Show(tab->GetContents());

  // 2. We expect the delegate to be notified with `accepted = false` after
  // the simulated close completes.
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnUIClosed(false)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });

  // 3. Manually trigger the close (simulating user rejecting the dialog).
  host->Close(/*accepted=*/false);

  // 4. Run the loop to wait for the PostTask to bounce back to the delegate.
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInUIHostAndroidBrowserTest,
                       ShowAndSimulateClosingBottomSheet) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);

  testing::StrictMock<MockGlicExperimentalOptInUIHostDelegate> delegate;
  auto host = GlicExperimentalOptInUIHost::Create(GetProfile(), &delegate);
  ASSERT_TRUE(host);

  // 1. Show the UI.
  host->Show(tab->GetContents());

  // 2. We expect the delegate to be notified with `accepted = false` after
  // the simulated close completes.
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnUIClosed(false)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });

  // 3. Close bottom sheet.
  static_cast<GlicExperimentalOptInUIHostAndroid*>(host.get())
      ->SimulateClosingBottomSheetForTesting();

  // 4. Run the loop to wait for the PostTask to bounce back to the delegate.
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInUIHostAndroidBrowserTest,
                       ShowCalledTwice) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);

  testing::StrictMock<MockGlicExperimentalOptInUIHostDelegate> delegate;
  auto host = GlicExperimentalOptInUIHost::Create(GetProfile(), &delegate);
  ASSERT_TRUE(host);

  // 1. Show the UI for the first time.
  host->Show(tab->GetContents());

  // 2. Call Show again while it is already active. This should safely do
  // nothing.
  host->Show(tab->GetContents());

  // 3. We expect the delegate to be notified exactly once when the UI closes.
  // Because `delegate` is a StrictMock, any extra calls will inherently fail
  // the test.
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnUIClosed(false)).WillOnce([&run_loop]() {
    run_loop.Quit();
  });

  // 4. Manually dismiss the bottom sheet.
  static_cast<GlicExperimentalOptInUIHostAndroid*>(host.get())
      ->SimulateClosingBottomSheetForTesting();

  // 5. Wait for the async callback.
  run_loop.Run();
}

}  // namespace glic
