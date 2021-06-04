// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_assistant_private/extension_access_token_fetcher.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ExtensionAccessTokenFetcherTest : public testing::Test {
 public:
  ExtensionAccessTokenFetcherTest() = default;
  ~ExtensionAccessTokenFetcherTest() override = default;

  void SetUp() override {
    identity_test_env_.MakePrimaryAccountAvailable("primary@example.com",
                                                   signin::ConsentLevel::kSync);
  }

  void TearDown() override {}

 protected:
  // The environment needs to be the first member to be initialized.
  base::test::TaskEnvironment task_environment_;

  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(ExtensionAccessTokenFetcherTest, SuccessfulAccessTokenFetch) {
  base::MockCallback<base::OnceCallback<void(bool, const std::string&)>>
      callback;
  EXPECT_CALL(callback, Run(true, "access_token"));

  ExtensionAccessTokenFetcher fetcher(identity_test_env_.identity_manager());
  fetcher.FetchAccessToken(callback.Get());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Max());
}

TEST_F(ExtensionAccessTokenFetcherTest, FailedAccessTokenFetch) {
  base::MockCallback<base::OnceCallback<void(bool, const std::string&)>>
      callback;
  EXPECT_CALL(callback, Run(false, ""));

  ExtensionAccessTokenFetcher fetcher(identity_test_env_.identity_manager());
  fetcher.FetchAccessToken(callback.Get());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromConnectionError(
          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
}

}  // namespace extensions
