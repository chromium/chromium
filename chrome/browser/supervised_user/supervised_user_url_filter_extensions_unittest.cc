// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS), "For Enabled extensions only");

namespace supervised_user {
namespace {

// URL filter delegate that verifies url extensions support.
class FakeURLFilterDelegate : public SupervisedUserURLFilter::Delegate {
 public:
  bool SupportsWebstoreURL(const GURL& url) const override {
    return IsSupportedChromeExtensionURL(url);
  }
};

class SupervisedUserURLFilterExtensionsTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  std::unique_ptr<TestingProfile> CreateTestingProfile() override {
    return TestingProfile::Builder()
        .SetIsSupervisedProfile()
        .AddTestingFactory(
            SupervisedUserServiceFactory::GetInstance(),
            base::BindRepeating(
                &supervised_user_test_util::BuildSupervisedUserService<
                    SupervisedUserURLFilter, FakeURLFilterDelegate>))
        .Build();
  }

  SupervisedUserURLFilter& filter() {
    return *SupervisedUserServiceFactory::GetForProfile(profile())
                ->GetURLFilter();
  }
};

TEST_F(SupervisedUserURLFilterExtensionsTest,
       ChromeWebstoreURLsAreAlwaysAllowed) {
  // When installing an extension from Chrome Webstore, it tries to download the
  // crx file from "https://clients2.google.com/service/update2/", which
  // redirects to "https://clients2.googleusercontent.com/crx/blobs/"
  // or "https://chrome.google.com/webstore/download/".
  // All URLs should be allowed regardless from the default filtering
  // behavior.
  GURL crx_download_url1(
      "https://clients2.google.com/service/update2/"
      "crx?response=redirect&os=linux&arch=x64&nacl_arch=x86-64&prod="
      "chromiumcrx&prodchannel=&prodversion=55.0.2882.0&lang=en-US&x=id%"
      "3Dciniambnphakdoflgeamacamhfllbkmo%26installsource%3Dondemand%26uc");
  GURL crx_download_url2(
      "https://clients2.googleusercontent.com/crx/blobs/"
      "QgAAAC6zw0qH2DJtnXe8Z7rUJP1iCQF099oik9f2ErAYeFAX7_"
      "CIyrNH5qBru1lUSBNvzmjILCGwUjcIBaJqxgegSNy2melYqfodngLxKtHsGBehAMZSmuWSg6"
      "FupAcPS3Ih6NSVCOB9KNh6Mw/extension_2_0.crx");
  GURL crx_download_url3(
      "https://chrome.google.com/webstore/download/"
      "QgAAAC6zw0qH2DJtnXe8Z7rUJP1iCQF099oik9f2ErAYeFAX7_"
      "CIyrNH5qBru1lUSBNvzmjILCGwUjcIBaJqxgegSNy2melYqfodngLxKtHsGBehAMZSmuWSg6"
      "FupAcPS3Ih6NSVCOB9KNh6Mw/extension_2_0.crx");
  // The actual Webstore URLs should also be allowed regardless of filtering
  // behavior,
  GURL webstore_url("https://chrome.google.com/webstore");
  GURL new_webstore_url("https://chromewebstore.google.com/");

  supervised_user_test_util::SetWebFilterType(profile(),
                                              WebFilterType::kCertainSites);
  EXPECT_TRUE(filter().GetFilteringBehavior(crx_download_url1).IsAllowed());
  EXPECT_TRUE(filter().GetFilteringBehavior(crx_download_url2).IsAllowed());
  EXPECT_TRUE(filter().GetFilteringBehavior(crx_download_url3).IsAllowed());
  EXPECT_TRUE(filter().GetFilteringBehavior(webstore_url).IsAllowed());
  EXPECT_TRUE(filter().GetFilteringBehavior(new_webstore_url).IsAllowed());

  // Set explicit host rules to block those website, and make sure the
  // URLs still work.
  supervised_user_test_util::SetManualFilterForHost(
      profile(), "clients2.google.com", /*allowlist=*/false);
  supervised_user_test_util::SetManualFilterForHost(
      profile(), "clients2.googleusercontent.com",
      /*allowlist=*/false);
  supervised_user_test_util::SetManualFilterForHost(
      profile(), "chrome.google.com", /*allowlist=*/false);
  supervised_user_test_util::SetManualFilterForHost(
      profile(), "chromewebstore.google.com", /*allowlist=*/false);
  supervised_user_test_util::SetWebFilterType(profile(),
                                              WebFilterType::kAllowAllSites);

  EXPECT_TRUE(filter().GetFilteringBehavior(crx_download_url1).IsAllowed());
  EXPECT_TRUE(filter().GetFilteringBehavior(crx_download_url2).IsAllowed());
  EXPECT_TRUE(filter().GetFilteringBehavior(crx_download_url3).IsAllowed());
  EXPECT_TRUE(filter().GetFilteringBehavior(webstore_url).IsAllowed());
  EXPECT_TRUE(filter().GetFilteringBehavior(new_webstore_url).IsAllowed());
}

}  // namespace
}  // namespace supervised_user
