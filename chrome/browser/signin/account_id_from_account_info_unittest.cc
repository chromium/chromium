// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_id_from_account_info.h"

#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests that AccountIdFromAccountInfo() passes along a canonicalized email to
// AccountId.
TEST(AccountIdFromAccountInfoTest,
     AccountIdFromAccountInfo_CanonicalizesRawEmail) {
  AccountInfo info;
  info.email = "test.email@gmail.com";
  info.gaia = GaiaId("test_id");

  EXPECT_EQ("testemail@gmail.com",
            AccountIdFromAccountInfo(info).GetUserEmail());
}
