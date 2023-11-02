// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_SAFE_SEARCH_POLICY_TEST_H_
#define CHROME_BROWSER_POLICY_SAFE_SEARCH_POLICY_TEST_H_

#include <string>

#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Browser;

namespace policy {

class SafeSearchPolicyTest : public PolicyTest {
 protected:
  SafeSearchPolicyTest();
  ~SafeSearchPolicyTest() override;

  void ApplySafeSearchPolicy(absl::optional<base::Value> legacy_safe_search,
                             absl::optional<base::Value> google_safe_search,
                             absl::optional<base::Value> legacy_youtube,
                             absl::optional<base::Value> youtube_restrict);

  static GURL GetExpectedSearchURL(bool expect_safe_search);

  static void CheckSafeSearch(Browser* browser,
                              bool expect_safe_search,
                              const std::string& url = "http://google.com/");
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_SAFE_SEARCH_POLICY_TEST_H_
