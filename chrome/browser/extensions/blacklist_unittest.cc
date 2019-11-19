// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blacklist.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/blacklist_state_fetcher.h"
#include "chrome/browser/extensions/fake_safe_browsing_database_manager.h"
#include "chrome/browser/extensions/test_blacklist.h"
#include "chrome/browser/extensions/test_blacklist_state_fetcher.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class BlacklistTest : public testing::Test {
 public:
  BlacklistTest()
      : test_prefs_(base::ThreadTaskRunnerHandle::Get()) {}

 protected:
  ExtensionPrefs* prefs() {
    return test_prefs_.prefs();
  }

  std::string AddExtension(const std::string& id) {
    return test_prefs_.AddExtension(id)->id();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  TestExtensionPrefs test_prefs_;
};

template<typename T>
void Assign(T *to, const T& from) {
  *to = from;
}

}  // namespace

TEST_F(BlacklistTest, OnlyIncludesRequestedIDs) {
  std::string a = AddExtension("a");
  std::string b = AddExtension("b");
  std::string c = AddExtension("c");

  Blacklist blacklist(prefs());
  TestBlacklist tester(&blacklist);
  tester.SetBlacklistState(a, BLACKLISTED_MALWARE, false);
  tester.SetBlacklistState(b, BLACKLISTED_MALWARE, false);

  EXPECT_EQ(BLACKLISTED_MALWARE, tester.GetBlacklistState(a));
  EXPECT_EQ(BLACKLISTED_MALWARE, tester.GetBlacklistState(b));
  EXPECT_EQ(NOT_BLACKLISTED, tester.GetBlacklistState(c));

  std::set<std::string> blacklisted_ids;
  blacklist.GetMalwareIDs(
      {a, c}, base::Bind(&Assign<std::set<std::string>>, &blacklisted_ids));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ((std::set<std::string>{a}), blacklisted_ids);
}

TEST_F(BlacklistTest, SafeBrowsing) {
  std::string a = AddExtension("a");

  Blacklist blacklist(prefs());
  TestBlacklist tester(&blacklist);
  tester.DisableSafeBrowsing();

  EXPECT_EQ(NOT_BLACKLISTED, tester.GetBlacklistState(a));

  tester.SetBlacklistState(a, BLACKLISTED_MALWARE, false);
  // The manager is still disabled at this point, so it won't be blacklisted.
  EXPECT_EQ(NOT_BLACKLISTED, tester.GetBlacklistState(a));

  tester.EnableSafeBrowsing();
  tester.NotifyUpdate();
  base::RunLoop().RunUntilIdle();
  // Now it should be.
  EXPECT_EQ(BLACKLISTED_MALWARE, tester.GetBlacklistState(a));

  tester.Clear(true);
  // Safe browsing blacklist empty, now enabled.
  EXPECT_EQ(NOT_BLACKLISTED, tester.GetBlacklistState(a));
}

// Test getting different blacklist states from Blacklist.
TEST_F(BlacklistTest, GetBlacklistStates) {
  Blacklist blacklist(prefs());
  TestBlacklist tester(&blacklist);

  std::string a = AddExtension("a");
  std::string b = AddExtension("b");
  std::string c = AddExtension("c");
  std::string d = AddExtension("d");
  std::string e = AddExtension("e");

  tester.SetBlacklistState(a, BLACKLISTED_MALWARE, false);
  tester.SetBlacklistState(b, BLACKLISTED_SECURITY_VULNERABILITY, false);
  tester.SetBlacklistState(c, BLACKLISTED_CWS_POLICY_VIOLATION, false);
  tester.SetBlacklistState(d, BLACKLISTED_POTENTIALLY_UNWANTED, false);

  Blacklist::BlacklistStateMap states_abc;
  Blacklist::BlacklistStateMap states_bcd;
  blacklist.GetBlacklistedIDs(
      {a, b, c, e},
      base::Bind(&Assign<Blacklist::BlacklistStateMap>, &states_abc));
  blacklist.GetBlacklistedIDs(
      {b, c, d, e},
      base::Bind(&Assign<Blacklist::BlacklistStateMap>, &states_bcd));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(BLACKLISTED_MALWARE, states_abc[a]);
  EXPECT_EQ(BLACKLISTED_SECURITY_VULNERABILITY, states_abc[b]);
  EXPECT_EQ(BLACKLISTED_CWS_POLICY_VIOLATION, states_abc[c]);
  EXPECT_EQ(BLACKLISTED_SECURITY_VULNERABILITY, states_bcd[b]);
  EXPECT_EQ(BLACKLISTED_CWS_POLICY_VIOLATION, states_bcd[c]);
  EXPECT_EQ(BLACKLISTED_POTENTIALLY_UNWANTED, states_bcd[d]);
  EXPECT_EQ(0U, states_abc.count(e));
  EXPECT_EQ(0U, states_bcd.count(e));

  int old_request_count = tester.fetcher()->request_count();
  Blacklist::BlacklistStateMap states_ad;
  blacklist.GetBlacklistedIDs(
      {a, d, e}, base::Bind(&Assign<Blacklist::BlacklistStateMap>, &states_ad));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BLACKLISTED_MALWARE, states_ad[a]);
  EXPECT_EQ(BLACKLISTED_POTENTIALLY_UNWANTED, states_ad[d]);
  EXPECT_EQ(0U, states_ad.count(e));
  EXPECT_EQ(old_request_count, tester.fetcher()->request_count());
}

// Test both Blacklist and BlacklistStateFetcher by requesting the blacklist
// states, sending fake requests and parsing the responses.
TEST_F(BlacklistTest, FetchBlacklistStates) {
  Blacklist blacklist(prefs());
  scoped_refptr<FakeSafeBrowsingDatabaseManager> blacklist_db(
      new FakeSafeBrowsingDatabaseManager(true));
  Blacklist::ScopedDatabaseManagerForTest scoped_blacklist_db(blacklist_db);

  std::string a = AddExtension("a");
  std::string b = AddExtension("b");
  std::string c = AddExtension("c");

  blacklist_db->Enable();
  blacklist_db->SetUnsafe(a, b);

  // Prepare real fetcher.
  BlacklistStateFetcher* fetcher = new BlacklistStateFetcher();
  TestBlacklistStateFetcher fetcher_tester(fetcher);
  blacklist.SetBlacklistStateFetcherForTest(fetcher);

  fetcher_tester.SetBlacklistVerdict(
      a, ClientCRXListInfoResponse_Verdict_CWS_POLICY_VIOLATION);
  fetcher_tester.SetBlacklistVerdict(
      b, ClientCRXListInfoResponse_Verdict_POTENTIALLY_UNWANTED);

  Blacklist::BlacklistStateMap states;
  blacklist.GetBlacklistedIDs(
      {a, b, c}, base::Bind(&Assign<Blacklist::BlacklistStateMap>, &states));
  base::RunLoop().RunUntilIdle();

   // Two fetchers should be created.
  EXPECT_TRUE(fetcher_tester.HandleFetcher(a));
  EXPECT_TRUE(fetcher_tester.HandleFetcher(b));

  EXPECT_EQ(BLACKLISTED_CWS_POLICY_VIOLATION, states[a]);
  EXPECT_EQ(BLACKLISTED_POTENTIALLY_UNWANTED, states[b]);
  EXPECT_EQ(0U, states.count(c));

  Blacklist::BlacklistStateMap cached_states;

  blacklist.GetBlacklistedIDs(
      {a, b, c},
      base::Bind(&Assign<Blacklist::BlacklistStateMap>, &cached_states));
  base::RunLoop().RunUntilIdle();

  // No new fetchers.
  EXPECT_FALSE(fetcher_tester.HandleFetcher(c));
  EXPECT_EQ(BLACKLISTED_CWS_POLICY_VIOLATION, cached_states[a]);
  EXPECT_EQ(BLACKLISTED_POTENTIALLY_UNWANTED, cached_states[b]);
  EXPECT_EQ(0U, cached_states.count(c));
}

}  // namespace extensions
