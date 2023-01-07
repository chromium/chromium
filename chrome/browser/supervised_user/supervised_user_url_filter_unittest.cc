// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_url_filter.h"

#include <map>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class SupervisedUserURLFilterTest : public ::testing::Test,
                                    public SupervisedUserURLFilter::Observer {
 public:
  SupervisedUserURLFilterTest() {
    filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);
    filter_.AddObserver(this);
  }

  ~SupervisedUserURLFilterTest() override { filter_.RemoveObserver(this); }

  // SupervisedUserURLFilter::Observer:
  void OnSiteListUpdated() override { run_loop_.Quit(); }
  void OnURLChecked(const GURL& url,
                    SupervisedUserURLFilter::FilteringBehavior behavior,
                    supervised_user_error_page::FilteringBehaviorReason reason,
                    bool uncertain) override {
    behavior_ = behavior;
    reason_ = reason;
  }

 protected:
  bool IsURLAllowlisted(const std::string& url) {
    return filter_.GetFilteringBehaviorForURL(GURL(url)) ==
           SupervisedUserURLFilter::ALLOW;
  }

  void ExpectURLInDefaultAllowlist(const std::string& url) {
    ExpectURLCheckMatches(url, SupervisedUserURLFilter::ALLOW,
                          supervised_user_error_page::DEFAULT);
  }

  void ExpectURLInDefaultDenylist(const std::string& url) {
    ExpectURLCheckMatches(url, SupervisedUserURLFilter::BLOCK,
                          supervised_user_error_page::DEFAULT);
  }

  void ExpectURLInManualAllowlist(const std::string& url) {
    ExpectURLCheckMatches(url, SupervisedUserURLFilter::ALLOW,
                          supervised_user_error_page::MANUAL);
  }

  void ExpectURLInManualDenylist(const std::string& url) {
    ExpectURLCheckMatches(url, SupervisedUserURLFilter::BLOCK,
                          supervised_user_error_page::MANUAL);
  }

  base::test::TaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  SupervisedUserURLFilter filter_;
  SupervisedUserURLFilter::FilteringBehavior behavior_;
  supervised_user_error_page::FilteringBehaviorReason reason_;

 private:
  void ExpectURLCheckMatches(
      const std::string& url,
      SupervisedUserURLFilter::FilteringBehavior expected_behavior,
      supervised_user_error_page::FilteringBehaviorReason expected_reason,
      bool skip_manual_parent_filter = false) {
    bool called_synchronously =
        filter_.GetFilteringBehaviorForURLWithAsyncChecks(
            GURL(url), base::DoNothing(), skip_manual_parent_filter);
    ASSERT_TRUE(called_synchronously);

    EXPECT_EQ(behavior_, expected_behavior);
    EXPECT_EQ(reason_, expected_reason);
  }
};

TEST_F(SupervisedUserURLFilterTest, Basic) {
  std::map<std::string, bool> hosts;
  hosts["*.google.com"] = true;

  filter_.SetManualHosts(std::move(hosts));
  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);

  EXPECT_TRUE(IsURLAllowlisted("http://google.com"));
  EXPECT_TRUE(IsURLAllowlisted("http://google.com/"));
  EXPECT_TRUE(IsURLAllowlisted("http://google.com/whatever"));
  EXPECT_TRUE(IsURLAllowlisted("https://google.com/"));
  EXPECT_FALSE(IsURLAllowlisted("http://notgoogle.com/"));
  EXPECT_TRUE(IsURLAllowlisted("http://mail.google.com"));
  EXPECT_TRUE(IsURLAllowlisted("http://x.mail.google.com"));
  EXPECT_TRUE(IsURLAllowlisted("https://x.mail.google.com/"));
  EXPECT_TRUE(IsURLAllowlisted("http://x.y.google.com/a/b"));
  EXPECT_FALSE(IsURLAllowlisted("http://youtube.com/"));

  EXPECT_TRUE(IsURLAllowlisted("bogus://youtube.com/"));
  EXPECT_TRUE(IsURLAllowlisted("chrome://youtube.com/"));
  EXPECT_TRUE(IsURLAllowlisted("chrome://extensions/"));
  EXPECT_TRUE(IsURLAllowlisted("chrome-extension://foo/main.html"));
  EXPECT_TRUE(IsURLAllowlisted("file:///home/chronos/user/Downloads/img.jpg"));
}

TEST_F(SupervisedUserURLFilterTest, EffectiveURL) {
  std::map<std::string, bool> hosts;
  hosts["example.com"] = true;

  filter_.SetManualHosts(std::move(hosts));
  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);

  ASSERT_TRUE(IsURLAllowlisted("http://example.com"));
  ASSERT_TRUE(IsURLAllowlisted("https://example.com"));

  // AMP Cache URLs.
  EXPECT_FALSE(IsURLAllowlisted("https://cdn.ampproject.org"));
  EXPECT_TRUE(IsURLAllowlisted("https://cdn.ampproject.org/c/example.com"));
  EXPECT_TRUE(IsURLAllowlisted("https://cdn.ampproject.org/c/www.example.com"));
  EXPECT_TRUE(
      IsURLAllowlisted("https://cdn.ampproject.org/c/example.com/path"));
  EXPECT_TRUE(IsURLAllowlisted("https://cdn.ampproject.org/c/s/example.com"));
  EXPECT_FALSE(IsURLAllowlisted("https://cdn.ampproject.org/c/other.com"));

  EXPECT_FALSE(IsURLAllowlisted("https://sub.cdn.ampproject.org"));
  EXPECT_TRUE(IsURLAllowlisted("https://sub.cdn.ampproject.org/c/example.com"));
  EXPECT_TRUE(
      IsURLAllowlisted("https://sub.cdn.ampproject.org/c/www.example.com"));
  EXPECT_TRUE(
      IsURLAllowlisted("https://sub.cdn.ampproject.org/c/example.com/path"));
  EXPECT_TRUE(
      IsURLAllowlisted("https://sub.cdn.ampproject.org/c/s/example.com"));
  EXPECT_FALSE(IsURLAllowlisted("https://sub.cdn.ampproject.org/c/other.com"));

  // Google AMP viewer URLs.
  EXPECT_FALSE(IsURLAllowlisted("https://www.google.com"));
  EXPECT_FALSE(IsURLAllowlisted("https://www.google.com/amp/"));
  EXPECT_TRUE(IsURLAllowlisted("https://www.google.com/amp/example.com"));
  EXPECT_TRUE(IsURLAllowlisted("https://www.google.com/amp/www.example.com"));
  EXPECT_TRUE(IsURLAllowlisted("https://www.google.com/amp/s/example.com"));
  EXPECT_TRUE(
      IsURLAllowlisted("https://www.google.com/amp/s/example.com/path"));
  EXPECT_FALSE(IsURLAllowlisted("https://www.google.com/amp/other.com"));

  // Google web cache URLs.
  EXPECT_FALSE(IsURLAllowlisted("https://webcache.googleusercontent.com"));
  EXPECT_FALSE(
      IsURLAllowlisted("https://webcache.googleusercontent.com/search"));
  EXPECT_FALSE(IsURLAllowlisted(
      "https://webcache.googleusercontent.com/search?q=example.com"));
  EXPECT_TRUE(IsURLAllowlisted(
      "https://webcache.googleusercontent.com/search?q=cache:example.com"));
  EXPECT_TRUE(
      IsURLAllowlisted("https://webcache.googleusercontent.com/"
                       "search?q=cache:example.com+search_query"));
  EXPECT_TRUE(
      IsURLAllowlisted("https://webcache.googleusercontent.com/"
                       "search?q=cache:123456789-01:example.com+search_query"));
  EXPECT_FALSE(IsURLAllowlisted(
      "https://webcache.googleusercontent.com/search?q=cache:other.com"));
  EXPECT_FALSE(
      IsURLAllowlisted("https://webcache.googleusercontent.com/"
                       "search?q=cache:other.com+example.com"));
  EXPECT_FALSE(
      IsURLAllowlisted("https://webcache.googleusercontent.com/"
                       "search?q=cache:123456789-01:other.com+example.com"));

  // Google Translate URLs.
  EXPECT_FALSE(IsURLAllowlisted("https://translate.google.com"));
  EXPECT_FALSE(IsURLAllowlisted("https://translate.googleusercontent.com"));
  EXPECT_TRUE(
      IsURLAllowlisted("https://translate.google.com/translate?u=example.com"));
  EXPECT_TRUE(IsURLAllowlisted(
      "https://translate.googleusercontent.com/translate?u=example.com"));
  EXPECT_TRUE(IsURLAllowlisted(
      "https://translate.google.com/translate?u=www.example.com"));
  EXPECT_TRUE(IsURLAllowlisted(
      "https://translate.google.com/translate?u=https://example.com"));
  EXPECT_FALSE(
      IsURLAllowlisted("https://translate.google.com/translate?u=other.com"));
}

TEST_F(SupervisedUserURLFilterTest, Inactive) {
  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::ALLOW);
  std::map<std::string, bool> hosts;
  hosts["google.com"] = true;

  filter_.SetManualHosts(std::move(hosts));

  // If the filter is inactive, every URL should be allowed.
  EXPECT_TRUE(IsURLAllowlisted("http://google.com"));
  EXPECT_TRUE(IsURLAllowlisted("https://www.example.com"));
}

TEST_F(SupervisedUserURLFilterTest, IPAddress) {
  std::map<std::string, bool> hosts;
  hosts["123.123.123.123"] = true;

  filter_.SetManualHosts(std::move(hosts));
  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);

  EXPECT_TRUE(IsURLAllowlisted("http://123.123.123.123/"));
  EXPECT_FALSE(IsURLAllowlisted("http://123.123.123.124/"));
}

TEST_F(SupervisedUserURLFilterTest, Canonicalization) {
  // We assume that the hosts and URLs are already canonicalized.
  std::map<std::string, bool> hosts;
  hosts["www.moose.org"] = true;
  hosts["www.xn--n3h.net"] = true;
  std::map<GURL, bool> urls;
  urls[GURL("http://www.example.com/foo/")] = true;
  urls[GURL("http://www.example.com/%C3%85t%C3%B8mstr%C3%B6m")] = true;
  filter_.SetManualHosts(std::move(hosts));
  filter_.SetManualURLs(std::move(urls));

  // Base cases.
  EXPECT_TRUE(IsURLAllowlisted("http://www.example.com/foo/"));
  EXPECT_TRUE(
      IsURLAllowlisted("http://www.example.com/%C3%85t%C3%B8mstr%C3%B6m"));

  // Verify that non-URI characters are escaped.
  EXPECT_TRUE(IsURLAllowlisted(
      "http://www.example.com/\xc3\x85t\xc3\xb8mstr\xc3\xb6m"));

  // Verify that unnecessary URI escapes are unescaped.
  EXPECT_TRUE(IsURLAllowlisted("http://www.example.com/%66%6F%6F/"));

  // Verify that the default port are removed.
  EXPECT_TRUE(IsURLAllowlisted("http://www.example.com:80/foo/"));

  // Verify that scheme and hostname are lowercased.
  EXPECT_TRUE(IsURLAllowlisted("htTp://wWw.eXamPle.com/foo/"));
  EXPECT_TRUE(IsURLAllowlisted("HttP://WwW.mOOsE.orG/blurp/"));

  // Verify that UTF-8 in hostnames are converted to punycode.
  EXPECT_TRUE(IsURLAllowlisted("http://www.\xe2\x98\x83\x0a.net/bla/"));

  // Verify that query and ref are stripped.
  EXPECT_TRUE(IsURLAllowlisted("http://www.example.com/foo/?bar=baz#ref"));
}

TEST_F(SupervisedUserURLFilterTest, UrlWithNonStandardUrlSchemeAllowed) {
  // Non-standard url scheme.
  EXPECT_TRUE(IsURLAllowlisted("file://example.com"));
  EXPECT_TRUE(IsURLAllowlisted("filesystem://80cols.com"));
  EXPECT_TRUE(IsURLAllowlisted("chrome://example.com"));
  EXPECT_TRUE(IsURLAllowlisted("wtf://example.com"));
  EXPECT_TRUE(IsURLAllowlisted("gopher://example.com"));

  // Standard url scheme.
  EXPECT_FALSE(IsURLAllowlisted(("http://example.com")));
  EXPECT_FALSE(IsURLAllowlisted("https://example.com"));
  EXPECT_FALSE(IsURLAllowlisted("ftp://example.com"));
  EXPECT_FALSE(IsURLAllowlisted("ws://example.com"));
  EXPECT_FALSE(IsURLAllowlisted("wss://example.com"));
}

TEST_F(SupervisedUserURLFilterTest, HostMatchesPattern) {
  EXPECT_TRUE(SupervisedUserURLFilter::HostMatchesPattern("www.google.com",
                                                          "google.com"));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com",
                                                  "*.google.com"));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HostMatchesPattern("google.com",
                                                  "*.google.com"));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HostMatchesPattern("accounts.google.com",
                                                  "*.google.com"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.de",
                                                  "*.google.com"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("notgoogle.com",
                                                  "*.google.com"));


  EXPECT_TRUE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com",
                                                  "www.google.*"));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.de",
                                                  "www.google.*"));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.co.uk",
                                                  "www.google.*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.blogspot.com",
                                                  "www.google.*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google",
                                                  "www.google.*"));
  EXPECT_TRUE(SupervisedUserURLFilter::HostMatchesPattern("google.com",
                                                          "www.google.*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("mail.google.com",
                                                  "www.google.*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.googleplex.com",
                                                  "www.google.*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.googleco.uk",
                                                  "www.google.*"));


  EXPECT_TRUE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com",
                                                  "*.google.*"));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HostMatchesPattern("google.com",
                                                  "*.google.*"));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HostMatchesPattern("accounts.google.com",
                                                  "*.google.*"));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HostMatchesPattern("mail.google.com",
                                                  "*.google.*"));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.de",
                                                  "*.google.*"));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HostMatchesPattern("google.de",
                                                  "*.google.*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("google.blogspot.com",
                                                  "*.google.*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("google", "*.google.*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("notgoogle.com",
                                                  "*.google.*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.googleplex.com",
                                                  "*.google.*"));

  // Now test a few invalid patterns. They should never match.
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", ""));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", "."));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", "*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", ".*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", "*."));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", "*.*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google..com", "*..*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", "*.*.com"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com", "www.*.*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com",
                                                  "*.goo.*le.*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com",
                                                  "*google*"));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("www.google.com",
                                                  "www.*.google.com"));
}

TEST_F(SupervisedUserURLFilterTest, PatternsWithoutConflicts) {
  std::map<std::string, bool> hosts;

  // The third rule is redundant with the first, but it's not a conflict
  // since they have the same value (allow).
  hosts["*.google.com"] = true;
  hosts["calendar.google.com"] = false;
  hosts["mail.google.com"] = true;

  filter_.SetManualHosts(std::move(hosts));
  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);

  EXPECT_TRUE(IsURLAllowlisted("http://www.google.com/foo/"));
  EXPECT_FALSE(IsURLAllowlisted("http://calendar.google.com/bar/"));
  EXPECT_TRUE(IsURLAllowlisted("http://mail.google.com/moose/"));
  EXPECT_FALSE(IsURLAllowlisted("http://www.google.co.uk/blurp/"));

  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::ALLOW);

  EXPECT_TRUE(IsURLAllowlisted("http://www.google.com/foo/"));
  EXPECT_FALSE(IsURLAllowlisted("http://calendar.google.com/bar/"));
  EXPECT_TRUE(IsURLAllowlisted("http://mail.google.com/moose/"));
  EXPECT_TRUE(IsURLAllowlisted("http://www.google.co.uk/blurp/"));
}

TEST_F(SupervisedUserURLFilterTest, PatternsWithConflicts) {
  std::map<std::string, bool> hosts;
  base::HistogramTester histogram_tester;

  // First and second rule always conflicting.
  // The fourth rule conflicts with the first for "www.google.com" host.
  // Blocking then takes precedence.
  hosts["*.google.com"] = true;
  hosts["calendar.google.com"] = false;
  hosts["mail.google.com"] = true;
  hosts["www.google.*"] = false;

  filter_.SetManualHosts(std::move(hosts));
  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);

  EXPECT_FALSE(IsURLAllowlisted("http://www.google.com/foo/"));
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest(),
      1, 1);
  // Match with conflicting first and second rule.
  EXPECT_FALSE(IsURLAllowlisted("http://calendar.google.com/bar/"));
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest(),
      1, 2);

  // Match with first and third rule both allowed, no conflict.
  EXPECT_TRUE(IsURLAllowlisted("http://mail.google.com/moose/"));
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest(),
      0, 1);

  // Match with fourth rule.
  EXPECT_FALSE(IsURLAllowlisted("http://www.google.co.uk/blurp/"));
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest(),
      0, 2);

  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::ALLOW);

  EXPECT_FALSE(IsURLAllowlisted("http://www.google.com/foo/"));
  EXPECT_FALSE(IsURLAllowlisted("http://calendar.google.com/bar/"));
  EXPECT_TRUE(IsURLAllowlisted("http://mail.google.com/moose/"));
  EXPECT_FALSE(IsURLAllowlisted("http://www.google.co.uk/blurp/"));
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest(),
      1, 4);
  histogram_tester.ExpectBucketCount(
      SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest(),
      0, 4);

  // No known rule, the metric is not recorded.
  EXPECT_TRUE(IsURLAllowlisted("https://youtube.com"));
  histogram_tester.ExpectTotalCount(
      SupervisedUserURLFilter::GetManagedSiteListConflictHistogramNameForTest(),
      8);
}

TEST_F(SupervisedUserURLFilterTest, Reason) {
  std::map<std::string, bool> hosts;
  std::map<GURL, bool> urls;
  hosts["youtube.com"] = true;
  hosts["*.google.*"] = true;
  urls[GURL("https://youtube.com/robots.txt")] = false;
  urls[GURL("https://google.co.uk/robots.txt")] = false;

  filter_.SetManualHosts(std::move(hosts));
  filter_.SetManualURLs(std::move(urls));

  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);

  ExpectURLInDefaultDenylist("https://m.youtube.com/feed/trending");
  ExpectURLInDefaultDenylist("https://com.google");
  ExpectURLInManualAllowlist("https://youtube.com/feed/trending");
  ExpectURLInManualAllowlist("https://google.com/humans.txt");
  ExpectURLInManualDenylist("https://youtube.com/robots.txt");
  ExpectURLInManualDenylist("https://google.co.uk/robots.txt");

  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::ALLOW);

  ExpectURLInDefaultAllowlist("https://m.youtube.com/feed/trending");
  ExpectURLInDefaultAllowlist("https://com.google");
  ExpectURLInManualAllowlist("https://youtube.com/feed/trending");
  ExpectURLInManualAllowlist("https://google.com/humans.txt");
  ExpectURLInManualDenylist("https://youtube.com/robots.txt");
  ExpectURLInManualDenylist("https://google.co.uk/robots.txt");
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(SupervisedUserURLFilterTest, ChromeWebstoreURLsAreAlwaysAllowed) {
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

  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(crx_download_url1));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(crx_download_url2));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(crx_download_url3));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(webstore_url));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(new_webstore_url));

  // Set explicit host rules to block those website, and make sure the
  // URLs still work.
  std::map<std::string, bool> hosts;
  hosts["clients2.google.com"] = false;
  hosts["clients2.googleusercontent.com"] = false;
  hosts["chrome.google.com"] = false;
  hosts["chromewebstore.google.com"] = false;
  filter_.SetManualHosts(std::move(hosts));
  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::ALLOW);
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(crx_download_url1));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(crx_download_url2));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(crx_download_url3));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(webstore_url));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(new_webstore_url));
}
#endif

TEST_F(SupervisedUserURLFilterTest, UrlsNotRequiringGuardianApprovalAllowed) {
  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);
  EXPECT_TRUE(IsURLAllowlisted("https://families.google.com/"));
  EXPECT_TRUE(IsURLAllowlisted("https://families.google.com"));
  EXPECT_TRUE(IsURLAllowlisted("https://families.google.com/something"));
  EXPECT_TRUE(IsURLAllowlisted("http://families.google.com/"));
  EXPECT_FALSE(IsURLAllowlisted("https://subdomain.families.google.com/"));
  EXPECT_TRUE(IsURLAllowlisted("https://myaccount.google.com/"));
  EXPECT_TRUE(IsURLAllowlisted("https://accounts.google.com/"));
  EXPECT_TRUE(IsURLAllowlisted("https://familylink.google.com/"));
  EXPECT_TRUE(IsURLAllowlisted("https://policies.google.com/"));
  EXPECT_TRUE(IsURLAllowlisted("https://support.google.com/"));

  // Chrome sync dashboard URLs (base initial URL, plus the version with locale
  // appended, and the redirect URL with locale appended).
  EXPECT_TRUE(IsURLAllowlisted("https://www.google.com/settings/chrome/sync"));
  EXPECT_TRUE(
      IsURLAllowlisted("https://www.google.com/settings/chrome/sync?hl=en-US"));
  EXPECT_TRUE(IsURLAllowlisted("https://chrome.google.com/sync?hl=en-US"));
}

TEST_F(SupervisedUserURLFilterTest, PlayTermsAlwaysAllowed) {
  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);
  EXPECT_TRUE(IsURLAllowlisted("https://play.google.com/about/play-terms"));
  EXPECT_TRUE(IsURLAllowlisted("https://play.google.com/about/play-terms/"));
  EXPECT_TRUE(IsURLAllowlisted(
      "https://play.google.com/intl/pt-BR_pt/about/play-terms/"));
  EXPECT_TRUE(
      IsURLAllowlisted("https://play.google.com/about/play-terms/index.html"));
  EXPECT_FALSE(IsURLAllowlisted("http://play.google.com/about/play-terms/"));
  EXPECT_FALSE(
      IsURLAllowlisted("https://subdomain.play.google.com/about/play-terms/"));
  EXPECT_FALSE(IsURLAllowlisted("https://play.google.com/"));
  EXPECT_FALSE(IsURLAllowlisted("https://play.google.com/about"));
}
