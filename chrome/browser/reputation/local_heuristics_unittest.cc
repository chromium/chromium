// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/lookalikes/lookalike_url_service.h"
#include "chrome/browser/reputation/local_heuristics.h"

#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(SafetyTipHeuristicsTest, ShouldTriggerSafetyTipFromLookalike) {
  struct TestCase {
    GURL navigated_url;
    GURL engaged_url;
    GURL expected_safe_url;
  } kTestCases[] = {
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
