// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_URL_BLOCKING_POLICY_TEST_UTILS_H_
#define CHROME_BROWSER_POLICY_URL_BLOCKING_POLICY_TEST_UTILS_H_

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"

class Browser;
class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace policy {

class UrlBlockingPolicyTest : public PolicyTest {
 public:
  UrlBlockingPolicyTest();
  ~UrlBlockingPolicyTest() override;

  // Verifies that access to the given url |spec| is blocked and that
  // the correct error page is displayed.
  void CheckURLIsBlockedInWebContents(
      content::WebContents* web_contents,
      const GURL& url,
      bool is_blocked_by_incognito_policy = false);

  // Verifies that access to the given url |spec| is blocked  and that
  // the correct error page is displayed.
  void CheckURLIsBlocked(Browser* browser,
                         const std::string& spec,
                         bool is_blocked_by_incognito_policy = false);

  // Verifies that access to |view-source:spec| is blocked.
  void CheckViewSourceURLIsBlocked(Browser* browser, const std::string& spec);

 private:
  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  test::ScopedPrewarmFeatureList scoped_prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_URL_BLOCKING_POLICY_TEST_UTILS_H_
