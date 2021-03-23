// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_url_filter.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/supervised_user/supervised_user_site_list.h"
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
  std::vector<std::string> list;
  // Allow domain and all subdomains, for any filtered scheme.
  list.push_back("google.com");
  filter_.SetFromPatternsForTesting(list);
  run_loop_.Run();

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
  std::vector<std::string> list;
  // Allow domain and all subdomains, for any filtered scheme.
  list.push_back("example.com");
  filter_.SetFromPatternsForTesting(list);
  run_loop_.Run();

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

  std::vector<std::string> list;
  list.push_back("google.com");
  filter_.SetFromPatternsForTesting(list);
  run_loop_.Run();

  // If the filter is inactive, every URL should be allowed.
  EXPECT_TRUE(IsURLAllowlisted("http://google.com"));
  EXPECT_TRUE(IsURLAllowlisted("https://www.example.com"));
}

TEST_F(SupervisedUserURLFilterTest, Scheme) {
  std::vector<std::string> list;
  // Filter only http, ftp and ws schemes.
  list.push_back("http://secure.com");
  list.push_back("ftp://secure.com");
  list.push_back("ws://secure.com");
  filter_.SetFromPatternsForTesting(list);
  run_loop_.Run();

  EXPECT_TRUE(IsURLAllowlisted("http://secure.com"));
  EXPECT_TRUE(IsURLAllowlisted("http://secure.com/whatever"));
  EXPECT_TRUE(IsURLAllowlisted("ftp://secure.com/"));
  EXPECT_TRUE(IsURLAllowlisted("ws://secure.com"));
  EXPECT_FALSE(IsURLAllowlisted("https://secure.com/"));
  EXPECT_FALSE(IsURLAllowlisted("wss://secure.com"));
  EXPECT_TRUE(IsURLAllowlisted("http://www.secure.com"));
  EXPECT_FALSE(IsURLAllowlisted("https://www.secure.com"));
  EXPECT_FALSE(IsURLAllowlisted("wss://www.secure.com"));
}

TEST_F(SupervisedUserURLFilterTest, Path) {
  std::vector<std::string> list;
  // Filter only a certain path prefix.
  list.push_back("path.to/ruin");
  filter_.SetFromPatternsForTesting(list);
  run_loop_.Run();

  EXPECT_TRUE(IsURLAllowlisted("http://path.to/ruin"));
  EXPECT_TRUE(IsURLAllowlisted("https://path.to/ruin"));
  EXPECT_TRUE(IsURLAllowlisted("http://path.to/ruins"));
  EXPECT_TRUE(IsURLAllowlisted("http://path.to/ruin/signup"));
  EXPECT_TRUE(IsURLAllowlisted("http://www.path.to/ruin"));
  EXPECT_FALSE(IsURLAllowlisted("http://path.to/fortune"));
}

TEST_F(SupervisedUserURLFilterTest, PathAndScheme) {
  std::vector<std::string> list;
  // Filter only a certain path prefix and scheme.
  list.push_back("https://s.aaa.com/path");
  filter_.SetFromPatternsForTesting(list);
  run_loop_.Run();

  EXPECT_TRUE(IsURLAllowlisted("https://s.aaa.com/path"));
  EXPECT_TRUE(IsURLAllowlisted("https://s.aaa.com/path/bbb"));
  EXPECT_FALSE(IsURLAllowlisted("http://s.aaa.com/path"));
  EXPECT_FALSE(IsURLAllowlisted("https://aaa.com/path"));
  EXPECT_FALSE(IsURLAllowlisted("https://x.aaa.com/path"));
  EXPECT_FALSE(IsURLAllowlisted("https://s.aaa.com/bbb"));
  EXPECT_FALSE(IsURLAllowlisted("https://s.aaa.com/"));
}

TEST_F(SupervisedUserURLFilterTest, Host) {
  std::vector<std::string> list;
  // Filter only a certain hostname, without subdomains.
  list.push_back(".www.example.com");
  filter_.SetFromPatternsForTesting(list);
  run_loop_.Run();

  EXPECT_TRUE(IsURLAllowlisted("http://www.example.com"));
  EXPECT_FALSE(IsURLAllowlisted("http://example.com"));
  EXPECT_FALSE(IsURLAllowlisted("http://subdomain.example.com"));
}

TEST_F(SupervisedUserURLFilterTest, IPAddress) {
  std::vector<std::string> list;
  // Filter an ip address.
  list.push_back("123.123.123.123");
  filter_.SetFromPatternsForTesting(list);
  run_loop_.Run();

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

TEST_F(SupervisedUserURLFilterTest, HasFilteredScheme) {
  EXPECT_TRUE(
      SupervisedUserURLFilter::HasFilteredScheme(GURL("http://example.com")));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HasFilteredScheme(GURL("https://example.com")));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HasFilteredScheme(GURL("ftp://example.com")));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HasFilteredScheme(GURL("ws://example.com")));
  EXPECT_TRUE(
      SupervisedUserURLFilter::HasFilteredScheme(GURL("wss://example.com")));

  EXPECT_FALSE(
      SupervisedUserURLFilter::HasFilteredScheme(GURL("file://example.com")));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HasFilteredScheme(
          GURL("filesystem://80cols.com")));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HasFilteredScheme(GURL("chrome://example.com")));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HasFilteredScheme(GURL("wtf://example.com")));
  EXPECT_FALSE(
      SupervisedUserURLFilter::HasFilteredScheme(GURL("gopher://example.com")));
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
  EXPECT_FALSE(
      SupervisedUserURLFilter::HostMatchesPattern("google.com",
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
  hosts["accounts.google.com"] = false;
  hosts["mail.google.com"] = true;

  filter_.SetManualHosts(std::move(hosts));
  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);

  EXPECT_TRUE(IsURLAllowlisted("http://www.google.com/foo/"));
  EXPECT_FALSE(IsURLAllowlisted("http://accounts.google.com/bar/"));
  EXPECT_TRUE(IsURLAllowlisted("http://mail.google.com/moose/"));
  EXPECT_FALSE(IsURLAllowlisted("http://www.google.co.uk/blurp/"));

  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::ALLOW);

  EXPECT_TRUE(IsURLAllowlisted("http://www.google.com/foo/"));
  EXPECT_FALSE(IsURLAllowlisted("http://accounts.google.com/bar/"));
  EXPECT_TRUE(IsURLAllowlisted("http://mail.google.com/moose/"));
  EXPECT_TRUE(IsURLAllowlisted("http://www.google.co.uk/blurp/"));
}

TEST_F(SupervisedUserURLFilterTest, PatternsWithConflicts) {
  std::map<std::string, bool> hosts;

  // The fourth rule conflicts with the first for "www.google.com" host.
  // Blocking then takes precedence.
  hosts["*.google.com"] = true;
  hosts["accounts.google.com"] = false;
  hosts["mail.google.com"] = true;
  hosts["www.google.*"] = false;

  filter_.SetManualHosts(std::move(hosts));
  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);

  EXPECT_FALSE(IsURLAllowlisted("http://www.google.com/foo/"));
  EXPECT_FALSE(IsURLAllowlisted("http://accounts.google.com/bar/"));
  EXPECT_TRUE(IsURLAllowlisted("http://mail.google.com/moose/"));
  EXPECT_FALSE(IsURLAllowlisted("http://www.google.co.uk/blurp/"));

  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::ALLOW);

  EXPECT_FALSE(IsURLAllowlisted("http://www.google.com/foo/"));
  EXPECT_FALSE(IsURLAllowlisted("http://accounts.google.com/bar/"));
  EXPECT_TRUE(IsURLAllowlisted("http://mail.google.com/moose/"));
  EXPECT_FALSE(IsURLAllowlisted("http://www.google.co.uk/blurp/"));
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

TEST_F(SupervisedUserURLFilterTest, AllowlistsPatterns) {
  std::vector<std::string> patterns1;
  patterns1.push_back("google.com");
  patterns1.push_back("example.com");

  std::vector<std::string> patterns2;
  patterns2.push_back("secure.com");
  patterns2.push_back("example.com");

  const std::string id1 = "ID1";
  const std::string id2 = "ID2";
  const std::u16string title1 = u"Title 1";
  const std::u16string title2 = u"Title 2";
  const std::vector<std::string> hostname_hashes;
  const GURL entry_point("https://entry.com");

  scoped_refptr<SupervisedUserSiteList> site_list1 = base::WrapRefCounted(
      new SupervisedUserSiteList(id1, title1, entry_point, base::FilePath(),
                                 patterns1, hostname_hashes));
  scoped_refptr<SupervisedUserSiteList> site_list2 = base::WrapRefCounted(
      new SupervisedUserSiteList(id2, title2, entry_point, base::FilePath(),
                                 patterns2, hostname_hashes));

  std::vector<scoped_refptr<SupervisedUserSiteList>> site_lists;
  site_lists.push_back(site_list1);
  site_lists.push_back(site_list2);

  filter_.SetFromSiteListsForTesting(site_lists);
  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);
  run_loop_.Run();

  std::map<std::string, std::u16string> expected_allowlists;
  expected_allowlists[id1] = title1;
  expected_allowlists[id2] = title2;

  std::map<std::string, std::u16string> actual_allowlists =
      filter_.GetMatchingAllowlistTitles(GURL("https://example.com"));
  ASSERT_EQ(expected_allowlists, actual_allowlists);

  expected_allowlists.erase(id2);

  actual_allowlists =
      filter_.GetMatchingAllowlistTitles(GURL("https://google.com"));
  ASSERT_EQ(expected_allowlists, actual_allowlists);
}

TEST_F(SupervisedUserURLFilterTest, AllowlistsHostnameHashes) {
  std::vector<std::string> patterns1;
  patterns1.push_back("google.com");
  patterns1.push_back("example.com");

  std::vector<std::string> patterns2;
  patterns2.push_back("secure.com");
  patterns2.push_back("example.com");

  std::vector<std::string> patterns3;

  std::vector<std::string> hostname_hashes1;
  std::vector<std::string> hostname_hashes2;
  std::vector<std::string> hostname_hashes3;
  // example.com
  hostname_hashes3.push_back("0caaf24ab1a0c33440c06afe99df986365b0781f");
  // secure.com
  hostname_hashes3.push_back("529597fa818be828ffc7b59763fb2b185f419fc5");

  const std::string id1 = "ID1";
  const std::string id2 = "ID2";
  const std::string id3 = "ID3";
  const std::u16string title1 = u"Title 1";
  const std::u16string title2 = u"Title 2";
  const std::u16string title3 = u"Title 3";
  const GURL entry_point("https://entry.com");

  scoped_refptr<SupervisedUserSiteList> site_list1 = base::WrapRefCounted(
      new SupervisedUserSiteList(id1, title1, entry_point, base::FilePath(),
                                 patterns1, hostname_hashes1));
  scoped_refptr<SupervisedUserSiteList> site_list2 = base::WrapRefCounted(
      new SupervisedUserSiteList(id2, title2, entry_point, base::FilePath(),
                                 patterns2, hostname_hashes2));
  scoped_refptr<SupervisedUserSiteList> site_list3 = base::WrapRefCounted(
      new SupervisedUserSiteList(id3, title3, entry_point, base::FilePath(),
                                 patterns3, hostname_hashes3));

  std::vector<scoped_refptr<SupervisedUserSiteList>> site_lists;
  site_lists.push_back(site_list1);
  site_lists.push_back(site_list2);
  site_lists.push_back(site_list3);

  filter_.SetFromSiteListsForTesting(site_lists);
  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);
  run_loop_.Run();

  std::map<std::string, std::u16string> expected_allowlists;
  expected_allowlists[id1] = title1;
  expected_allowlists[id2] = title2;
  expected_allowlists[id3] = title3;

  std::map<std::string, std::u16string> actual_allowlists =
      filter_.GetMatchingAllowlistTitles(GURL("http://example.com"));
  ASSERT_EQ(expected_allowlists, actual_allowlists);

  expected_allowlists.erase(id1);

  actual_allowlists =
      filter_.GetMatchingAllowlistTitles(GURL("https://secure.com"));
  ASSERT_EQ(expected_allowlists, actual_allowlists);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(SupervisedUserURLFilterTest, ChromeWebstoreDownloadsAreAlwaysAllowed) {
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

  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(crx_download_url1));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(crx_download_url2));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(crx_download_url3));

  // Set explicit host rules to block those website, and make sure the
  // update URLs still work.
  std::map<std::string, bool> hosts;
  hosts["clients2.google.com"] = false;
  hosts["clients2.googleusercontent.com"] = false;
  filter_.SetManualHosts(std::move(hosts));
  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::ALLOW);
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(crx_download_url1));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(crx_download_url2));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter_.GetFilteringBehaviorForURL(crx_download_url3));
}
#endif

TEST_F(SupervisedUserURLFilterTest, GoogleFamiliesAlwaysAllowed) {
  filter_.SetDefaultFilteringBehavior(SupervisedUserURLFilter::BLOCK);
  EXPECT_TRUE(IsURLAllowlisted("https://families.google.com/"));
  EXPECT_TRUE(IsURLAllowlisted("https://families.google.com"));
  EXPECT_TRUE(IsURLAllowlisted("https://families.google.com/something"));
  EXPECT_TRUE(IsURLAllowlisted("http://families.google.com/"));
  EXPECT_FALSE(IsURLAllowlisted("https://families.google.com:8080/"));
  EXPECT_FALSE(IsURLAllowlisted("https://subdomain.families.google.com/"));
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
