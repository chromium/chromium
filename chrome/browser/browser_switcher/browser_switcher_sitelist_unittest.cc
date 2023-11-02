// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/ieem_sitelist_parser.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browser_switcher {

namespace {

class TestBrowserSwitcherPrefs : public BrowserSwitcherPrefs {
 public:
  explicit TestBrowserSwitcherPrefs(PrefService* prefs)
      : BrowserSwitcherPrefs(prefs, nullptr) {}
};

base::Value StringArrayToValue(const std::vector<const char*>& strings) {
  base::Value::List list;
  for (const auto* string : strings)
    list.Append(string);
  return base::Value(std::move(list));
}

void CheckRuleSetSize(size_t expected_sitelist_size,
                      size_t expected_greylist_size,
                      const RuleSet* ruleset) {
  ASSERT_EQ(expected_sitelist_size, ruleset->sitelist.size());
  ASSERT_EQ(expected_greylist_size, ruleset->greylist.size());
}

}  // namespace

class BrowserSwitcherSitelistTest : public testing::TestWithParam<ParsingMode> {
 public:
  BrowserSwitcherSitelistTest() { parsing_mode_ = GetParam(); }

  void Initialize(const std::vector<const char*>& url_list,
                  const std::vector<const char*>& url_greylist,
                  bool enabled = true) {
    BrowserSwitcherPrefs::RegisterProfilePrefs(prefs_backend_.registry());
    prefs_backend_.SetManagedPref(prefs::kEnabled, base::Value(enabled));
    prefs_backend_.SetManagedPref(prefs::kUrlList,
                                  StringArrayToValue(url_list));
    prefs_backend_.SetManagedPref(prefs::kUrlGreylist,
                                  StringArrayToValue(url_greylist));
    prefs_backend_.SetManagedPref(prefs::kParsingMode,
                                  base::Value(static_cast<int>(parsing_mode_)));
    prefs_ = std::make_unique<TestBrowserSwitcherPrefs>(&prefs_backend_);
    sitelist_ = std::make_unique<BrowserSwitcherSitelistImpl>(prefs_.get());
  }

  bool ShouldSwitch(const GURL& url) { return sitelist_->ShouldSwitch(url); }
  Decision GetDecision(const GURL& url) { return sitelist_->GetDecision(url); }

  sync_preferences::TestingPrefServiceSyncable* prefs_backend() {
    return &prefs_backend_;
  }
  BrowserSwitcherPrefs* prefs() { return prefs_.get(); }
  BrowserSwitcherSitelist* sitelist() { return sitelist_.get(); }
  ParsingMode parsing_mode() { return prefs_->GetParsingMode(); }

  void CheckCanonicalizedRule(std::string expected, std::string raw_rule) {
    std::unique_ptr<Rule> rule = CanonicalizeRule(raw_rule, parsing_mode());
    if (!expected.empty()) {
      ASSERT_NE(nullptr, rule) << raw_rule;
      EXPECT_EQ(expected, rule->ToString()) << raw_rule;
    } else {
      EXPECT_EQ(nullptr, rule) << raw_rule;
    }
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  ParsingMode parsing_mode_;
  sync_preferences::TestingPrefServiceSyncable prefs_backend_;
  std::unique_ptr<BrowserSwitcherPrefs> prefs_;
  std::unique_ptr<BrowserSwitcherSitelist> sitelist_;
};

TEST_P(BrowserSwitcherSitelistTest, CanonicalizeRule) {
  Initialize({}, {});
  if (parsing_mode() == ParsingMode::kDefault) {
    CheckCanonicalizedRule("example.com", "Example.Com");
    CheckCanonicalizedRule("AAA", "ftp://XXX/AAA");
    CheckCanonicalizedRule("//example.com/", "Example.Com/");
    CheckCanonicalizedRule("!//example.com/Abc", "!Example.Com/Abc");
    CheckCanonicalizedRule("/Example.Com", "/Example.Com");
    CheckCanonicalizedRule("//example.com/", "//Example.Com");
    CheckCanonicalizedRule("!//example.com/", "!//Example.Com");
    CheckCanonicalizedRule("http://example.com/", "HTTP://EXAMPLE.COM");
    CheckCanonicalizedRule("http://example.com/ABC", "HTTP://EXAMPLE.COM/ABC");
    CheckCanonicalizedRule("//User@example.com:8080/Test",
                           "User@Example.Com:8080/Test");
    CheckCanonicalizedRule("10.122.34.99:8080", "10.122.34.99:8080");
    // IPv6.
    CheckCanonicalizedRule("[2001:db8:3333:4444:5555:6666:7777:8888]:8080",
                           "[2001:db8:3333:4444:5555:6666:7777:8888]:8080");
  } else {
    CheckCanonicalizedRule("*://example.com/", "Example.Com");
    CheckCanonicalizedRule("*://example.com/", "Example.Com/");
    CheckCanonicalizedRule("!*://example.com/abc", "!Example.Com/Abc");
    CheckCanonicalizedRule("*://example.com/", "/Example.Com");
#if BUILDFLAG(IS_WIN)
    CheckCanonicalizedRule("*://example.com/", "//Example.Com");
    CheckCanonicalizedRule("!*://example.com/", "!//Example.Com");
#else
    CheckCanonicalizedRule("file:///example.com", "//Example.Com");
    CheckCanonicalizedRule("!file:///example.com", "!//Example.Com");
#endif
    CheckCanonicalizedRule("http://example.com/", "HTTP://EXAMPLE.COM");
    CheckCanonicalizedRule("http://example.com/abc", "HTTP://EXAMPLE.COM/ABC");
    CheckCanonicalizedRule("*://example.com:8080/test",
                           "User@Example.Com:8080/Test");
    CheckCanonicalizedRule("http://example.com/path",
                           "http://example.com/path?withQuery#andFragment");
    CheckCanonicalizedRule("*://10.122.34.99:8080/", "10.122.34.99:8080");
    // IPv6.
    CheckCanonicalizedRule("*://[2001:db8:3333:4444:5555:6666:7777:8888]:8080/",
                           "[2001:db8:3333:4444:5555:6666:7777:8888]:8080");
  }
}

TEST_P(BrowserSwitcherSitelistTest, CanonicalizeRulesLikeMicrosoft) {
  Initialize({}, {});
  if (parsing_mode() != ParsingMode::kIESiteListMode)
    return;

  // Every weird input I could think of, tested in Edge.
  //
  // Microsoft doesn't recommend using any of these, as they "break parsing",
  // but it's nice to be bug-compatible as much as we can :-)
  CheckCanonicalizedRule("*://%2A//example.com/", "*://example.com/");
  CheckCanonicalizedRule("*://var/", "/var");
  CheckCanonicalizedRule("*://var/www", "/var/www");
  CheckCanonicalizedRule("*://xn--dp8h/", "🐸");
  CheckCanonicalizedRule("*://xn--3e0bs9hfvinn1a/", "대한민국");

  // A bunch of rules that won't parse in Edge.
  CheckCanonicalizedRule("", "");
  CheckCanonicalizedRule("", "/");
  CheckCanonicalizedRule("", "     ");
  CheckCanonicalizedRule("", "  \t \n \r   ");
  CheckCanonicalizedRule("", "data:text/plain,hi");
  CheckCanonicalizedRule("", "javascript:alert('hello')");
  CheckCanonicalizedRule("", "ヽ༼ຈل͜ຈ༽ﾉ");

  // These rules don't match Edge behavior, but most of them are edge-casey
  // enough that it's fine(?). Edge uses IE's URL parser for this, so it's
  // normal for behaviour to be slightly different.
  //
  // The value in Edge (on edge://compat) is included as a comment for
  // reference.
  CheckCanonicalizedRule("*",  // *://%2A/  (this is deliberate, to allow
                               //            wildcards)
                         "*");
  CheckCanonicalizedRule("*://example.com/",  // (invalid rule)
                         "://example.com");
  CheckCanonicalizedRule("",  // *://bar.com/
                         "mailto:foo@bar.com");
#if BUILDFLAG(IS_WIN)
  CheckCanonicalizedRule("file:///c:/src/",  // *://c/src
                         "C:/src/");
  CheckCanonicalizedRule("file:///c:/src",  // *://c/src
                         "C:\\src");
  CheckCanonicalizedRule("*://var/",  // file://var/
                         "//var");
  CheckCanonicalizedRule("*://var/www",  // file://var/www
                         "//var/www");
  CheckCanonicalizedRule("file:///c:/src",  // *://file//c:/src
                         "file://c:/src");
#else
  CheckCanonicalizedRule("file:///var",  // *://var
                         "//var");
  CheckCanonicalizedRule("file:///var/www",  // *://var/www
                         "//var/www");
  CheckCanonicalizedRule("",  // *://file//c:/src
                         "file://c:/src");
#endif
}

TEST_P(BrowserSwitcherSitelistTest, ShouldRedirectWildcard) {
  // A "*" by itself means everything matches.
  Initialize({"*"}, {});
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/")));
  EXPECT_TRUE(ShouldSwitch(GURL("https://example.com/foobar/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/foobar/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://google.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, ShouldRedirectHost) {
  // A string without slashes means compare the URL's host (case-insensitive).
  Initialize({"example.com"}, {});
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/")));
  EXPECT_TRUE(ShouldSwitch(GURL("https://example.com/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://subdomain.example.com/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/foobar/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com:8000/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://google.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://example.ca/")));

  // For backwards compatibility, this should also match, even if it's not the
  // same host.
  EXPECT_EQ(parsing_mode() != ParsingMode::kIESiteListMode,
            ShouldSwitch(GURL("https://notexample.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, ShouldRedirectHostNotLowerCase) {
  // Host is not in lowercase form, but we compare ignoring case.
  Initialize({"eXaMpLe.CoM"}, {});
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, ShouldRedirectWrongScheme) {
  Initialize({"example.com"}, {});
  // Scheme is not one of 'http', 'https' or 'file'.
  EXPECT_FALSE(ShouldSwitch(GURL("ftp://example.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, ShouldRedirectPrefix) {
  // A string with slashes means check if it's a prefix (case-sensitive).
  Initialize({"http://example.com/foobar"}, {});
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/foobar")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/foobar/subroute/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/foobar#fragment")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/foobar?query=param")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://example.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("https://example.com/foobar")));
  EXPECT_EQ(parsing_mode() == ParsingMode::kIESiteListMode,
            ShouldSwitch(GURL("HTTP://EXAMPLE.COM/FOOBAR")));
  EXPECT_EQ(parsing_mode() == ParsingMode::kIESiteListMode,
            ShouldSwitch(GURL("http://subdomain.example.com/foobar")));
  EXPECT_EQ(parsing_mode() == ParsingMode::kIESiteListMode,
            ShouldSwitch(GURL("http://subdomain.example.com:8000/foobar")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://google.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, ShouldRedirectMicrosoftExamples) {
  // Inspired by examples from:
  // https://docs.microsoft.com/en-us/internet-explorer/ie11-deploy-guide/enterprise-mode-schema-version-2-guidance#updated-schema-elements
  //
  // Rules are host + port + path. Test lots of permutations of host, port, and
  // path being present/absent, for each rule.
  Initialize(
      {
          // Just domain name.
          "example.com",
          // Domain name + port.
          "example.net:8080",
          // Domain name + path.
          "contoso.com/hello",
          // IPv4 + port + path.
          "10.122.34.99:8080/hello",
          // IPv6 + port + path.
          "[2001:db8:3333:4444:5555:6666:7777:8888]:8080/hello",
      },
      {});

  // example.com
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/")));
  EXPECT_TRUE(ShouldSwitch(GURL("https://example.com/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/hello")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com:8080/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com:8080/hello")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://subdomain.example.com/")));
  EXPECT_EQ(parsing_mode() == ParsingMode::kDefault,
            ShouldSwitch(GURL("http://example.com.evil.com/")));

  // example.net:8080
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.net:8080/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.net:8080/hello")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://subdomain.example.net:8080/hello")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://example.net/")));
  EXPECT_FALSE(ShouldSwitch(GURL("https://example.net/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://example.net/hello")));

  // contoso.com/hello
  EXPECT_TRUE(ShouldSwitch(GURL("http://contoso.com:8080/hello")));
  EXPECT_TRUE(ShouldSwitch(GURL("https://contoso.com:8080/hello")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://contoso.com/hello")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://contoso.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://contoso.com:8080/")));
  EXPECT_EQ(parsing_mode() == ParsingMode::kIESiteListMode,
            ShouldSwitch(GURL("http://subdomain.contoso.com:8080/hello")));

  // 10.122.34.99:8080/hello
  EXPECT_TRUE(ShouldSwitch(GURL("http://10.122.34.99:8080/hello")));
  EXPECT_TRUE(ShouldSwitch(GURL("https://10.122.34.99:8080/hello")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://10.122.34.99/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://10.122.34.99/hello")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://10.122.34.99:8080/")));

  // [2001:db8:3333:4444:5555:6666:7777:8888]:8080/hello
  EXPECT_TRUE(ShouldSwitch(
      GURL("http://[2001:db8:3333:4444:5555:6666:7777:8888]:8080/hello")));
  EXPECT_TRUE(ShouldSwitch(
      GURL("https://[2001:db8:3333:4444:5555:6666:7777:8888]:8080/hello")));
  EXPECT_FALSE(
      ShouldSwitch(GURL("http://[2001:db8:3333:4444:5555:6666:7777:8888]/")));
  EXPECT_FALSE(ShouldSwitch(
      GURL("http://[2001:db8:3333:4444:5555:6666:7777:8888]/hello")));
  EXPECT_FALSE(ShouldSwitch(
      GURL("http://[2001:db8:3333:4444:5555:6666:7777:8888]:8080/")));
}

TEST_P(BrowserSwitcherSitelistTest, ShouldRedirectInvertedMatch) {
  // The most specific (i.e., longest string) rule should have priority.
  Initialize({"!subdomain.example.com", "example.com",
              "subsubdomain.subdomain.example.com"},
             {});
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://subdomain.example.com/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://subsubdomain.subdomain.example.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, ShouldRedirectGreylist) {
  // The most specific (i.e., longest string) rule should have priority.
  Initialize({"example.com"}, {"http://example.com/login/"});
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://example.com/login/")));
}

TEST_P(BrowserSwitcherSitelistTest, ShouldRedirectGreylistWildcard) {
  Initialize({"*"}, {"*"});
  // If both are wildcards, prefer the greylist.
  EXPECT_FALSE(ShouldSwitch(GURL("http://example.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, ShouldMatchAnySchema) {
  // URLs formatted like these don't include a schema, so should match both HTTP
  // and HTTPS.
  Initialize({"example.com/", "reddit.com/r/funny"}, {});
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/something")));
  EXPECT_TRUE(ShouldSwitch(GURL("https://example.com/something")));
  EXPECT_TRUE(ShouldSwitch(GURL("file://example.com/foobar/")));
  EXPECT_EQ(parsing_mode() == ParsingMode::kIESiteListMode,
            ShouldSwitch(GURL("https://foo.example.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("mailto://example.com")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://bad.com/example.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://bad.com//example.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://bad.com/hackme.html?example.com")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://reddit.com/r/funny")));
  EXPECT_TRUE(ShouldSwitch(GURL("https://reddit.com/r/funny")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://reddit.com/r/pics")));
  EXPECT_FALSE(ShouldSwitch(GURL("https://reddit.com/r/pics")));
}

TEST_P(BrowserSwitcherSitelistTest, ShouldRedirectPort) {
  Initialize({"example.com/", "test.com:3000/", "lol.com:3000", "trololo.com"},
             {});
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com:2000/something")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://test.com:3000/something")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://lol.com:3000/something")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://trololo.com/something")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://trololo.com:3000/something")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://test.com:2000/something")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://test.com:2000/something:3000")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://test.com/something:3000")));
}

// crbug.com/1209124
TEST_P(BrowserSwitcherSitelistTest, ShouldRedirectHostnamePrefix) {
  // A hostname rule (no "/") can match at the beginning of the hostname, not
  // just at the end.
  Initialize({"10.", "subdomain"}, {});
  EXPECT_EQ(parsing_mode() == ParsingMode::kDefault,
            ShouldSwitch(GURL("http://10.0.0.1/")));
  EXPECT_EQ(parsing_mode() == ParsingMode::kDefault,
            ShouldSwitch(GURL("http://subdomain.example.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, InvertedRuleInGreylist) {
  // A greylist can't contain inverted rules.
  Initialize({"example.com"},
             {"!foo.bar.example.com", "bar.example.com", "!qux.example.com"});
  EXPECT_FALSE(ShouldSwitch(GURL("http://foo.bar.example.com/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://qux.example.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, ShouldPickUpPrefChanges) {
  Initialize({}, {});
  prefs_backend()->SetManagedPref(prefs::kUrlList,
                                  StringArrayToValue({"example.com"}));
  prefs_backend()->SetManagedPref(prefs::kUrlGreylist,
                                  StringArrayToValue({"foo.example.com"}));
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://bar.example.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://foo.example.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://google.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, ShouldIgnoreNonManagedPrefs) {
  Initialize({}, {});

  prefs_backend()->Set(prefs::kUrlList, StringArrayToValue({"example.com"}));
  EXPECT_FALSE(ShouldSwitch(GURL("http://example.com/")));

  prefs_backend()->SetManagedPref(prefs::kUrlList,
                                  StringArrayToValue({"example.com"}));
  prefs_backend()->Set(prefs::kUrlGreylist,
                       StringArrayToValue({"morespecific.example.com"}));
  EXPECT_TRUE(ShouldSwitch(GURL("http://morespecific.example.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, SetIeemSitelist) {
  Initialize({}, {});
  // XXX: Also do some tests that use ieem.rules.greylist.
  sitelist()->SetIeemSitelist(RawRuleSet({"example.com"}, {}));
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://bar.example.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://google.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, SetExternalSitelist) {
  Initialize({}, {});
  sitelist()->SetExternalSitelist(RawRuleSet({"example.com"}, {}));
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://bar.example.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://google.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, SetExternalGreylist) {
  Initialize({"example.com"}, {});
  sitelist()->SetExternalGreylist(RawRuleSet({}, {"foo.example.com"}));
  EXPECT_TRUE(ShouldSwitch(GURL("http://example.com/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://bar.example.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://foo.example.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://google.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, All3Sources) {
  Initialize({"google.com"}, {"mail.google.com"});
  sitelist()->SetIeemSitelist(RawRuleSet({"!maps.google.com"}, {}));
  sitelist()->SetExternalSitelist(RawRuleSet({"yahoo.com"}, {}));
  EXPECT_TRUE(ShouldSwitch(GURL("http://google.com/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://drive.google.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://mail.google.com/")));
  EXPECT_FALSE(ShouldSwitch(GURL("http://maps.google.com/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://yahoo.com/")));
  EXPECT_TRUE(ShouldSwitch(GURL("http://news.yahoo.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, ReCanonicalizeWhenParsingModeChanges) {
  // Configure all sitelists/greylists at the same time.
  Initialize({"google.com"}, {"mail.google.com"});
  sitelist()->SetIeemSitelist(RawRuleSet({"example.com"}, {"grey.com"}));
  sitelist()->SetExternalSitelist(RawRuleSet({"yahoo.com"}, {}));
  sitelist()->SetExternalGreylist(RawRuleSet({}, {"duckduckgo.com"}));

  // Check initial state.
  CheckRuleSetSize(1u, 1u, &prefs()->GetRules());
  CheckRuleSetSize(1u, 1u, sitelist()->GetIeemSitelist());
  CheckRuleSetSize(1u, 0u, sitelist()->GetExternalSitelist());
  CheckRuleSetSize(0u, 1u, sitelist()->GetExternalGreylist());
  if (parsing_mode() == ParsingMode::kDefault) {
    EXPECT_EQ("google.com", prefs()->GetRules().sitelist[0]->ToString());
    EXPECT_EQ("mail.google.com", prefs()->GetRules().greylist[0]->ToString());
    EXPECT_EQ("example.com",
              sitelist()->GetIeemSitelist()->sitelist[0]->ToString());
    EXPECT_EQ("grey.com",
              sitelist()->GetIeemSitelist()->greylist[0]->ToString());
    EXPECT_EQ("yahoo.com",
              sitelist()->GetExternalSitelist()->sitelist[0]->ToString());
    EXPECT_EQ("duckduckgo.com",
              sitelist()->GetExternalGreylist()->greylist[0]->ToString());
  } else {
    EXPECT_EQ("*://google.com/", prefs()->GetRules().sitelist[0]->ToString());
    EXPECT_EQ("*://mail.google.com/",
              prefs()->GetRules().greylist[0]->ToString());
    EXPECT_EQ("*://example.com/",
              sitelist()->GetIeemSitelist()->sitelist[0]->ToString());
    EXPECT_EQ("*://grey.com/",
              sitelist()->GetIeemSitelist()->greylist[0]->ToString());
    EXPECT_EQ("*://yahoo.com/",
              sitelist()->GetExternalSitelist()->sitelist[0]->ToString());
    EXPECT_EQ("*://duckduckgo.com/",
              sitelist()->GetExternalGreylist()->greylist[0]->ToString());
  }

  // Change parsing mode.
  ParsingMode new_parsing_mode = parsing_mode() == ParsingMode::kDefault
                                     ? ParsingMode::kIESiteListMode
                                     : ParsingMode::kDefault;
  prefs_backend()->SetManagedPref(
      prefs::kParsingMode, base::Value(static_cast<int>(new_parsing_mode)));
  prefs()->OnPolicyUpdated(policy::PolicyNamespace(), policy::PolicyMap(),
                           policy::PolicyMap());
  base::RunLoop().RunUntilIdle();

  // Check that canonicalization changed.
  CheckRuleSetSize(1u, 1u, &prefs()->GetRules());
  CheckRuleSetSize(1u, 1u, sitelist()->GetIeemSitelist());
  CheckRuleSetSize(1u, 0u, sitelist()->GetExternalSitelist());
  CheckRuleSetSize(0u, 1u, sitelist()->GetExternalGreylist());
  if (new_parsing_mode == ParsingMode::kDefault) {
    EXPECT_EQ("google.com", prefs()->GetRules().sitelist[0]->ToString());
    EXPECT_EQ("mail.google.com", prefs()->GetRules().greylist[0]->ToString());
    EXPECT_EQ("example.com",
              sitelist()->GetIeemSitelist()->sitelist[0]->ToString());
    EXPECT_EQ("grey.com",
              sitelist()->GetIeemSitelist()->greylist[0]->ToString());
    EXPECT_EQ("yahoo.com",
              sitelist()->GetExternalSitelist()->sitelist[0]->ToString());
    EXPECT_EQ("duckduckgo.com",
              sitelist()->GetExternalGreylist()->greylist[0]->ToString());
  } else {
    EXPECT_EQ("*://google.com/", prefs()->GetRules().sitelist[0]->ToString());
    EXPECT_EQ("*://mail.google.com/",
              prefs()->GetRules().greylist[0]->ToString());
    EXPECT_EQ("*://example.com/",
              sitelist()->GetIeemSitelist()->sitelist[0]->ToString());
    EXPECT_EQ("*://grey.com/",
              sitelist()->GetIeemSitelist()->greylist[0]->ToString());
    EXPECT_EQ("*://yahoo.com/",
              sitelist()->GetExternalSitelist()->sitelist[0]->ToString());
    EXPECT_EQ("*://duckduckgo.com/",
              sitelist()->GetExternalGreylist()->greylist[0]->ToString());
  }
}

TEST_P(BrowserSwitcherSitelistTest, BrowserSwitcherDisabled) {
  Initialize({"example.com"}, {}, false);
  EXPECT_FALSE(ShouldSwitch(GURL("http://example.com/")));
  EXPECT_EQ(Decision(kStay, kDisabled, nullptr),
            GetDecision(GURL("http://example.com/")));
}

TEST_P(BrowserSwitcherSitelistTest, CheckReason) {
  Initialize({"foo.invalid.com", "!example.com"},
             {"foo.invalid.com/foobar", "invalid.com"});
  EXPECT_EQ(Decision(kStay, kProtocol, nullptr),
            GetDecision(GURL("ftp://example.com/")));
  EXPECT_EQ(Decision(kStay, kDefault, nullptr),
            GetDecision(GURL("http://google.com/")));
  EXPECT_EQ(Decision(kStay, kDefault, nullptr),
            GetDecision(GURL("http://bar.invalid.com/")));
  std::unique_ptr<Rule> rule = CanonicalizeRule("!example.com", parsing_mode());
  EXPECT_EQ(Decision(kStay, kSitelist, rule.get()),
            GetDecision(GURL("http://example.com/")));
  rule = CanonicalizeRule("foo.invalid.com", parsing_mode());
  EXPECT_EQ(Decision(kGo, kSitelist, rule.get()),
            GetDecision(GURL("http://foo.invalid.com/")));
  rule = CanonicalizeRule("foo.invalid.com/foobar", parsing_mode());
  EXPECT_EQ(Decision(kStay, kGreylist, rule.get()),
            GetDecision(GURL("http://foo.invalid.com/foobar")));
}

INSTANTIATE_TEST_SUITE_P(ParsingMode,
                         BrowserSwitcherSitelistTest,
                         testing::Values(ParsingMode::kDefault,
                                         ParsingMode::kIESiteListMode,
                                         // 999 should behave like kDefault
                                         static_cast<ParsingMode>(999)));

}  // namespace browser_switcher
