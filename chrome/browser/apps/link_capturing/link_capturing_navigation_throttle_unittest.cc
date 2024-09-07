// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/link_capturing_navigation_throttle.h"

#include <map>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace apps {
namespace {

TEST(LinkCapturingNavigationThrottleTest, TestIsGoogleRedirectorUrl) {
  // Test that redirect urls with different TLDs are still recognized.
  EXPECT_TRUE(LinkCapturingNavigationThrottle::IsGoogleRedirectorUrl(
      GURL("https://www.google.com.au/url?q=whatever")));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::IsGoogleRedirectorUrl(
      GURL("https://www.google.com.mx/url?q=hotpot")));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::IsGoogleRedirectorUrl(
      GURL("https://www.google.co/url?q=query")));

  // Non-google domains shouldn't be used as valid redirect links.
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsGoogleRedirectorUrl(
      GURL("https://www.not-google.com/url?q=query")));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsGoogleRedirectorUrl(
      GURL("https://www.gooogle.com/url?q=legit_query")));

  // This method only takes "/url" as a valid path, it needs to contain a query,
  // we don't analyze that query as it will expand later on in the same
  // throttle.
  EXPECT_TRUE(LinkCapturingNavigationThrottle::IsGoogleRedirectorUrl(
      GURL("https://www.google.com/url?q=who_dis")));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::IsGoogleRedirectorUrl(
      GURL("http://www.google.com/url?q=who_dis")));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsGoogleRedirectorUrl(
      GURL("https://www.google.com/url")));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsGoogleRedirectorUrl(
      GURL("https://www.google.com/link?q=query")));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsGoogleRedirectorUrl(
      GURL("https://www.google.com/link")));
}

TEST(LinkCapturingNavigationThrottleTest, TestShouldOverrideUrlLoading) {
  // A navigation from chrome-extension scheme cannot be overridden.
  EXPECT_FALSE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("chrome-extension://fake_document"), GURL("http://www.a.com")));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("chrome-extension://fake_document"), GURL("https://www.a.com")));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("chrome-extension://fake_a"), GURL("chrome-extension://fake_b")));

  // Other navigations can be overridden.
  EXPECT_TRUE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("http://www.google.com"), GURL("http://www.not-google.com/")));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("http://www.not-google.com"), GURL("http://www.google.com/")));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("http://www.google.com"), GURL("http://www.google.com/")));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("http://a.google.com"), GURL("http://b.google.com/")));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("http://a.not-google.com"), GURL("http://b.not-google.com")));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("chrome://fake_document"), GURL("http://www.a.com")));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("file://fake_document"), GURL("http://www.a.com")));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("chrome://fake_document"), GURL("https://www.a.com")));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("file://fake_document"), GURL("https://www.a.com")));

  // A navigation going to a redirect url cannot be overridden, unless there's
  // no query or the path is not valid.
  EXPECT_FALSE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("http://www.google.com"), GURL("https://www.google.com/url?q=b")));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("https://www.a.com"), GURL("https://www.google.com/url?q=a")));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("https://www.a.com"), GURL("https://www.google.com/url")));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::ShouldOverrideUrlIfRedirected(
      GURL("https://www.a.com"), GURL("https://www.google.com/link?q=a")));
}

// Tests that LinkCapturingNavigationThrottle::ShouldIgnoreNavigation returns
// false only for PAGE_TRANSITION_LINK when |allow_form_submit| is false and
// |is_in_fenced_frame_tree| is false.
TEST(LinkCapturingNavigationThrottleTest,
     TestShouldIgnoreNavigationWithCoreTypes) {
  EXPECT_TRUE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PAGE_TRANSITION_LINK, false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PAGE_TRANSITION_TYPED, false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PAGE_TRANSITION_AUTO_BOOKMARK, false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PAGE_TRANSITION_AUTO_SUBFRAME, false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PAGE_TRANSITION_MANUAL_SUBFRAME, false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PAGE_TRANSITION_GENERATED, false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PAGE_TRANSITION_FORM_SUBMIT, false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PAGE_TRANSITION_RELOAD, false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PAGE_TRANSITION_KEYWORD, false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PAGE_TRANSITION_KEYWORD_GENERATED, false, false, false));

  static_assert(static_cast<int32_t>(ui::PAGE_TRANSITION_KEYWORD_GENERATED) ==
                    static_cast<int32_t>(ui::PAGE_TRANSITION_LAST_CORE),
                "Not all core transition types are covered here");
}

// Test that LinkCapturingNavigationThrottle::ShouldIgnoreNavigation accepts
// FORM_SUBMIT when |allow_form_submit| is true.
TEST(LinkCapturingNavigationThrottleTest, TestFormSubmit) {
  EXPECT_TRUE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PAGE_TRANSITION_FORM_SUBMIT, true, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PAGE_TRANSITION_FORM_SUBMIT, false, false, false));
}

// Tests that LinkCapturingNavigationThrottle::ShouldIgnoreNavigation returns
// true when no qualifiers except client redirect and server redirect are
// provided when |is_in_fenced_frame_tree| is false.
TEST(LinkCapturingNavigationThrottleTest,
     TestShouldIgnoreNavigationWithLinkWithQualifiers) {
  // The navigation is triggered by Forward or Back button.
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FORM_SUBMIT |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      false, false, false));
  // The user used the address bar to trigger the navigation.
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false, false, false));
  // The user pressed the Home button.
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      false, false, false));
  // ARC (for example) opened the link in Chrome.
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_API),
      false, false, false));
  // The navigation is triggered by a client side redirect.
  EXPECT_TRUE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false, false, false));
  // Also tests the case with 2+ qualifiers.
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false, false, false));
  // The navigation is triggered by a server side redirect.
  EXPECT_TRUE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT),
      false, false, false));
  // Also tests the case with 2+ qualifiers.
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT),
      false, false, false));
}

// Just in case, does the same with ui::PAGE_TRANSITION_TYPED.
TEST(LinkCapturingNavigationThrottleTest,
     TestShouldIgnoreNavigationWithTypedWithQualifiers) {
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FORWARD_BACK),
      false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_API),
      false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false, false, false));
}

// Test that LinkCapturingNavigationThrottle::ShouldIgnoreNavigation accepts
// SERVER_REDIRECT and CLIENT_REDIRECT when |is_in_fenced_frame_tree| is false.
TEST(LinkCapturingNavigationThrottleTest,
     TestShouldIgnoreNavigationWithClientRedirect) {
  EXPECT_TRUE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK), false, false,
      false));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      false, false, false));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT),
      false, false, false));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT),
      false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      false, false, false));
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_HOME_PAGE),
      false, false, false));
}

// Test that MaskOutPageTransition correctly remove a qualifier from a given
// |page_transition|.
TEST(LinkCapturingNavigationThrottleTest, TestMaskOutPageTransition) {
  ui::PageTransition page_transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  EXPECT_EQ(
      static_cast<int>(ui::PAGE_TRANSITION_LINK),
      static_cast<int>(LinkCapturingNavigationThrottle::MaskOutPageTransition(
          page_transition, ui::PAGE_TRANSITION_CLIENT_REDIRECT)));

  page_transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_SERVER_REDIRECT);
  EXPECT_EQ(
      static_cast<int>(ui::PAGE_TRANSITION_LINK),
      static_cast<int>(LinkCapturingNavigationThrottle::MaskOutPageTransition(
          page_transition, ui::PAGE_TRANSITION_SERVER_REDIRECT)));
}

// Test that LinkCapturingNavigationThrottle::ShouldIgnoreNavigation accepts iff
// |has_user_gesture| is true when |is_in_fenced_frame_tree| is true.
TEST(LinkCapturingNavigationThrottleTest, TestInFencedFrameTree) {
  EXPECT_FALSE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_SUBFRAME), false, true,
      false));
  EXPECT_TRUE(LinkCapturingNavigationThrottle::IsCapturableLinkNavigation(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_SUBFRAME), false, true,
      true));
}

class LinkCapturingNavThrottleReimplTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  LinkCapturingNavThrottleReimplTest() {
    std::map<std::string, std::string> parameters;
    parameters["link_capturing_state"] = FlagBoolToReimpl();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPwaNavigationCapturing, parameters);
  }

  std::string FlagBoolToReimpl() {
    if (GetParam()) {
      return "reimpl_default_on";
    }
    return "reimpl_default_off";
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(LinkCapturingNavThrottleReimplTest, NotCreated) {
  EXPECT_EQ(nullptr, LinkCapturingNavigationThrottle::MaybeCreate(
                         /*handle=*/nullptr, /*delegate=*/nullptr));
}

INSTANTIATE_TEST_SUITE_P(All,
                         LinkCapturingNavThrottleReimplTest,
                         testing::Bool(),
                         [](const ::testing::TestParamInfo<bool> info) {
                           if (info.param) {
                             return "reimpl_default_on";
                           }
                           return "reimpl_default_off";
                         });

}  // namespace
}  // namespace apps
