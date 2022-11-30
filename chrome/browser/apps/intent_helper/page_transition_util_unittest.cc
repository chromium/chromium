// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/page_transition_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace apps {

// Tests that ShouldIgnoreNavigation returns false only for
// PAGE_TRANSITION_LINK when |allow_form_submit| is false and
// |is_in_fenced_frame_tree| is false.
TEST(PageTransitionUtilTest, TestShouldIgnoreNavigationWithCoreTypes) {
  EXPECT_FALSE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_LINK, false, false, false));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_TYPED, false, false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_BOOKMARK, false,
                                     false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_SUBFRAME, false,
                                     false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_MANUAL_SUBFRAME, false,
                                     false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_GENERATED, false,
                                     false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false,
                                     false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_FORM_SUBMIT, false,
                                     false, false));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_RELOAD, false, false, false));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_KEYWORD, false, false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_KEYWORD_GENERATED,
                                     false, false, false));

  static_assert(static_cast<int32_t>(ui::PAGE_TRANSITION_KEYWORD_GENERATED) ==
                    static_cast<int32_t>(ui::PAGE_TRANSITION_LAST_CORE),
                "Not all core transition types are covered here");
}

// Test that ShouldIgnoreNavigation accepts FORM_SUBMIT when |allow_form_submit|
// is true.
TEST(PageTransitionUtilTest, TestFormSubmit) {
  EXPECT_FALSE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_FORM_SUBMIT, true,
                                      false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_FORM_SUBMIT, false,
                                     false, false));
}

// Tests that ShouldIgnoreNavigation returns true when no qualifiers except
// client redirect and server redirect are provided when
// |is_in_fenced_frame_tree| is false.
TEST(PageTransitionUtilTest, TestShouldIgnoreNavigationWithLinkWithQualifiers) {
  // The navigation is triggered by Forward or Back button.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      false, false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORM_SUBMIT |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      false, false, false));
  // The user used the address bar to trigger the navigation.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false, false, false));
  // The user pressed the Home button.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      false, false, false));
  // ARC (for example) opened the link in Chrome.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_API),
      false, false, false));
  // The navigation is triggered by a client side redirect.
  EXPECT_FALSE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false, false, false));
  // Also tests the case with 2+ qualifiers.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false, false, false));
  // The navigation is triggered by a server side redirect.
  EXPECT_FALSE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT),
      false, false, false));
  // Also tests the case with 2+ qualifiers.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT),
      false, false, false));
}

// Just in case, does the same with ui::PAGE_TRANSITION_TYPED.
TEST(PageTransitionUtilTest,
     TestShouldIgnoreNavigationWithTypedWithQualifiers) {
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      false, false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      false, false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false, false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      false, false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_API),
      false, false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false, false, false));
}

// Test that ShouldIgnoreNavigation accepts SERVER_REDIRECT and CLIENT_REDIRECT
// when |is_in_fenced_frame_tree| is false.
TEST(PageTransitionUtilTest, TestShouldIgnoreNavigationWithClientRedirect) {
  EXPECT_FALSE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK), false, false,
      false));
  EXPECT_FALSE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false, false, false));
  EXPECT_FALSE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT),
      false, false, false));
  EXPECT_FALSE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT),
      false, false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      false, false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      false, false, false));
}

// Test that MaskOutPageTransition correctly remove a qualifier from a given
// |page_transition|.
TEST(PageTransitionUtilTest, TestMaskOutPageTransition) {
  ui::PageTransition page_transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  EXPECT_EQ(static_cast<int>(ui::PAGE_TRANSITION_LINK),
            static_cast<int>(MaskOutPageTransition(
                page_transition, ui::PAGE_TRANSITION_CLIENT_REDIRECT)));

  page_transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_SERVER_REDIRECT);
  EXPECT_EQ(static_cast<int>(ui::PAGE_TRANSITION_LINK),
            static_cast<int>(MaskOutPageTransition(
                page_transition, ui::PAGE_TRANSITION_SERVER_REDIRECT)));
}

// Test that ShouldIgnoreNavigation accepts iff |has_user_gesture| is true
// when |is_in_fenced_frame_tree| is true.
TEST(PageTransitionUtilTest, TestInFencedFrameTree) {
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_SUBFRAME), false, true,
      false));
  EXPECT_FALSE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_SUBFRAME), false, true,
      true));
}

}  // namespace apps
