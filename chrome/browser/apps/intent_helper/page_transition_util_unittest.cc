// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/page_transition_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace apps {

// Tests that ShouldIgnoreNavigation returns false only for
// PAGE_TRANSITION_LINK.
TEST(PageTransitionUtilTest, TestShouldIgnoreNavigationWithCoreTypes) {
  EXPECT_FALSE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_LINK, false, false));
  EXPECT_FALSE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_LINK, true, true));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_TYPED, false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_TYPED, true, true));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_BOOKMARK, false, false));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_BOOKMARK, true, true));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_SUBFRAME, false, false));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_SUBFRAME, true, true));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_MANUAL_SUBFRAME, false,
                                     false));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_MANUAL_SUBFRAME, true, true));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_GENERATED, false, false));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_GENERATED, true, true));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false, false));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_AUTO_TOPLEVEL, true, true));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_FORM_SUBMIT, false, false));
  EXPECT_FALSE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_FORM_SUBMIT, true, true));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_RELOAD, false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_RELOAD, true, true));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_KEYWORD, false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_KEYWORD, true, true));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_KEYWORD_GENERATED,
                                     false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_KEYWORD_GENERATED,
                                     true, true));

  static_assert(static_cast<int32_t>(ui::PAGE_TRANSITION_KEYWORD_GENERATED) ==
                    static_cast<int32_t>(ui::PAGE_TRANSITION_LAST_CORE),
                "Not all core transition types are covered here");
}

// Tests that ShouldIgnoreNavigation returns true when no qualifiers except
// server redirect are provided.
TEST(PageTransitionUtilTest, TestShouldIgnoreNavigationWithLinkWithQualifiers) {
  // The navigation is triggered by Forward or Back button.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORM_SUBMIT |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      true, true));
  // The user used the address bar to triger the navigation.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false, false));
  // The user pressed the Home button.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      false, false));
  // ARC (for example) opened the link in Chrome.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_API),
      false, false));
  // The navigation is triggered by a client side redirect.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false, false));
  // Also tests the case with 2+ qualifiers.
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false, false));
}

// Just in case, does the same with ui::PAGE_TRANSITION_TYPED.
TEST(PageTransitionUtilTest,
     TestShouldIgnoreNavigationWithTypedWithQualifiers) {
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      true, true));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_API),
      false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false, false));
}

// Tests that ShouldIgnoreNavigation returns false if SERVER_REDIRECT is the
// only qualifier given.
TEST(PageTransitionUtilTest, TestShouldIgnoreNavigationWithServerRedirect) {
  EXPECT_FALSE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT),
      false, false));
  EXPECT_FALSE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORM_SUBMIT |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT),
      true, true));

  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                ui::PAGE_TRANSITION_FROM_API),
      false, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORM_SUBMIT |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                ui::PAGE_TRANSITION_FROM_API),
      true, true));
}

// Test that ShouldIgnoreNavigation accepts CLIENT_REDIRECT qualifier when
// |allow_form_submit| equals true.
TEST(PageTransitionUtilTest, TestShouldIgnoreNavigationWithClientRedirect) {
  EXPECT_FALSE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK), true, true));
  EXPECT_FALSE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      true, true));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      true, true));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      true, true));
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

// Test mixed variants between |allow_form_submit| and |allow_client_redirect|.
TEST(PageTransitionUtilTest, TestShouldIgnoreNavigationWithNonProdScenarios) {
  EXPECT_FALSE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_LINK, true, false));
  EXPECT_FALSE(ShouldIgnoreNavigation(ui::PAGE_TRANSITION_LINK, false, true));

  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      true, false));
  EXPECT_FALSE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false, true));

  EXPECT_FALSE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_FORM_SUBMIT, true, false));
  EXPECT_TRUE(
      ShouldIgnoreNavigation(ui::PAGE_TRANSITION_FORM_SUBMIT, false, true));

  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORM_SUBMIT |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      true, false));
  EXPECT_TRUE(ShouldIgnoreNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORM_SUBMIT |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false, true));
}

}  // namespace apps
