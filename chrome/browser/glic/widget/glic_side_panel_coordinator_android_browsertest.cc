// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_android.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicSidePanelCoordinatorAndroidBrowserTest : public GlicBrowserTest {
 public:
  GlicSidePanelCoordinatorAndroidBrowserTest() = default;
  ~GlicSidePanelCoordinatorAndroidBrowserTest() override = default;

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
  EXPECT_EQ(first_coordinator->GetInitialStateOverrideForTesting(),
            GlicSidePanelCoordinator::ShowOptions::InitialState::kExpanded);

  // Switch back to the first tab.
  GetTabListInterface()->ActivateTab(first_tab->GetHandle());
  EXPECT_EQ(first_state_future.Take(), GlicSidePanelCoordinator::State::kPeek);
  EXPECT_EQ(first_coordinator->state(), GlicSidePanelCoordinator::State::kPeek);
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

  // Verify that the first tab transitions to backgrounded and saves the peeked
  // state override.
  EXPECT_EQ(first_state_future.Take(),
            GlicSidePanelCoordinator::State::kBackgrounded);
  EXPECT_EQ(first_coordinator->state(),
            GlicSidePanelCoordinator::State::kBackgrounded);
  EXPECT_EQ(first_coordinator->GetInitialStateOverrideForTesting(),
            GlicSidePanelCoordinator::ShowOptions::InitialState::kPeeked);

  // Switch back to the first tab.
  GetTabListInterface()->ActivateTab(first_tab->GetHandle());
  EXPECT_EQ(first_state_future.Take(), GlicSidePanelCoordinator::State::kPeek);
  EXPECT_EQ(first_coordinator->state(), GlicSidePanelCoordinator::State::kPeek);
}

}  // namespace glic
