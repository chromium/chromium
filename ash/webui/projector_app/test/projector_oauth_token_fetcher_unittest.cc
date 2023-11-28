// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_oauth_token_fetcher.h"

#include "ash/webui/projector_app/test/mock_app_client.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestUserEmail[] = "testuser1@gmail.com";
const char kTestUser2Email[] = "testuser2@gmail.com";

const base::TimeDelta kExpiryTimeFromNow = base::Minutes(10);

using OnOAuthTokenFetchFuture =
    base::test::TestFuture<const std::string&,
                           GoogleServiceAuthError,
                           const signin::AccessTokenInfo&>;
}  // namespace

namespace ash {

class ProjectorOAuthTokenFetcherTest : public testing::Test {
 public:
  ProjectorOAuthTokenFetcherTest() = default;
  ProjectorOAuthTokenFetcherTest(const ProjectorOAuthTokenFetcherTest&) =
      delete;
  ProjectorOAuthTokenFetcherTest& operator=(
      const ProjectorOAuthTokenFetcherTest&) = delete;
  ~ProjectorOAuthTokenFetcherTest() override = default;

  MockAppClient& mock_app_client() { return mock_app_client_; }

  ProjectorOAuthTokenFetcher& fetcher() { return access_token_fetcher_; }

  void VerifyOAuthTokenFetchResult(
      OnOAuthTokenFetchFuture& future,
      const std::optional<std::string>& email = std::nullopt) {
    const auto& expected_email = email ? email : kTestUserEmail;
    EXPECT_EQ(expected_email, future.Get<0>());
    EXPECT_EQ(GoogleServiceAuthError::State::NONE, future.Get<1>().state());
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockAppClient mock_app_client_;
  ProjectorOAuthTokenFetcher access_token_fetcher_;
};

TEST_F(ProjectorOAuthTokenFetcherTest, GetAccessTokenFirstRequest) {
  OnOAuthTokenFetchFuture future;
  fetcher().GetAccessTokenFor(kTestUserEmail, future.GetCallback());

  EXPECT_TRUE(fetcher().HasPendingRequestForTest(kTestUserEmail));

  mock_app_client().GrantOAuthTokenFor(
      kTestUserEmail,
      /* expiry_time = */ base::Time::Now() + kExpiryTimeFromNow);
  VerifyOAuthTokenFetchResult(future);

  EXPECT_TRUE(fetcher().HasCachedTokenForTest(kTestUserEmail));
}

TEST_F(ProjectorOAuthTokenFetcherTest, GetAccessTokenRepeatedRequest) {
  OnOAuthTokenFetchFuture future1;
  fetcher().GetAccessTokenFor(kTestUserEmail, future1.GetCallback());

  OnOAuthTokenFetchFuture future2;
  fetcher().GetAccessTokenFor(kTestUserEmail, future2.GetCallback());

  EXPECT_TRUE(fetcher().HasPendingRequestForTest(kTestUserEmail));

  mock_app_client().GrantOAuthTokenFor(
      kTestUserEmail,
      /* expiry_time = */ base::Time::Now() + kExpiryTimeFromNow);

  // Both requests should be granted.
  VerifyOAuthTokenFetchResult(future1);
  VerifyOAuthTokenFetchResult(future2);

  EXPECT_TRUE(fetcher().HasCachedTokenForTest(kTestUserEmail));
}

TEST_F(ProjectorOAuthTokenFetcherTest, AlmostExpiredToken) {
  OnOAuthTokenFetchFuture future1;
  fetcher().GetAccessTokenFor(kTestUserEmail, future1.GetCallback());

  mock_app_client().GrantOAuthTokenFor(kTestUserEmail,
                                       /* expiry_time = */
                                       base::Time::Now());
  VerifyOAuthTokenFetchResult(future1);

  OnOAuthTokenFetchFuture future2;
  fetcher().GetAccessTokenFor(kTestUserEmail, future2.GetCallback());

  EXPECT_FALSE(fetcher().HasCachedTokenForTest(kTestUserEmail));

  mock_app_client().GrantOAuthTokenFor(kTestUserEmail,
                                       /* expiry_time = */
                                       base::Time::Now() + kExpiryTimeFromNow);
  VerifyOAuthTokenFetchResult(future2);

  EXPECT_TRUE(fetcher().HasCachedTokenForTest(kTestUserEmail));
}

TEST_F(ProjectorOAuthTokenFetcherTest, MultipleAccountsRequesting) {
  mock_app_client().AddSecondaryAccount(kTestUser2Email);

  OnOAuthTokenFetchFuture future1;
  fetcher().GetAccessTokenFor(kTestUserEmail, future1.GetCallback());

  OnOAuthTokenFetchFuture future2;
  fetcher().GetAccessTokenFor(kTestUser2Email, future2.GetCallback());

  EXPECT_TRUE(fetcher().HasPendingRequestForTest(kTestUserEmail));
  EXPECT_TRUE(fetcher().HasPendingRequestForTest(kTestUser2Email));

  mock_app_client().GrantOAuthTokenFor(kTestUserEmail,
                                       /* expiry_time = */
                                       base::Time::Now() + kExpiryTimeFromNow);
  mock_app_client().GrantOAuthTokenFor(kTestUser2Email,
                                       /* expiry_time = */
                                       base::Time::Now() + kExpiryTimeFromNow);

  VerifyOAuthTokenFetchResult(future1);
  VerifyOAuthTokenFetchResult(future2, kTestUser2Email);

  // Now let's check that the tokens are present for both accounts.
  EXPECT_TRUE(fetcher().HasCachedTokenForTest(kTestUserEmail));
  EXPECT_TRUE(fetcher().HasCachedTokenForTest(kTestUser2Email));
}

TEST_F(ProjectorOAuthTokenFetcherTest, ValidCachedToken) {
  OnOAuthTokenFetchFuture future1;
  fetcher().GetAccessTokenFor(kTestUserEmail, future1.GetCallback());

  mock_app_client().GrantOAuthTokenFor(kTestUserEmail,
                                       /* expiry_time = */
                                       base::Time::Now() + kExpiryTimeFromNow);
  VerifyOAuthTokenFetchResult(future1);

  EXPECT_TRUE(fetcher().HasCachedTokenForTest(kTestUserEmail));

  OnOAuthTokenFetchFuture future2;
  fetcher().GetAccessTokenFor(kTestUserEmail, future2.GetCallback());

  // A valid token for `kTestUserEmail` is cached in ProjectorOAuthTokenFetcher.
  // Therefore, the request should be granted immediately and the callback
  // executed with the results without the need to go through
  // signin::IdentityManager.
  VerifyOAuthTokenFetchResult(future2);

  EXPECT_TRUE(fetcher().HasCachedTokenForTest(kTestUserEmail));
}

TEST_F(ProjectorOAuthTokenFetcherTest, InvalidateToken) {
  OnOAuthTokenFetchFuture future;
  fetcher().GetAccessTokenFor(kTestUserEmail, future.GetCallback());
  mock_app_client().GrantOAuthTokenFor(
      kTestUserEmail,
      /* expiry_time = */ base::Time::Now() + kExpiryTimeFromNow);
  VerifyOAuthTokenFetchResult(future);

  EXPECT_TRUE(fetcher().HasCachedTokenForTest(kTestUserEmail));

  fetcher().InvalidateToken(future.Get<2>().token);
  EXPECT_FALSE(fetcher().HasCachedTokenForTest(kTestUserEmail));
}

}  // namespace ash
