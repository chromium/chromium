// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/lookalikes/core/safety_tip_test_utils.h"
#include "components/url_formatter/spoof_checks/idn_spoof_checker.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/test/mock_navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_features.h"

namespace lookalikes {

// IDNA mode to use in tests.
enum class IDNAMode { kTransitional, kNonTransitional };

class LookalikeThrottleTest : public testing::WithParamInterface<IDNAMode>,
                              public ChromeRenderViewHostTestHarness {
 public:
  LookalikeThrottleTest() {
    if (GetParam() == IDNAMode::kNonTransitional) {
      scoped_feature_list_.InitAndEnableFeature(
          url::kUseIDNA2008NonTransitional);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          url::kUseIDNA2008NonTransitional);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LookalikeThrottleTest,
                         ::testing::Values(IDNAMode::kTransitional,
                                           IDNAMode::kNonTransitional));

// Tests that spoofy hostnames are properly handled in the throttle.
TEST_P(LookalikeThrottleTest, SpoofsBlocked) {
  lookalikes::InitializeSafetyTipConfig();

  const struct TestCase {
    const char* hostname;
    bool expected_blocked;
    url_formatter::IDNSpoofChecker::Result expected_spoof_check_result;
  } kTestCases[] = {
      // ASCII private domain.
      {"private.hostname", false,
       url_formatter::IDNSpoofChecker::Result::kNone},

      // lɔlocked.com, fails ICU spoof checks.
      {"xn--llocked-9bd.com", true,
       url_formatter::IDNSpoofChecker::Result::kICUSpoofChecks},
      // þook.com, contains a TLD specific character (þ).
      {"xn--ook-ooa.com", true,
       url_formatter::IDNSpoofChecker::Result::kTLDSpecificCharacters},
      // example·com.com, unsafe middle dot.
      {"xn--examplecom-rra.com", true,
       url_formatter::IDNSpoofChecker::Result::kUnsafeMiddleDot},
      // scope.com, with scope in Cyrillic. Whole script confusable.
      {"xn--e1argc3h.com", true,
       url_formatter::IDNSpoofChecker::Result::kWholeScriptConfusable},
      //  Non-ASCII Latin with Non-Latin character
      {"xn--caf-dma9024xvpg.kr", true,
       url_formatter::IDNSpoofChecker::Result::
           kNonAsciiLatinCharMixedWithNonLatin},
      // testーsite.com, has dangerous pattern (ー is CJK character).
      {"xn--testsite-1g5g.com", true,
       url_formatter::IDNSpoofChecker::Result::kDangerousPattern},

      // TODO(crbug.com/40052713): Add an example for digit lookalikes.

      // 🍕.com, fails ICU spoof checks, but is allowed because consists of only
      // emoji and ASCII.
      {"xn--vi8h.com", false,
       url_formatter::IDNSpoofChecker::Result::kICUSpoofChecks},
      // sparkasse-gießen.de, has a deviation character (ß). This is in punycode
      // because GURL canonicalizes ß to ss. Safe in IDNA Non-Transitional mode,
      // unsafe otherwise.
      {"xn--sparkasse-gieen-2ib.de", false,
       GetParam() == IDNAMode::kNonTransitional
           ? url_formatter::IDNSpoofChecker::Result::kSafe
           : url_formatter::IDNSpoofChecker::Result::kDeviationCharacters},
  };

  for (const TestCase& test_case : kTestCases) {
    url_formatter::IDNConversionResult idn_result =
        url_formatter::UnsafeIDNToUnicodeWithDetails(test_case.hostname);
    ASSERT_EQ(test_case.expected_spoof_check_result,
              idn_result.spoof_check_result)
        << test_case.hostname;

    GURL url(std::string("http://") + test_case.hostname);
    ::testing::NiceMock<content::MockNavigationHandle> handle(url, main_rfh());
    handle.set_redirect_chain({url});
    handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);

    auto throttle =
        LookalikeUrlNavigationThrottle::MaybeCreateNavigationThrottle(&handle);
    ASSERT_TRUE(throttle);
    throttle->SetUseTestProfileForTesting();

    EXPECT_EQ(content::NavigationThrottle::PROCEED,
              throttle->WillStartRequest().action());

    if (test_case.expected_blocked) {
      EXPECT_EQ(content::NavigationThrottle::CANCEL,
                throttle->WillProcessResponse().action())
          << "Failed: " << test_case.hostname;
    } else {
      EXPECT_EQ(content::NavigationThrottle::PROCEED,
                throttle->WillProcessResponse().action())
          << "Failed: " << test_case.hostname;
    }
  }
}

}  // namespace lookalikes
