// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/browsing_data_counter_utils.h"

#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/counters/cache_counter.h"
#include "chrome/browser/browsing_data/counters/signin_data_counter.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/strings/string_split.h"
#include "chrome/browser/browsing_data/counters/hosted_apps_counter.h"
#endif

namespace browsing_data_counter_utils {

class BrowsingDataCounterUtilsTest : public testing::Test {
 public:
  BrowsingDataCounterUtilsTest() {}
  ~BrowsingDataCounterUtilsTest() override {}

  TestingProfile* GetProfile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(BrowsingDataCounterUtilsTest, CacheCounterResult) {
  // This test assumes that the strings are served exactly as defined,
  // i.e. that the locale is set to the default "en".
  ASSERT_EQ("en", TestingBrowserProcess::GetGlobal()->GetApplicationLocale());
  const int kBytesInAMegabyte = 1024 * 1024;

  // Test the output for various forms of CacheResults.
  const struct TestCase {
    int bytes;
    bool is_upper_limit;
    bool is_basic_tab;
    std::string expected_output;
  } kTestCases[] = {
      {42, false, false, "Less than 1 MB"},
      {42, false, true,
       "Frees up less than 1 MB. Some sites may load more slowly on your next "
       "visit."},
      {2.312 * kBytesInAMegabyte, false, false, "2.3 MB"},
      {2.312 * kBytesInAMegabyte, false, true,
       "Frees up 2.3 MB. Some sites may load more slowly on your next visit."},
      {2.312 * kBytesInAMegabyte, true, false, "Less than 2.3 MB"},
      {2.312 * kBytesInAMegabyte, true, true,
       "Frees up less than 2.3 MB. Some sites may load more slowly on your "
       "next visit."},
      {500.2 * kBytesInAMegabyte, false, false, "500 MB"},
      {500.2 * kBytesInAMegabyte, true, false, "Less than 500 MB"},
  };

  for (const TestCase& test_case : kTestCases) {
    CacheCounter counter(GetProfile());
    browsing_data::ClearBrowsingDataTab tab =
        test_case.is_basic_tab ? browsing_data::ClearBrowsingDataTab::BASIC
                               : browsing_data::ClearBrowsingDataTab::ADVANCED;
    counter.Init(GetProfile()->GetPrefs(), tab,
                 browsing_data::BrowsingDataCounter::Callback());
    CacheCounter::CacheResult result(&counter, test_case.bytes,
                                     test_case.is_upper_limit);
    SCOPED_TRACE(base::StringPrintf(
        "Test params: %d bytes, %d is_upper_limit, %d is_basic_tab.",
        test_case.bytes, test_case.is_upper_limit, test_case.is_basic_tab));

    base::string16 output =
        GetChromeCounterTextFromResult(&result, GetProfile());
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Tests the complex output of the hosted apps counter.
TEST_F(BrowsingDataCounterUtilsTest, HostedAppsCounterResult) {
  HostedAppsCounter counter(GetProfile());

  // This test assumes that the strings are served exactly as defined,
  // i.e. that the locale is set to the default "en".
  ASSERT_EQ("en", TestingBrowserProcess::GetGlobal()->GetApplicationLocale());

  // Test the output for various numbers of hosted apps.
  const struct TestCase {
    std::string apps_list;
    std::string expected_output;
  } kTestCases[] = {
      {"", "None"},
      {"App1", "1 app (App1)"},
      {"App1, App2", "2 apps (App1, App2)"},
      {"App1, App2, App3", "3 apps (App1, App2, and 1 more)"},
      {"App1, App2, App3, App4", "4 apps (App1, App2, and 2 more)"},
      {"App1, App2, App3, App4, App5", "5 apps (App1, App2, and 3 more)"},
  };

  for (const TestCase& test_case : kTestCases) {
    // Split the list of installed apps by commas.
    std::vector<std::string> apps =
        base::SplitString(test_case.apps_list, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    // The first two apps in the list are used as examples.
    std::vector<std::string> examples;
    examples.assign(apps.begin(),
                    apps.begin() + (apps.size() > 2 ? 2 : apps.size()));

    HostedAppsCounter::HostedAppsResult result(&counter, apps.size(), examples);

    base::string16 output =
        GetChromeCounterTextFromResult(&result, GetProfile());
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}
#endif

// Tests the output for "Passwords and other sign-in data" on the advanced tab.
TEST_F(BrowsingDataCounterUtilsTest, DeletePasswordsAndSigninData) {
  // This test assumes that the strings are served exactly as defined,
  // i.e. that the locale is set to the default "en".
  ASSERT_EQ("en", TestingBrowserProcess::GetGlobal()->GetApplicationLocale());

  auto password_store =
      base::MakeRefCounted<password_manager::TestPasswordStore>();

  // This counter does not really count anything; we just need a reference to
  // pass to the SigninDataResult ctor.
  browsing_data::SigninDataCounter counter(
      password_store, ProfileSyncServiceFactory::GetForProfile(GetProfile()),
      nullptr);

  const struct TestCase {
    int num_passwords;
    int num_webauthn_credentials;
    bool sync_enabled;
    std::vector<std::string> domain_examples;
    std::string expected_output;
  } kTestCases[] = {
      {0, 0, false, {}, "None"},
      {0, 0, true, {}, "None"},
      {1, 0, false, {"a.com"}, "1 password (for a.com)"},
      {1, 0, true, {"a.com"}, "1 password (for a.com, synced)"},
      {2, 0, false, {"a.com"}, "2 passwords (for a.com)"},
      {2, 0, false, {"a.com", "b.com"}, "2 passwords (for a.com, b.com)"},
      {2,
       0,
       true,
       {"a.com", "b.com"},
       "2 passwords (for a.com, b.com, synced)"},
      {0, 1, false, {}, "sign-in data for 1 account"},
      {0, 1, true, {}, "sign-in data for 1 account"},
      {0, 2, false, {}, "sign-in data for 2 accounts"},
      {0, 2, true, {}, "sign-in data for 2 accounts"},
      {1,
       2,
       false,
       {"a.de"},
       "1 password (for a.de); sign-in data for 2 accounts"},
      {2,
       1,
       false,
       {"a.de", "b.de"},
       "2 passwords (for a.de, b.de); sign-in data for 1 account"},
      {2,
       3,
       true,
       {"a.de", "b.de"},
       "2 passwords (for a.de, b.de, synced); sign-in data for 3 "
       "accounts"},
      {4,
       2,
       false,
       {"a.de", "b.de"},
       "4 passwords (for a.de, b.de, and 2 more); sign-in data for 2 "
       "accounts"},
      {8,
       0,
       true,
       {"a.de", "b.de", "c.de", "d.de", "e.de", "f.de", "g.de", "h.de"},
       "8 passwords (for a.de, b.de, and 6 more, synced)"}};
  for (const auto& test_case : kTestCases) {
    browsing_data::SigninDataCounter::SigninDataResult result(
        &counter, test_case.num_passwords, test_case.num_webauthn_credentials,
        test_case.sync_enabled, test_case.domain_examples);
    std::string output = base::UTF16ToASCII(
        GetChromeCounterTextFromResult(&result, GetProfile()));
    EXPECT_EQ(test_case.expected_output, output);
  }

  password_store->ShutdownOnUIThread();
}

}  // namespace browsing_data_counter_utils
