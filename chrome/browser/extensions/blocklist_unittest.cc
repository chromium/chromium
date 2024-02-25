// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blocklist.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/blocklist_state_fetcher.h"
#include "chrome/browser/extensions/fake_safe_browsing_database_manager.h"
#include "chrome/browser/extensions/scoped_database_manager_for_test.h"
#include "chrome/browser/extensions/test_blocklist.h"
#include "chrome/browser/extensions/test_blocklist_state_fetcher.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class BlocklistTest : public testing::Test {
 public:
  BlocklistTest()
      : test_prefs_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

 protected:
  ExtensionId AddExtension(const ExtensionId& id) {
    return test_prefs_.AddExtension(id)->id();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  TestExtensionPrefs test_prefs_;
};

template <typename T>
void Assign(T* to, const T& from) {
  *to = from;
}

}  // namespace

TEST_F(BlocklistTest, OnlyIncludesRequestedIDs) {
  ExtensionId a = AddExtension("a");
  ExtensionId b = AddExtension("b");
  ExtensionId c = AddExtension("c");

  Blocklist blocklist;
  TestBlocklist tester(&blocklist);
  tester.SetBlocklistState(a, BLOCKLISTED_MALWARE, false);
  tester.SetBlocklistState(b, BLOCKLISTED_MALWARE, false);

  EXPECT_EQ(BLOCKLISTED_MALWARE, tester.GetBlocklistState(a));
  EXPECT_EQ(BLOCKLISTED_MALWARE, tester.GetBlocklistState(b));
  EXPECT_EQ(NOT_BLOCKLISTED, tester.GetBlocklistState(c));

  std::set<ExtensionId> blocklisted_ids;
  blocklist.GetMalwareIDs(
      {a, c}, base::BindOnce(&Assign<std::set<ExtensionId>>, &blocklisted_ids));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ((std::set<ExtensionId>{a}), blocklisted_ids);
}

TEST_F(BlocklistTest, SafeBrowsing) {
  ExtensionId a = AddExtension("a");

  Blocklist blocklist;
  TestBlocklist tester(&blocklist);
  tester.DisableSafeBrowsing();

  EXPECT_EQ(NOT_BLOCKLISTED, tester.GetBlocklistState(a));

  tester.SetBlocklistState(a, BLOCKLISTED_MALWARE, false);
  // The manager is still disabled at this point, so it won't be blocklisted.
  EXPECT_EQ(NOT_BLOCKLISTED, tester.GetBlocklistState(a));

  tester.EnableSafeBrowsing();
  tester.NotifyUpdate();
  base::RunLoop().RunUntilIdle();
  // Now it should be.
  EXPECT_EQ(BLOCKLISTED_MALWARE, tester.GetBlocklistState(a));

  tester.Clear(true);
  // Safe browsing blocklist empty, now enabled.
  EXPECT_EQ(NOT_BLOCKLISTED, tester.GetBlocklistState(a));
}

// Test getting different blocklist states from Blocklist.
TEST_F(BlocklistTest, GetBlocklistStates) {
  Blocklist blocklist;
  TestBlocklist tester(&blocklist);

  ExtensionId a = AddExtension("a");
  ExtensionId b = AddExtension("b");
  ExtensionId c = AddExtension("c");
  ExtensionId d = AddExtension("d");
  ExtensionId e = AddExtension("e");

  tester.SetBlocklistState(a, BLOCKLISTED_MALWARE, false);
  tester.SetBlocklistState(b, BLOCKLISTED_SECURITY_VULNERABILITY, false);
  tester.SetBlocklistState(c, BLOCKLISTED_CWS_POLICY_VIOLATION, false);
  tester.SetBlocklistState(d, BLOCKLISTED_POTENTIALLY_UNWANTED, false);

  Blocklist::BlocklistStateMap states_abc;
  Blocklist::BlocklistStateMap states_bcd;
  blocklist.GetBlocklistedIDs(
      {a, b, c, e},
      base::BindOnce(&Assign<Blocklist::BlocklistStateMap>, &states_abc));
  blocklist.GetBlocklistedIDs(
      {b, c, d, e},
      base::BindOnce(&Assign<Blocklist::BlocklistStateMap>, &states_bcd));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(BLOCKLISTED_MALWARE, states_abc[a]);
  EXPECT_EQ(BLOCKLISTED_SECURITY_VULNERABILITY, states_abc[b]);
  EXPECT_EQ(BLOCKLISTED_CWS_POLICY_VIOLATION, states_abc[c]);
  EXPECT_EQ(BLOCKLISTED_SECURITY_VULNERABILITY, states_bcd[b]);
  EXPECT_EQ(BLOCKLISTED_CWS_POLICY_VIOLATION, states_bcd[c]);
  EXPECT_EQ(BLOCKLISTED_POTENTIALLY_UNWANTED, states_bcd[d]);
  EXPECT_EQ(0U, states_abc.count(e));
  EXPECT_EQ(0U, states_bcd.count(e));

  int old_request_count = tester.fetcher()->request_count();
  Blocklist::BlocklistStateMap states_ad;
  blocklist.GetBlocklistedIDs(
      {a, d, e},
      base::BindOnce(&Assign<Blocklist::BlocklistStateMap>, &states_ad));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BLOCKLISTED_MALWARE, states_ad[a]);
  EXPECT_EQ(BLOCKLISTED_POTENTIALLY_UNWANTED, states_ad[d]);
  EXPECT_EQ(0U, states_ad.count(e));
  EXPECT_EQ(old_request_count, tester.fetcher()->request_count());
}

// Test both Blocklist and BlocklistStateFetcher by requesting the blocklist
// states, sending fake requests and parsing the responses.
TEST_F(BlocklistTest, FetchBlocklistStates) {
  Blocklist blocklist;
  scoped_refptr<FakeSafeBrowsingDatabaseManager> blocklist_db(
      new FakeSafeBrowsingDatabaseManager(true));
  ScopedDatabaseManagerForTest scoped_blocklist_db(blocklist_db);

  ExtensionId a = AddExtension("a");
  ExtensionId b = AddExtension("b");
  ExtensionId c = AddExtension("c");

  blocklist_db->Enable();
  blocklist_db->SetUnsafe(a, b);

  // Prepare real fetcher.
  BlocklistStateFetcher* fetcher = new BlocklistStateFetcher();
  TestBlocklistStateFetcher fetcher_tester(fetcher);
  blocklist.SetBlocklistStateFetcherForTest(fetcher);

  fetcher_tester.SetBlocklistVerdict(
      a, ClientCRXListInfoResponse_Verdict_CWS_POLICY_VIOLATION);
  fetcher_tester.SetBlocklistVerdict(
      b, ClientCRXListInfoResponse_Verdict_POTENTIALLY_UNWANTED);

  Blocklist::BlocklistStateMap states;
  blocklist.GetBlocklistedIDs(
      {a, b, c},
      base::BindOnce(&Assign<Blocklist::BlocklistStateMap>, &states));
  base::RunLoop().RunUntilIdle();

  // Two fetchers should be created.
  EXPECT_TRUE(fetcher_tester.HandleFetcher(a));
  EXPECT_TRUE(fetcher_tester.HandleFetcher(b));

  EXPECT_EQ(BLOCKLISTED_CWS_POLICY_VIOLATION, states[a]);
  EXPECT_EQ(BLOCKLISTED_POTENTIALLY_UNWANTED, states[b]);
  EXPECT_EQ(0U, states.count(c));

  Blocklist::BlocklistStateMap cached_states;

  blocklist.GetBlocklistedIDs(
      {a, b, c},
      base::BindOnce(&Assign<Blocklist::BlocklistStateMap>, &cached_states));
  base::RunLoop().RunUntilIdle();

  // No new fetchers.
  EXPECT_FALSE(fetcher_tester.HandleFetcher(c));
  EXPECT_EQ(BLOCKLISTED_CWS_POLICY_VIOLATION, cached_states[a]);
  EXPECT_EQ(BLOCKLISTED_POTENTIALLY_UNWANTED, cached_states[b]);
  EXPECT_EQ(0U, cached_states.count(c));
}

}  // namespace extensions
