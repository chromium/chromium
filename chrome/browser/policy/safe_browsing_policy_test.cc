// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/safe_browsing_policy_test.h"

#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

SafeBrowsingPolicyTest::SafeBrowsingPolicyTest() = default;

SafeBrowsingPolicyTest::~SafeBrowsingPolicyTest() = default;

bool SafeBrowsingPolicyTest::IsShowingInterstitial(content::WebContents* tab) {
  return chrome_browser_interstitials::IsShowingInterstitial(tab);
}

}  // namespace policy
