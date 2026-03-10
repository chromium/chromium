// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/captcha_provider_manager.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_load_metrics {

class CaptchaProviderManagerTest : public testing::Test {
 public:
  CaptchaProviderManagerTest()
      : manager_(CaptchaProviderManager::CreateForTesting()) {}
  ~CaptchaProviderManagerTest() override = default;

 protected:
  CaptchaProviderManager manager_;
};

TEST_F(CaptchaProviderManagerTest, IsCaptchaUrl) {
  // The manager is initially not loaded and empty.
  EXPECT_FALSE(manager_.loaded());
  EXPECT_TRUE(manager_.empty());
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://example.com")));

  // Set the list of captcha providers.
  std::vector<std::string> providers = {
      "provider1.com", "provider2.com/captcha", "provider3.com/page/*",
      "*provider4.com/captcha", "*sub.provider5.com/*"};
  manager_.SetCaptchaProviders(providers);
  EXPECT_TRUE(manager_.loaded());
  EXPECT_FALSE(manager_.empty());

  // Provider 1: match all URL paths on the host. No subdomain match.
  EXPECT_TRUE(manager_.IsCaptchaUrl(GURL("https://provider1.com")));
  EXPECT_TRUE(manager_.IsCaptchaUrl(GURL("https://provider1.com/")));
  EXPECT_TRUE(manager_.IsCaptchaUrl(GURL("https://provider1.com/page")));
  EXPECT_TRUE(
      manager_.IsCaptchaUrl(GURL("https://provider1.com/page?query=1")));
  EXPECT_TRUE(manager_.IsCaptchaUrl(GURL("https://provider1.com/page#ref")));
  EXPECT_TRUE(manager_.IsCaptchaUrl(GURL("https://PROVIDER1.com")));
  EXPECT_TRUE(manager_.IsCaptchaUrl(GURL("http://provider1.com/page")));
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("http://www.provider1.com/page")));
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("http://sub.provider1.com/page")));
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://notprovider1.com/page")));
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://provider1.com.evil.com/")));

  // Provider 2: match specific URL path on the host. No subdomain match.
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://provider2.com")));
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://provider2.com/")));
  EXPECT_TRUE(manager_.IsCaptchaUrl(GURL("https://provider2.com/captcha")));
  EXPECT_FALSE(
      manager_.IsCaptchaUrl(GURL("https://provider2.com/captcha/subpage")));
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://provider2.com/another")));
  EXPECT_FALSE(
      manager_.IsCaptchaUrl(GURL("https://sub.provider2.com/captcha")));

  // Provider 3: match all URL paths with a specific prefix on the host. No
  // subdomain match.
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://provider3.com")));
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://provider3.com/")));
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://provider3.com/page")));
  EXPECT_TRUE(
      manager_.IsCaptchaUrl(GURL("https://provider3.com/page/subpage")));
  EXPECT_TRUE(manager_.IsCaptchaUrl(
      GURL("https://provider3.com/page/subpage?query=1")));
  EXPECT_TRUE(
      manager_.IsCaptchaUrl(GURL("https://provider3.com/page/subpage/more")));
  EXPECT_TRUE(manager_.IsCaptchaUrl(GURL("https://provider3.com/page/other")));
  EXPECT_FALSE(
      manager_.IsCaptchaUrl(GURL("https://sub.provider3.com/page/subpage")));
  EXPECT_FALSE(
      manager_.IsCaptchaUrl(GURL("https://notprovider3.com/page/subpage")));

  // Provider 4: match specific URL path on the host. Match all subdomains.
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://provider4.com")));
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://provider4.com/")));
  EXPECT_TRUE(manager_.IsCaptchaUrl(GURL("https://provider4.com/captcha")));
  EXPECT_FALSE(
      manager_.IsCaptchaUrl(GURL("https://provider4.com/captcha/subpage")));
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://provider4.com/another")));
  EXPECT_TRUE(manager_.IsCaptchaUrl(GURL("https://sub.provider4.com/captcha")));
  EXPECT_TRUE(
      manager_.IsCaptchaUrl(GURL("https://other.provider4.com/captcha")));
  EXPECT_TRUE(
      manager_.IsCaptchaUrl(GURL("https://sub.other.provider4.com/captcha")));
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://notprovider4.com/captcha")));

  // Provider 5: match all URL paths on the host. Match all subdomains.
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://provider5.com")));
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://provider5.com/")));
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://provider5.com/page")));
  EXPECT_TRUE(manager_.IsCaptchaUrl(GURL("https://sub.provider5.com")));
  EXPECT_TRUE(manager_.IsCaptchaUrl(GURL("https://sub.provider5.com/")));
  EXPECT_TRUE(manager_.IsCaptchaUrl(GURL("https://sub.provider5.com/page")));
  EXPECT_TRUE(manager_.IsCaptchaUrl(GURL("https://sub.provider5.com/other")));
  EXPECT_TRUE(
      manager_.IsCaptchaUrl(GURL("https://sub.provider5.com/page/more")));
  EXPECT_TRUE(
      manager_.IsCaptchaUrl(GURL("https://other.sub.provider5.com/page")));
  EXPECT_TRUE(
      manager_.IsCaptchaUrl(GURL("https://other.sub.provider5.com/page/more")));
  EXPECT_FALSE(
      manager_.IsCaptchaUrl(GURL("https://notsub.provider5.com/page")));
  EXPECT_FALSE(manager_.IsCaptchaUrl(GURL("https://other.provider5.com/page")));
}

TEST_F(CaptchaProviderManagerTest, GetCaptchaProviderForUrl) {
  // The manager is initially not loaded and empty.
  EXPECT_FALSE(manager_.loaded());
  EXPECT_TRUE(manager_.empty());

  // Set the list of captcha providers.
  std::vector<std::string> providers = {
      "*google.com/recaptcha/api2/anchor",
      "*recaptcha.net/recaptcha/api2/anchor",
      "*hcaptcha.com/captcha/*",
      "*challenges.cloudflare.com/*",
      "*other.com/captcha/*",
  };
  manager_.SetCaptchaProviders(providers);
  EXPECT_TRUE(manager_.loaded());
  EXPECT_FALSE(manager_.empty());

  // Verify that the captcha provider is correctly identified for each URL.
  EXPECT_EQ(manager_.GetCaptchaProviderForUrl(
                GURL("https://www.google.com/recaptcha/api2/anchor?query=1")),
            CaptchaProvider::kReCaptcha);
  EXPECT_EQ(manager_.GetCaptchaProviderForUrl(
                GURL("https://www.recaptcha.net/recaptcha/api2/anchor")),
            CaptchaProvider::kReCaptcha);
  EXPECT_EQ(manager_.GetCaptchaProviderForUrl(
                GURL("https://www.hcaptcha.com/captcha/index.html")),
            CaptchaProvider::kHCaptcha);
  EXPECT_EQ(manager_.GetCaptchaProviderForUrl(
                GURL("https://challenges.cloudflare.com/turnstile/index.html")),
            CaptchaProvider::kCloudflareTurnstile);
  EXPECT_EQ(manager_.GetCaptchaProviderForUrl(
                GURL("https://other.com/captcha/index.html")),
            CaptchaProvider::kUnknown);
  EXPECT_EQ(manager_.GetCaptchaProviderForUrl(
                GURL("https://not.captcha.com/index.html")),
            std::nullopt);
  EXPECT_EQ(manager_.GetCaptchaProviderForUrl(
                GURL("https://example.com/google.com/recaptcha/api2/anchor")),
            std::nullopt);
}

}  // namespace page_load_metrics
