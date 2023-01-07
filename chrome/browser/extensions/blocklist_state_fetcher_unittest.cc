// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blocklist_state_fetcher.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/test_blocklist_state_fetcher.h"
#include "chrome/common/safe_browsing/crx_info.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

void Assign(BlocklistState* to, BlocklistState from) {
  *to = from;
}

}  // namespace

class BlocklistStateFetcherTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BlocklistStateFetcherTest, RequestBlocklistState) {
  BlocklistStateFetcher fetcher;
  TestBlocklistStateFetcher tester(&fetcher);

  tester.SetBlocklistVerdict(
      "a", ClientCRXListInfoResponse_Verdict_SECURITY_VULNERABILITY);

  BlocklistState result;
  fetcher.Request("a", base::BindOnce(&Assign, &result));

  EXPECT_TRUE(tester.HandleFetcher("a"));
  EXPECT_EQ(BLOCKLISTED_SECURITY_VULNERABILITY, result);
}

TEST_F(BlocklistStateFetcherTest, RequestMultipleBlocklistStates) {
  BlocklistStateFetcher fetcher;
  TestBlocklistStateFetcher tester(&fetcher);

  tester.SetBlocklistVerdict(
      "a", ClientCRXListInfoResponse_Verdict_NOT_IN_BLOCKLIST);
  tester.SetBlocklistVerdict("b", ClientCRXListInfoResponse_Verdict_MALWARE);
  tester.SetBlocklistVerdict(
      "c", ClientCRXListInfoResponse_Verdict_SECURITY_VULNERABILITY);
  tester.SetBlocklistVerdict(
      "d", ClientCRXListInfoResponse_Verdict_CWS_POLICY_VIOLATION);
  tester.SetBlocklistVerdict(
      "e", ClientCRXListInfoResponse_Verdict_POTENTIALLY_UNWANTED);

  BlocklistState result[9];
  fetcher.Request("a", base::BindOnce(&Assign, &result[0]));
  fetcher.Request("a", base::BindOnce(&Assign, &result[1]));
  fetcher.Request("b", base::BindOnce(&Assign, &result[2]));
  fetcher.Request("b", base::BindOnce(&Assign, &result[3]));
  fetcher.Request("c", base::BindOnce(&Assign, &result[4]));
  fetcher.Request("d", base::BindOnce(&Assign, &result[5]));
  fetcher.Request("e", base::BindOnce(&Assign, &result[6]));
  fetcher.Request("f", base::BindOnce(&Assign, &result[7]));
  fetcher.Request("f", base::BindOnce(&Assign, &result[8]));

  // 6 fetchers should be created. Sending responses in shuffled order.
  EXPECT_TRUE(tester.HandleFetcher("e"));
  EXPECT_TRUE(tester.HandleFetcher("c"));
  EXPECT_TRUE(tester.HandleFetcher("f"));
  EXPECT_TRUE(tester.HandleFetcher("b"));
  EXPECT_TRUE(tester.HandleFetcher("a"));
  EXPECT_TRUE(tester.HandleFetcher("d"));

  EXPECT_EQ(NOT_BLOCKLISTED, result[0]);
  EXPECT_EQ(NOT_BLOCKLISTED, result[1]);
  EXPECT_EQ(BLOCKLISTED_MALWARE, result[2]);
  EXPECT_EQ(BLOCKLISTED_MALWARE, result[3]);
  EXPECT_EQ(BLOCKLISTED_SECURITY_VULNERABILITY, result[4]);
  EXPECT_EQ(BLOCKLISTED_CWS_POLICY_VIOLATION, result[5]);
  EXPECT_EQ(BLOCKLISTED_POTENTIALLY_UNWANTED, result[6]);
  EXPECT_EQ(NOT_BLOCKLISTED, result[7]);
  EXPECT_EQ(NOT_BLOCKLISTED, result[8]);
}

}  // namespace extensions
