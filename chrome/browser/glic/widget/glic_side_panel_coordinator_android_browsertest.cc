// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_bridge.h"
#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_android.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicSidePanelCoordinatorAndroidBrowserTest : public GlicBrowserTest {
 public:
  GlicSidePanelCoordinatorAndroidBrowserTest() = default;
  ~GlicSidePanelCoordinatorAndroidBrowserTest() override = default;

  void TearDownOnMainThread() override {
    context_sharing::TabBottomSheetBridge::SetManagerReadyForTesting(
        std::nullopt);
    GlicBrowserTest::TearDownOnMainThread();
  }

  GlicSidePanelCoordinatorAndroid* GetSidePanelCoordinatorAndroid(
      tabs::TabInterface* tab) {
    auto* coordinator = GlicSidePanelCoordinator::GetForTab(tab);
    CHECK(coordinator);
    return static_cast<GlicSidePanelCoordinatorAndroid*>(coordinator);
  }
};

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorAndroidBrowserTest,
                       PreservesExpandedStateOnTabSwitch) {
  ASSERT_OK(OpenGlicForActiveTab());
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();
  auto* first_coordinator = GetSidePanelCoordinatorAndroid(first_tab);

  // Set the bottom sheet state to expanded.
  first_coordinator->OnOpened(/*is_expanded=*/true);
  EXPECT_EQ(first_coordinator->state(),
            GlicSidePanelCoordinator::State::kShown);

  base::test::TestFuture<GlicSidePanelCoordinator::State> first_state_future;
  base::CallbackListSubscription subscription =
      first_coordinator->AddStateCallback(
          first_state_future.GetRepeatingCallback());

  // Create a new tab and switch to it. This naturally fires
  // OnTabWillDeactivate.
  tabs::TabInterface* second_tab = CreateAndActivateTab(GetSimpleTestUrl());
  EXPECT_NE(first_tab, second_tab);

  // Verify that the first tab transitions to backgrounded and saves the
  // expanded state override.
  EXPECT_EQ(first_state_future.Take(),
            GlicSidePanelCoordinator::State::kBackgrounded);
  EXPECT_EQ(first_coordinator->state(),
            GlicSidePanelCoordinator::State::kBackgrounded);
  EXPECT_TRUE(first_coordinator->GetRestoreExpandedOnRecreationForTesting());

  // Switch back to the first tab.
  GetTabListInterface()->ActivateTab(first_tab->GetHandle());
  EXPECT_EQ(first_state_future.Take(), GlicSidePanelCoordinator::State::kPeek);
  EXPECT_EQ(first_coordinator->state(), GlicSidePanelCoordinator::State::kPeek);
  EXPECT_FALSE(first_coordinator->GetRestoreExpandedOnRecreationForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorAndroidBrowserTest,
                       PreservesPeekStateOnTabSwitch) {
  ASSERT_OK(OpenGlicForActiveTab());
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();
  auto* first_coordinator = GetSidePanelCoordinatorAndroid(first_tab);

  // Set the bottom sheet state to peeked.
  first_coordinator->OnOpened(/*is_expanded=*/false);
  EXPECT_EQ(first_coordinator->state(), GlicSidePanelCoordinator::State::kPeek);

  base::test::TestFuture<GlicSidePanelCoordinator::State> first_state_future;
  base::CallbackListSubscription subscription =
      first_coordinator->AddStateCallback(
          first_state_future.GetRepeatingCallback());

  // Create a new tab and switch to it. This naturally fires
  // OnTabWillDeactivate.
  tabs::TabInterface* second_tab = CreateAndActivateTab(GetSimpleTestUrl());
  EXPECT_NE(first_tab, second_tab);

  // Verify that the first tab transitions to backgrounded and does not set the
  // upgrade-only expanded recreation flag.
  EXPECT_EQ(first_state_future.Take(),
            GlicSidePanelCoordinator::State::kBackgrounded);
  EXPECT_EQ(first_coordinator->state(),
            GlicSidePanelCoordinator::State::kBackgrounded);
  EXPECT_FALSE(first_coordinator->GetRestoreExpandedOnRecreationForTesting());

  // Switch back to the first tab.
  GetTabListInterface()->ActivateTab(first_tab->GetHandle());
  EXPECT_EQ(first_state_future.Take(), GlicSidePanelCoordinator::State::kPeek);
  EXPECT_EQ(first_coordinator->state(), GlicSidePanelCoordinator::State::kPeek);
}

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorAndroidBrowserTest,
                       ExpandedShowNotClobberedBeforeManagerInitialized) {
  ASSERT_OK(OpenGlicForActiveTab());
  tabs::TabInterface* first_tab = GetTabListInterface()->GetActiveTab();
  auto* first_coordinator = GetSidePanelCoordinatorAndroid(first_tab);

  // Set the bottom sheet state to peeked and deactivate the tab.
  first_coordinator->OnOpened(/*is_expanded=*/false);
  EXPECT_EQ(first_coordinator->state(), GlicSidePanelCoordinator::State::kPeek);

  base::test::TestFuture<GlicSidePanelCoordinator::State> first_state_future;
  base::CallbackListSubscription subscription =
      first_coordinator->AddStateCallback(
          first_state_future.GetRepeatingCallback());

  tabs::TabInterface* second_tab = CreateAndActivateTab(GetSimpleTestUrl());
  EXPECT_NE(first_tab, second_tab);
  EXPECT_EQ(first_state_future.Take(),
            GlicSidePanelCoordinator::State::kBackgrounded);
  EXPECT_FALSE(first_coordinator->GetRestoreExpandedOnRecreationForTesting());

  // Simulate the tab being restored/activated before TabBottomSheetManager
  // finishes initializing on the window.
  context_sharing::TabBottomSheetBridge::SetManagerReadyForTesting(false);
  GetTabListInterface()->ActivateTab(first_tab->GetHandle());

  // Explicitly invoke Show(kExpanded) (e.g. from tapping an Actor
  // notification), followed by a Show(kPeeked) before manager initialization.
  GlicSidePanelCoordinator::ShowOptions expanded_options;
  expanded_options.suppress_animations = true;
  expanded_options.initial_state =
      GlicSidePanelCoordinator::ShowOptions::InitialState::kExpanded;
  first_coordinator->Show(expanded_options);

  GlicSidePanelCoordinator::ShowOptions peek_options;
  peek_options.suppress_animations = true;
  peek_options.initial_state =
      GlicSidePanelCoordinator::ShowOptions::InitialState::kPeeked;
  first_coordinator->Show(peek_options);

  // When the manager finishes initializing, the sheet must open expanded
  // (kShown) rather than being downgraded to kPeek.
  context_sharing::TabBottomSheetBridge::SetManagerReadyForTesting(
      std::nullopt);
  context_sharing::TabBottomSheetBridge::NotifyManagerInitializedForTesting(
      first_tab->GetContents()->GetTopLevelNativeWindow());
  EXPECT_EQ(first_state_future.Take(), GlicSidePanelCoordinator::State::kShown);
  EXPECT_EQ(first_coordinator->state(),
            GlicSidePanelCoordinator::State::kShown);
}

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorAndroidBrowserTest,
                       BridgesLazilyCreatedOnShow) {
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(active_tab);
  EXPECT_FALSE(
      GetSidePanelCoordinatorAndroid(active_tab)->HasBridgesForTesting());

  std::unique_ptr<content::WebContents> bg_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  tabs::TabInterface* tab = GetTabListInterface()->InsertWebContentsAt(
      /*index=*/1, std::move(bg_contents), /*should_pin=*/false,
      /*group=*/std::nullopt);
  ASSERT_TRUE(tab);
  GetTabListInterface()->ActivateTab(active_tab->GetHandle());
  ASSERT_FALSE(tab->IsActivated());
  GlicSidePanelCoordinatorAndroid* coordinator =
      GetSidePanelCoordinatorAndroid(tab);

  EXPECT_FALSE(coordinator->HasBridgesForTesting());

  std::unique_ptr<content::WebContents> guest_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  coordinator->SetWebContents(guest_web_contents.get());
  EXPECT_FALSE(coordinator->HasBridgesForTesting());

  // `Show()` on a background tab defers bridge creation until the tab
  // activates.
  coordinator->Show(GlicSidePanelCoordinator::ShowOptions());
  EXPECT_FALSE(coordinator->HasBridgesForTesting());

  GetTabListInterface()->ActivateTab(tab->GetHandle());
  EXPECT_TRUE(coordinator->HasBridgesForTesting());
  ASSERT_OK(WaitForSidePanelState(tab, GlicSidePanelCoordinator::State::kPeek));

  CloseOptions close_options;
  close_options.suppress_animations = true;
  coordinator->Close(close_options);
  ASSERT_OK(
      WaitForSidePanelState(tab, GlicSidePanelCoordinator::State::kClosed));
  coordinator->SetWebContents(nullptr);
  guest_web_contents.reset();
}

IN_PROC_BROWSER_TEST_F(GlicSidePanelCoordinatorAndroidBrowserTest,
                       GuestWebContentsDestroyedClearsJavaViews) {
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(tab);
  GlicSidePanelCoordinatorAndroid* coordinator =
      GetSidePanelCoordinatorAndroid(tab);

  std::unique_ptr<content::WebContents> guest_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfile()));
  coordinator->SetWebContents(guest_web_contents.get());

  GlicSidePanelCoordinator::ShowOptions show_options;
  show_options.suppress_animations = true;
  coordinator->Show(show_options);
  ASSERT_OK(
      WaitForSidePanelState(tab, GlicSidePanelCoordinator::State::kShown));
  EXPECT_TRUE(coordinator->HasJavaWebContentsForTesting());

  // Destroy `guest_web_contents` while the sheet is shown;
  // `WebContentsDestroyed()` must notify Java to clear its `WebContents`
  // reference cleanly.
  guest_web_contents.reset();
  EXPECT_FALSE(coordinator->HasJavaWebContentsForTesting());

  CloseOptions close_options;
  close_options.suppress_animations = true;
  coordinator->Close(close_options);
  ASSERT_OK(
      WaitForSidePanelState(tab, GlicSidePanelCoordinator::State::kClosed));
}

}  // namespace glic
