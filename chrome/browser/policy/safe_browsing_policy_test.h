// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_SAFE_BROWSING_POLICY_TEST_H_
#define CHROME_BROWSER_POLICY_SAFE_BROWSING_POLICY_TEST_H_

#include "chrome/browser/policy/policy_test_utils.h"

namespace content {
class WebContents;
}  // namespace content

namespace policy {

class SafeBrowsingPolicyTest : public PolicyTest {
 public:
  SafeBrowsingPolicyTest();
  ~SafeBrowsingPolicyTest() override;

  bool IsShowingInterstitial(content::WebContents* tab);

  void WaitForInterstitial(content::WebContents* tab);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_SAFE_BROWSING_POLICY_TEST_H_
