// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_url_filter.h"

#include <map>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS), "For Enabled extensions only");

namespace {

// URL filter delegate that verifies url extensions support.
class FakeURLFilterDelegate
    : public supervised_user::SupervisedUserURLFilter::Delegate {
 public:
  bool SupportsWebstoreURL(const GURL& url) const override {
    return supervised_user::IsSupportedChromeExtensionURL(url);
  }
};

}  // namespace

class SupervisedUserURLFilterExtensionsTest : public ::testing::Test {
 public:
  SupervisedUserURLFilterExtensionsTest() {
    filter_.SetURLCheckerClient(
        std::make_unique<safe_search_api::FakeURLCheckerClient>());
    filter_.SetDefaultFilteringBehavior(
        supervised_user::FilteringBehavior::kBlock);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  supervised_user::SupervisedUserURLFilter filter_ =
      supervised_user::SupervisedUserURLFilter(
          pref_service_,
          std::make_unique<FakeURLFilterDelegate>());
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

  filter_.SetDefaultFilteringBehavior(
      supervised_user::FilteringBehavior::kBlock);
  EXPECT_EQ(supervised_user::FilteringBehavior::kAllow,
            filter_.GetFilteringBehaviorForURL(crx_download_url1));
  EXPECT_EQ(supervised_user::FilteringBehavior::kAllow,
            filter_.GetFilteringBehaviorForURL(crx_download_url2));
  EXPECT_EQ(supervised_user::FilteringBehavior::kAllow,
            filter_.GetFilteringBehaviorForURL(crx_download_url3));
  EXPECT_EQ(supervised_user::FilteringBehavior::kAllow,
            filter_.GetFilteringBehaviorForURL(webstore_url));
  EXPECT_EQ(supervised_user::FilteringBehavior::kAllow,
            filter_.GetFilteringBehaviorForURL(new_webstore_url));

  // Set explicit host rules to block those website, and make sure the
  // URLs still work.
  std::map<std::string, bool> hosts;
  hosts["clients2.google.com"] = false;
  hosts["clients2.googleusercontent.com"] = false;
  hosts["chrome.google.com"] = false;
  hosts["chromewebstore.google.com"] = false;
  filter_.SetManualHosts(std::move(hosts));
  filter_.SetDefaultFilteringBehavior(
      supervised_user::FilteringBehavior::kAllow);
  EXPECT_EQ(supervised_user::FilteringBehavior::kAllow,
            filter_.GetFilteringBehaviorForURL(crx_download_url1));
  EXPECT_EQ(supervised_user::FilteringBehavior::kAllow,
            filter_.GetFilteringBehaviorForURL(crx_download_url2));
  EXPECT_EQ(supervised_user::FilteringBehavior::kAllow,
            filter_.GetFilteringBehaviorForURL(crx_download_url3));
  EXPECT_EQ(supervised_user::FilteringBehavior::kAllow,
            filter_.GetFilteringBehaviorForURL(webstore_url));
  EXPECT_EQ(supervised_user::FilteringBehavior::kAllow,
            filter_.GetFilteringBehaviorForURL(new_webstore_url));
}
