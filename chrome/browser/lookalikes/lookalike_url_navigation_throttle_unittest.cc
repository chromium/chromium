// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_navigation_throttle.h"

#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/lookalikes/lookalike_url_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/lookalikes/core/safety_tip_test_utils.h"
#include "components/lookalikes/core/safety_tips_config.h"
#include "components/url_formatter/spoof_checks/idn_spoof_checker.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_navigation_throttle_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

namespace lookalikes {

class LookalikeThrottleTest : public ChromeRenderViewHostTestHarness {};

// Tests that spoofy hostnames are properly handled in the throttle.
TEST_F(LookalikeThrottleTest, SpoofsBlocked) {
  lookalikes::InitializeSafetyTipConfig();

  const struct TestCase {
    const char* hostname;
    bool expected_blocked;
    url_formatter::IDNSpoofCheckerResult expected_spoof_check_result;
  } kTestCases[] = {
      // ASCII private domain.
      {"private.hostname", false, url_formatter::IDNSpoofCheckerResult::kNone},

      // lɔlocked.com,
      // Update for ICU 78.2, and IDNA updates in Unicode 17.
      // this label no longer fails, compare: https://crbug.com/492063443
      {"xn--llocked-9bd.com", false,
       url_formatter::IDNSpoofCheckerResult::kSafe /*ICUSpoofChecks*/},
      // þook.com, contains a TLD specific character (þ).
      {"xn--ook-ooa.com", true,
       url_formatter::IDNSpoofCheckerResult::kTLDSpecificCharacters},
      // example·com.com, unsafe middle dot.
      {"xn--examplecom-rra.com", true,
       url_formatter::IDNSpoofCheckerResult::kUnsafeMiddleDot},
      // scope.com, with scope in Cyrillic. Whole script confusable.
      {"xn--e1argc3h.com", true,
       url_formatter::IDNSpoofCheckerResult::kWholeScriptConfusable},
      //  Non-ASCII Latin with Non-Latin character
      {"xn--caf-dma9024xvpg.kr", true,
       url_formatter::IDNSpoofCheckerResult::
           kNonAsciiLatinCharMixedWithNonLatin},
      // testーsite.com, has dangerous pattern (ー is CJK character).
      {"xn--testsite-1g5g.com", true,
       url_formatter::IDNSpoofCheckerResult::kDangerousPattern},

      // TODO(crbug.com/40052713): Add an example for digit lookalikes.

      // 🍕.com, fails ICU spoof checks, but is allowed because consists of only
      // emoji and ASCII.
      {"xn--vi8h.com", false,
       url_formatter::IDNSpoofCheckerResult::kICUSpoofChecks},
      // sparkasse-gießen.de, has a deviation character (ß). This is in punycode
      // because GURL canonicalizes ß to ss.
      {"xn--sparkasse-gieen-2ib.de", false,
       url_formatter::IDNSpoofCheckerResult::kSafe},
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
    ::testing::NiceMock<content::MockNavigationThrottleRegistry> registry(
        &handle,
        content::MockNavigationThrottleRegistry::RegistrationMode::kHold);

    LookalikeUrlNavigationThrottle::MaybeCreateAndAdd(registry);
    CHECK_EQ(1u, registry.throttles().size());
    raw_ptr<LookalikeUrlNavigationThrottle> throttle =
        static_cast<LookalikeUrlNavigationThrottle*>(
            registry.throttles().back().get());
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

// Verifies that `LookalikeUrlService::NeedsEngagedSitesForCheck()` returns
// false for URLs that early-return in `CheckUrlForLookalikes()` before
// consulting `engaged_sites`, and true for URLs that require `engaged_sites`.
TEST_F(LookalikeThrottleTest, NeedsEngagedSitesForCheck) {
  LookalikeUrlService* service =
      LookalikeUrlServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  const GURL kTopDomainUrl("https://www.google.com/search?q=test");
  const GURL kNonTopUrl("https://non-top-example-domain.com/path");

  // 1. Without a Safety Tips remote config proto, `CheckUrlForLookalikes()`
  // fails safe and returns immediately without consulting `engaged_sites`.
  lookalikes::SetSafetyTipsRemoteConfigProto(nullptr);
  EXPECT_FALSE(service->NeedsEngagedSitesForCheck(kNonTopUrl));

  // 2. Once the Safety Tips remote config proto is populated, top 500 domains,
  // safe TLDs (.gov), localhost, and non-HTTP(S) URLs early-return without
  // `engaged_sites`, whereas non-top domains require `engaged_sites`.
  lookalikes::InitializeSafetyTipConfig();
  EXPECT_FALSE(service->NeedsEngagedSitesForCheck(kTopDomainUrl));
  EXPECT_FALSE(service->NeedsEngagedSitesForCheck(GURL("https://agency.gov/")));
  EXPECT_FALSE(
      service->NeedsEngagedSitesForCheck(GURL("http://localhost:8080/")));
  EXPECT_FALSE(service->NeedsEngagedSitesForCheck(GURL("chrome://settings")));
  EXPECT_TRUE(service->NeedsEngagedSitesForCheck(kNonTopUrl));

  // 3. Once the user has dismissed a warning for `kNonTopUrl`, navigation
  // throttle checks early-return without consulting `engaged_sites`.
  service->SetUserIgnore(kNonTopUrl);
  EXPECT_FALSE(service->NeedsEngagedSitesForCheck(kNonTopUrl));
  service->ResetWarningDismissedETLDPlusOnesForTesting();
  EXPECT_TRUE(service->NeedsEngagedSitesForCheck(kNonTopUrl));
}

}  // namespace lookalikes
