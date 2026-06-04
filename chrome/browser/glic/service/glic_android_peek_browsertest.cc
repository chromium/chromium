// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_android.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "content/public/test/browser_test.h"

namespace glic {

class GlicAndroidPeekBrowserTest : public GlicBrowserTest {
 public:
  GlicAndroidPeekBrowserTest() = default;
  ~GlicAndroidPeekBrowserTest() override = default;

  GlicSidePanelCoordinatorAndroid* GetSidePanelCoordinatorAndroid(
      tabs::TabInterface* tab) {
    auto* coordinator = GlicSidePanelCoordinator::GetForTab(tab);
    CHECK(coordinator);
    return static_cast<GlicSidePanelCoordinatorAndroid*>(coordinator);
  }
};

IN_PROC_BROWSER_TEST_F(GlicAndroidPeekBrowserTest,
                       DaisyChainedTabsStartInPeekState) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());

  instance->CreateTab(GetSimpleTestUrl(), /*open_in_background=*/false,
                      std::nullopt, base::DoNothing());

  tabs::TabInterface* new_tab = GetTabListInterface()->GetActiveTab();
  GlicSidePanelCoordinator* coordinator =
      GlicSidePanelCoordinator::GetForTab(new_tab);
  ASSERT_TRUE(coordinator);
  EXPECT_EQ(coordinator->state(), GlicSidePanelCoordinator::State::kPeek);

  EXPECT_TRUE(RunUntil(
      [&]() { return instance->GetActiveEmbedderTabForTesting() == nullptr; },
      "Active embedder tab didn't become null after opening a new tab in peek "
      "state."));
}

IN_PROC_BROWSER_TEST_F(GlicAndroidPeekBrowserTest,
                       MaintainPeekStateOnWindowActivation) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());

  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  auto* coordinator_android = GetSidePanelCoordinatorAndroid(active_tab);
  coordinator_android->OnOpened(false);  // Set to PEEK
  EXPECT_EQ(coordinator_android->state(),
            GlicSidePanelCoordinator::State::kPeek);

  instance->OnBrowserActivated(active_tab->GetBrowserWindowInterface());

  EXPECT_EQ(coordinator_android->state(),
            GlicSidePanelCoordinator::State::kPeek);
  EXPECT_FALSE(instance->GetActiveEmbedderTabForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicAndroidPeekBrowserTest,
                       MaintainPeekStateOnTabSwitch) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());

  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  GlicSidePanelCoordinator* coordinator1 =
      GlicSidePanelCoordinator::GetForTab(tab1);

  CreateAndActivateTab(GetSimpleTestUrl());
  GetTabListInterface()->ActivateTab(tab1->GetHandle());

  EXPECT_EQ(coordinator1->state(), GlicSidePanelCoordinator::State::kPeek);
  EXPECT_TRUE(RunUntil(
      [&]() { return instance->GetActiveEmbedderTabForTesting() == nullptr; },
      "Active embedder tab didn't become null after switching back to a tab "
      "with a peek side panel."));
}

// TODO(b/508340459): Fix flakiness on x64.
IN_PROC_BROWSER_TEST_F(GlicAndroidPeekBrowserTest,
                       DISABLED_NotShowingOnTabSwitchIfPreviouslyClosed) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());

  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();

  PreventDeletionOnClose();
  instance->CloseAllEmbedders();
  ASSERT_OK(
      WaitForSidePanelState(tab1, GlicSidePanelCoordinator::State::kClosed));

  CreateAndActivateTab(GetSimpleTestUrl());

  GetTabListInterface()->ActivateTab(tab1->GetHandle());

  ASSERT_OK(
      WaitForSidePanelState(tab1, GlicSidePanelCoordinator::State::kClosed));
  EXPECT_FALSE(instance->GetActiveEmbedderTabForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicAndroidPeekBrowserTest,
                       TransitionToActiveEmbedderOnExpansion) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());

  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  auto* coordinator_android = GetSidePanelCoordinatorAndroid(active_tab);
  coordinator_android->OnOpened(false);  // Set to PEEK
  EXPECT_EQ(coordinator_android->state(),
            GlicSidePanelCoordinator::State::kPeek);

  coordinator_android->OnOpened(true);  // Set to Expanded

  EXPECT_EQ(coordinator_android->state(),
            GlicSidePanelCoordinator::State::kShown);
  EXPECT_TRUE(RunUntil(
      [&]() {
        return instance->GetActiveEmbedderTabForTesting() == active_tab;
      },
      "Active tab didn't become the active embedder after expansion."));
}

IN_PROC_BROWSER_TEST_F(GlicAndroidPeekBrowserTest,
                       OnTabAddedToTaskStartsInPeekState) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());

  tabs::TabInterface* new_tab = CreateAndActivateTab(GetSimpleTestUrl());

  instance->OnTabAddedToTask(actor::TaskId(1), new_tab->GetHandle());

  ASSERT_OK(
      WaitForSidePanelState(new_tab, GlicSidePanelCoordinator::State::kPeek));
}

IN_PROC_BROWSER_TEST_F(GlicAndroidPeekBrowserTest,
                       ShowDoesNotDeactivateActiveEmbedder) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  EXPECT_TRUE(instance->IsShowing());
  EXPECT_EQ(instance->GetActiveEmbedderTabForTesting(), tab);

  auto* embedder_before = instance->GetEmbedderForTab(tab);
  ASSERT_TRUE(embedder_before);

  // Call Show again with prefer_peek = true.
  SidePanelShowOptions side_panel_options{*tab};
  side_panel_options.prefer_peek = true;

  instance->Show(ShowOptions{side_panel_options});

  // It should still be showing and active.
  EXPECT_TRUE(instance->IsShowing());
  EXPECT_EQ(instance->GetActiveEmbedderTabForTesting(), tab);
  EXPECT_EQ(embedder_before, instance->GetEmbedderForTab(tab));
}

// This is a crash regression test, see crbug.com/512567837.
IN_PROC_BROWSER_TEST_F(GlicAndroidPeekBrowserTest,
                       ShowFailsWhenSuppressedCausesCrash) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  tabs::TabInterface* tab = GetTabListInterface()->GetActiveTab();

  auto* coordinator_android = GetSidePanelCoordinatorAndroid(tab);
  ASSERT_TRUE(coordinator_android);

  // Set the side panel to peek state so that state_ is State::kPeek.
  coordinator_android->OnOpened(false);
  EXPECT_EQ(coordinator_android->state(),
            GlicSidePanelCoordinator::State::kPeek);

  // Suppress the bottom sheet so that the next Show() will fail synchronously.
  coordinator_android->SuppressBottomSheetForTesting(true);

  // Call Toggle to attempt to expand the panel with focus_on_show = true.
  // In the buggy implementation, bridge_->Show will return false,
  // SetState(kClosed) will transition from kPeek to kClosed, synchronously
  // destroying the embedder. GlicInstanceImpl::Show will then call Focus() on
  // the destroyed embedder, causing a SIGSEGV crash.
  instance->Toggle(ShowOptions::ForSidePanel(*tab), /*prevent_close=*/false,
                   mojom::InvocationSource::kTopChromeButton);

  coordinator_android->SuppressBottomSheetForTesting(false);
}

IN_PROC_BROWSER_TEST_F(GlicAndroidPeekBrowserTest,
                       TransitionToPeekOnActiveEmbedderSwapAcrossWindows) {
  // Setup the first window and show Glic in expanded state on its active
  // tab.
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  tabs::TabInterface* tab1 = GetTabListInterface()->GetActiveTab();
  ASSERT_OK(
      WaitForSidePanelState(tab1, GlicSidePanelCoordinator::State::kShown));
  EXPECT_EQ(instance->GetActiveEmbedderTabForTesting(), tab1);

  // Create a second window/browser.
  BrowserWindowCreateParams create_params = BrowserWindowCreateParams(
      BrowserWindowInterface::Type::TYPE_NORMAL, *GetProfile(),
      /*from_user_gesture=*/false);
  base::test::TestFuture<BrowserWindowInterface*> future;
  CreateBrowserWindow(std::move(create_params), future.GetCallback());
  BrowserWindowInterface* browser2 = future.Get();
  ASSERT_TRUE(browser2);
  ASSERT_NE(GetBrowser(), browser2);

  // Get the active tab in the second window.
  tabs::TabInterface* tab2 = TabListInterface::From(browser2)->GetActiveTab();

  // Swap active embedders by showing Glic on the second window's active
  // tab. This will trigger the deactivation of tab1's active embedder,
  // replacing it with GlicInactiveSidePanelUi, which will then call
  // InitializeAfterRegistration(). Since tab1 is still the active/activated tab
  // in the first window, its bottom sheet transitions to the peek state.
  SidePanelShowOptions show_options{*tab2};
  show_options.prefer_peek = false;
  instance->Show(ShowOptions{show_options});

  ASSERT_OK(
      WaitForSidePanelState(tab1, GlicSidePanelCoordinator::State::kPeek));
  ASSERT_OK(
      WaitForSidePanelState(tab2, GlicSidePanelCoordinator::State::kShown));
  EXPECT_EQ(instance->GetActiveEmbedderTabForTesting(), tab2);

  // Cleanup created window
  browser2->GetWindow()->Close();
}

}  // namespace glic
