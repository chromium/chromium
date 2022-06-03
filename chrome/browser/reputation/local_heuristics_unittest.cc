// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/reputation/local_heuristics.h"

#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "components/security_state/core/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

struct KeywordHeuristicTestCase {
  const GURL url;
  const bool should_trigger;
};

// Verify behavior of the "Sensitive Keywords" heuristic, which, triggers when
// it finds a keyword (like a popular brand name) in a hostname unexpectedly.
TEST(SafetyTipHeuristicsTest, SensitiveKeywordsTest) {
  // These keywords must always be in sorted order.
  const std::vector<const char*> keywords = {"bad", "evil", "keyword"};

  const std::vector<KeywordHeuristicTestCase> test_cases = {
      // Verify scheme doesn't affect results.
      {GURL("http://good-domain.com"), false},
      {GURL("https://good-domain.com"), false},
      {GURL("http://bad-domain.com"), true},
      {GURL("https://bad-domain.com"), true},

      // Verify detection works in subdomains.
      {GURL("http://www.domain.evil.safe-domain.com"), true},
      {GURL("http://www.evil-domain.safe-domain.com"), true},
      {GURL("http://evil.domain.safe-domain.com"), true},

      // e2LDs that are a sensitive keyword themselves should *not* trigger the
      // heuristic, but they shouldn't prevent the heuristic from triggering for
      // other reasons.
      {GURL("http://www.bad.com"), false},
      {GURL("http://bad.com"), false},
      {GURL("http://evil.bad.com"), true},

      // Verify keywords still in the e2LD no matter where they fall.
      {GURL("http://www.good-and-bad.com"), true},
      {GURL("http://www.bad-other.edu"), true},
      {GURL("http://bad-keyword.com"), true},
      {GURL("http://www.evil-and-bad.com"), true},
      {GURL("http://www.good-evil-neutral.com"), true},

      // Make sure heuristic still works, even for really long domains.
      {GURL("http://"
            "www.super-duper-uber-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-domain-with-a-lot-of-parts-to-it.org"),
       false},
      {GURL("http://"
            "www.super-duper-uber-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-THISISEVIL-evil-THISISEVIL-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-long-long-long-long-long-"
            "long-long-long-long-long-long-long-long-domain-with-a-lot-of-"
            "parts-to-it.org"),
       true},

      // Ensure heuristic doesn't trigger on misspelled keywords.
      {GURL("http://www.misspelled-example-keywrd.edu"), false},
      {GURL("http://www.spelled-right-example-keyword.edu"), true},

      // Make sure passing a lot of keywords doesn't result in a false negative.
      {GURL("http://evil-bad-keyword-example.com"), true},

      // Test a few cases with edge-case URLs. URLs with unknown registries
      // shouldn't trigger.
      {GURL("http://foo"), false},
      {GURL("http://foo.invalidregistry"), false},
      {GURL("http://evil-site"), false},
      {GURL("http://evil-site.invalidregistry"), false},
      {GURL("http://this.is.an.evil-site.invalidregistry"), false},

      // Test some edge cases which the heuristic should gracefully handle.
      {GURL("http://localhost"), false},
      {GURL("http://1.2.3.4"), false},
      {GURL("http://127.0.0.1"), false},

      // Make sure the heuristic never triggers on non-http / https URLs.
      {GURL("ftp://www.safe-website.com"), false},
      {GURL("ftp://www.evil-website.com"), false},
      {GURL("garbage://www.evil-website.com"), false},

      // Ensure that the URL path doesn't affect the heuristic.
      {GURL("http://www.evil-site.com/some/random/path"), true},
      {GURL("http://www.safe-site.com/evil-path/even-more-evil-path"), false},
      {GURL("http://www.evil.com/safe-path/"), false},
      {GURL("http://www.evil.com/safe-path/evil"), false},
      {GURL("http://www.evil.com/evil-path/"), false},

      // Ensure a really long path doesn't affect the heuristic.
      {GURL("http://www.safe.com/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/safe-path/evil/evil/evil/evil/evil/"
            "evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/evil/"),
       false},
  };

  for (const auto& test_case : test_cases) {
    const DomainInfo test_case_navigated_domain = GetDomainInfo(test_case.url);
    ASSERT_EQ(test_case.should_trigger,
              ShouldTriggerSafetyTipFromKeywordInURL(
                  test_case.url, test_case_navigated_domain, keywords.data(),
                  keywords.size()))
        << "Expected that \"" << test_case.url << "\" would"
        << (test_case.should_trigger ? "" : "n't") << " trigger but it did"
        << (test_case.should_trigger ? "n't" : "");
  }
}

TEST(SafetyTipHeuristicsTest, ShouldTriggerSafetyTipFromLookalike) {
  base::FieldTrialParams params;
  params["editdistance"] = "true";
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      security_state::features::kSafetyTipUI, params);

  struct TestCase {
    GURL navigated_url;
    GURL engaged_url;
    GURL expected_safe_url;
  } kTestCases[] = {
      // Top domain matches should have https:// scheme for safe URLs.
      {GURL("https://gooogle.com"), GURL(), GURL("https://google.com")},
      {GURL("http://gooogle.com"), GURL(), GURL("https://google.com")},

      // Engaged site matches should use the scheme of the lookalike URL for
      // safe URLs.
      {GURL("http://teestsite.com"), GURL("https://testsite.com"),
       GURL("http://testsite.com")},
      {GURL("http://teestsite.com"), GURL("http://testsite.com"),
       GURL("http://testsite.com")},
      {GURL("https://teestsite.com"), GURL("https://testsite.com"),
       GURL("https://testsite.com")},
      {GURL("https://teestsite.com"), GURL("http://testsite.com"),
       GURL("https://testsite.com")},
  };

  for (const TestCase& test_case : kTestCases) {
    std::vector<DomainInfo> engaged_sites;
    if (test_case.engaged_url.is_valid()) {
      engaged_sites.push_back(GetDomainInfo(test_case.engaged_url));
    }
    GURL safe_url;
    EXPECT_TRUE(ShouldTriggerSafetyTipFromLookalike(
        test_case.navigated_url, GetDomainInfo(test_case.navigated_url),
        engaged_sites, &safe_url));
    EXPECT_EQ(test_case.expected_safe_url, safe_url);
  }
}
