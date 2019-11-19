// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/reputation_service.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/reputation/safety_tip_test_utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/security_state/core/security_state.h"
#include "testing/gtest/include/gtest/gtest.h"

class ReputationServiceTest : public ChromeRenderViewHostTestHarness {
 protected:
  ReputationServiceTest() {}
  ~ReputationServiceTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ReputationServiceTest);
};

// Test that the blocklist blocks patterns as expected.
TEST_F(ReputationServiceTest, BlocklistTest) {
  SetSafetyTipBadRepPatterns(
      {"domain.test/", "directory.test/foo/", "path.test/foo/bar.html",
       "query.test/foo/bar.html?baz=test", "sub.subdomain.test/"});

  const std::vector<std::pair<std::string, security_state::SafetyTipStatus>>
      kTests = {
          {"http://unblocked.test", security_state::SafetyTipStatus::kNone},
          {"http://unblocked.test/foo", security_state::SafetyTipStatus::kNone},
          {"http://unblocked.test/foo.html?bar=baz",
           security_state::SafetyTipStatus::kNone},

          {"http://sub.domain.test",
           security_state::SafetyTipStatus::kBadReputation},
          {"http://domain.test",
           security_state::SafetyTipStatus::kBadReputation},
          {"http://domain.test/foo",
           security_state::SafetyTipStatus::kBadReputation},
          {"http://domain.test/foo/bar",
           security_state::SafetyTipStatus::kBadReputation},
          {"http://domain.test/foo.html?bar=baz",
           security_state::SafetyTipStatus::kBadReputation},

          {"http://directory.test", security_state::SafetyTipStatus::kNone},
          {"http://directory.test/bar", security_state::SafetyTipStatus::kNone},
          {"http://directory.test/bar/foo.html",
           security_state::SafetyTipStatus::kNone},
          {"http://directory.test/foo", security_state::SafetyTipStatus::kNone},
          {"http://directory.test/foo/bar/",
           security_state::SafetyTipStatus::kBadReputation},
          {"http://directory.test/foo/bar.html?bar=baz",
           security_state::SafetyTipStatus::kBadReputation},

          {"http://path.test", security_state::SafetyTipStatus::kNone},
          {"http://path.test/foo", security_state::SafetyTipStatus::kNone},
          {"http://path.test/foo/bar/", security_state::SafetyTipStatus::kNone},
          {"http://path.test/foo/bar.htm",
           security_state::SafetyTipStatus::kNone},
          {"http://path.test/foo/bar.html",
           security_state::SafetyTipStatus::kBadReputation},
          {"http://path.test/foo/bar.html?bar=baz",
           security_state::SafetyTipStatus::kBadReputation},
          {"http://path.test/bar/foo.html",
           security_state::SafetyTipStatus::kNone},

          {"http://query.test", security_state::SafetyTipStatus::kNone},
          {"http://query.test/foo", security_state::SafetyTipStatus::kNone},
          {"http://query.test/foo/bar/",
           security_state::SafetyTipStatus::kNone},
          {"http://query.test/foo/bar.html",
           security_state::SafetyTipStatus::kNone},
          {"http://query.test/foo/bar.html?baz=test",
           security_state::SafetyTipStatus::kBadReputation},
          {"http://query.test/foo/bar.html?baz=test&a=1",
           security_state::SafetyTipStatus::kNone},
          {"http://query.test/foo/bar.html?baz=no",
           security_state::SafetyTipStatus::kNone},

          {"http://subdomain.test", security_state::SafetyTipStatus::kNone},
          {"http://sub.subdomain.test",
           security_state::SafetyTipStatus::kBadReputation},
          {"http://sub.subdomain.test/foo/bar",
           security_state::SafetyTipStatus::kBadReputation},
          {"http://sub.subdomain.test/foo.html?bar=baz",
           security_state::SafetyTipStatus::kBadReputation},
      };

  for (auto test : kTests) {
    EXPECT_EQ(GetSafetyTipUrlBlockType(GURL(test.first)), test.second);
  }
}
