// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/developer_tools_policy_checker.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

class DeveloperToolsPolicyCheckerTest : public testing::Test {
 public:
  DeveloperToolsPolicyCheckerTest() = default;
  ~DeveloperToolsPolicyCheckerTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(DeveloperToolsPolicyCheckerTest, Blocklist) {
  base::Value::List blocklist;
  blocklist.Append("example.com");
  profile_.GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                               std::move(blocklist));
  DeveloperToolsPolicyChecker checker(profile_.GetPrefs());

  EXPECT_TRUE(checker.IsUrlBlockedByPolicy(GURL("http://example.com")));
  EXPECT_TRUE(checker.IsUrlBlockedByPolicy(GURL("http://www.example.com")));
  EXPECT_FALSE(checker.IsUrlBlockedByPolicy(GURL("http://chromium.org")));
}

TEST_F(DeveloperToolsPolicyCheckerTest, Allowlist) {
  base::Value::List blocklist;
  blocklist.Append("*");
  profile_.GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityBlocklist,
                               std::move(blocklist));

  base::Value::List allowlist;
  allowlist.Append("chromium.org");
  profile_.GetPrefs()->SetList(prefs::kDeveloperToolsAvailabilityAllowlist,
                               std::move(allowlist));
  DeveloperToolsPolicyChecker checker(profile_.GetPrefs());

  EXPECT_TRUE(checker.IsUrlBlockedByPolicy(GURL("http://example.com")));
  EXPECT_FALSE(checker.IsUrlBlockedByPolicy(GURL("http://chromium.org")));
  EXPECT_TRUE(checker.IsUrlAllowedByPolicy(GURL("http://chromium.org")));
  EXPECT_FALSE(checker.IsUrlAllowedByPolicy(GURL("http://example.com")));
}

}  // namespace policy
