// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/reputation/local_heuristics.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

struct KeywordHeuristicTestCase {
  const GURL url;
  const bool should_trigger;
};

TEST(SafetyTipHeuristicsTest, SensitiveKeywordsTest) {
  // These keywords must always be in sorted order.
  const std::vector<const char*> keywords = {"bad", "evil", "keyword"};

  const std::vector<KeywordHeuristicTestCase> test_cases = {
      // Verify scheme doesn't affect results.
      {GURL("http://www.bad.com"), false},
      {GURL("https://www.bad.com"), false},

      {GURL("http://bad-domain.com"), true},
      {GURL("https://bad-domain.com"), true},

      // We don't really care about sub-domains for this heuristic, verify this
      // works as expected.
      {GURL("http://www.evil-domain.safe-domain.com"), false},
      {GURL("http://www.safe-domain.evil-domain.com"), true},

      {GURL("http://www.bad-other.edu"), true},
      {GURL("http://bad-keyword.com"), true},
      {GURL("http://www.evil-and-bad.com"), true},

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
        << "Expected that \"" << test_case.url << "\" should"
        << (test_case.should_trigger ? "" : "n't") << " trigger but it did"
        << (test_case.should_trigger ? "n't" : "");
  }
}
