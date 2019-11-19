// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blacklist_state_fetcher.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/test_blacklist_state_fetcher.h"
#include "chrome/common/safe_browsing/crx_info.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

void Assign(BlacklistState* to, BlacklistState from) {
  *to = from;
}

}  // namespace

class BlacklistStateFetcherTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BlacklistStateFetcherTest, RequestBlacklistState) {
  BlacklistStateFetcher fetcher;
  TestBlacklistStateFetcher tester(&fetcher);

  tester.SetBlacklistVerdict(
      "a", ClientCRXListInfoResponse_Verdict_SECURITY_VULNERABILITY);

  BlacklistState result;
  fetcher.Request("a", base::Bind(&Assign, &result));

  EXPECT_TRUE(tester.HandleFetcher("a"));
  EXPECT_EQ(BLACKLISTED_SECURITY_VULNERABILITY, result);
}

TEST_F(BlacklistStateFetcherTest, RequestMultipleBlacklistStates) {
  BlacklistStateFetcher fetcher;
  TestBlacklistStateFetcher tester(&fetcher);

  tester.SetBlacklistVerdict(
      "a", ClientCRXListInfoResponse_Verdict_NOT_IN_BLACKLIST);
  tester.SetBlacklistVerdict(
      "b", ClientCRXListInfoResponse_Verdict_MALWARE);
  tester.SetBlacklistVerdict(
      "c", ClientCRXListInfoResponse_Verdict_SECURITY_VULNERABILITY);
  tester.SetBlacklistVerdict(
      "d", ClientCRXListInfoResponse_Verdict_CWS_POLICY_VIOLATION);
  tester.SetBlacklistVerdict(
      "e", ClientCRXListInfoResponse_Verdict_POTENTIALLY_UNWANTED);

  BlacklistState result[9];
  fetcher.Request("a", base::Bind(&Assign, &result[0]));
  fetcher.Request("a", base::Bind(&Assign, &result[1]));
  fetcher.Request("b", base::Bind(&Assign, &result[2]));
  fetcher.Request("b", base::Bind(&Assign, &result[3]));
  fetcher.Request("c", base::Bind(&Assign, &result[4]));
  fetcher.Request("d", base::Bind(&Assign, &result[5]));
  fetcher.Request("e", base::Bind(&Assign, &result[6]));
  fetcher.Request("f", base::Bind(&Assign, &result[7]));
  fetcher.Request("f", base::Bind(&Assign, &result[8]));

  // 6 fetchers should be created. Sending responses in shuffled order.
  EXPECT_TRUE(tester.HandleFetcher("e"));
  EXPECT_TRUE(tester.HandleFetcher("c"));
  EXPECT_TRUE(tester.HandleFetcher("f"));
  EXPECT_TRUE(tester.HandleFetcher("b"));
  EXPECT_TRUE(tester.HandleFetcher("a"));
  EXPECT_TRUE(tester.HandleFetcher("d"));

  EXPECT_EQ(NOT_BLACKLISTED, result[0]);
  EXPECT_EQ(NOT_BLACKLISTED, result[1]);
  EXPECT_EQ(BLACKLISTED_MALWARE, result[2]);
  EXPECT_EQ(BLACKLISTED_MALWARE, result[3]);
  EXPECT_EQ(BLACKLISTED_SECURITY_VULNERABILITY, result[4]);
  EXPECT_EQ(BLACKLISTED_CWS_POLICY_VIOLATION, result[5]);
  EXPECT_EQ(BLACKLISTED_POTENTIALLY_UNWANTED, result[6]);
  EXPECT_EQ(NOT_BLACKLISTED, result[7]);
  EXPECT_EQ(NOT_BLACKLISTED, result[8]);
}

}  // namespace extensions
