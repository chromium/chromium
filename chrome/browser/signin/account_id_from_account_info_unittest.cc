// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_id_from_account_info.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests that AccountIdFromAccountInfo() passes along a canonicalized email to
// AccountId.
TEST(AccountIdFromAccountInfoTest,
     AccountIdFromAccountInfo_CanonicalizesRawEmail) {
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  AccountInfo info;
  info.email = "test.email@gmail.com";
  info.gaia = "test_id";

  EXPECT_EQ("testemail@gmail.com",
            AccountIdFromAccountInfo(info).GetUserEmail());
}
