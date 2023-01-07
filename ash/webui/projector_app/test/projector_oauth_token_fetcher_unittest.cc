// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_oauth_token_fetcher.h"

#include "ash/webui/projector_app/test/mock_app_client.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestUserEmail[] = "testuser1@gmail.com";
const char kTestUser2Email[] = "testuser2@gmail.com";

const base::TimeDelta kExpiryTimeFromNow = base::Minutes(10);
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

  void OnOAuthTokenFetchCallback(base::RepeatingClosure quit_closure,
                                 const std::string& expected_email,
                                 const std::string& email,
                                 GoogleServiceAuthError error,
                                 const signin::AccessTokenInfo& info) {
    EXPECT_EQ(expected_email, email);
    EXPECT_EQ(error.state(), GoogleServiceAuthError::State::NONE);
    quit_closure.Run();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockAppClient mock_app_client_;
  ProjectorOAuthTokenFetcher access_token_fetcher_;
};

TEST_F(ProjectorOAuthTokenFetcherTest, GetAccessTokenFirstRequest) {
  base::RunLoop run_loop;

  fetcher().GetAccessTokenFor(
      kTestUserEmail,
      base::BindOnce(&ProjectorOAuthTokenFetcherTest::OnOAuthTokenFetchCallback,
                     base::Unretained(this), run_loop.QuitClosure(),
                     kTestUserEmail));

  EXPECT_TRUE(fetcher().HasPendingRequestForTest(kTestUserEmail));

  mock_app_client().GrantOAuthTokenFor(
      kTestUserEmail,
      /* expiry_time = */ base::Time::Now() + kExpiryTimeFromNow);
  run_loop.Run();

  EXPECT_TRUE(fetcher().HasCachedTokenForTest(kTestUserEmail));
}

TEST_F(ProjectorOAuthTokenFetcherTest, GetAccessTokenRepeatedRequest) {
  base::RunLoop run_loop1;
  fetcher().GetAccessTokenFor(
      kTestUserEmail,
      base::BindOnce(&ProjectorOAuthTokenFetcherTest::OnOAuthTokenFetchCallback,
                     base::Unretained(this), run_loop1.QuitClosure(),
                     kTestUserEmail));

  base::RunLoop run_loop2;
  fetcher().GetAccessTokenFor(
      kTestUserEmail,
      base::BindOnce(&ProjectorOAuthTokenFetcherTest::OnOAuthTokenFetchCallback,
                     base::Unretained(this), run_loop2.QuitClosure(),
                     kTestUserEmail));

  EXPECT_TRUE(fetcher().HasPendingRequestForTest(kTestUserEmail));

  mock_app_client().GrantOAuthTokenFor(
      kTestUserEmail,
      /* expiry_time = */ base::Time::Now() + kExpiryTimeFromNow);

  // Both requests should be granted and their corresponding RunLoop's quit
  // closure should be run. Otherwise the run loops will keep running and
  // the test will time out.
  run_loop1.Run();
  run_loop2.Run();

  EXPECT_TRUE(fetcher().HasCachedTokenForTest(kTestUserEmail));
}

TEST_F(ProjectorOAuthTokenFetcherTest, AlmostExpiredToken) {
  base::RunLoop run_loop1;
  fetcher().GetAccessTokenFor(
      kTestUserEmail,
      base::BindOnce(&ProjectorOAuthTokenFetcherTest::OnOAuthTokenFetchCallback,
                     base::Unretained(this), run_loop1.QuitClosure(),
                     kTestUserEmail));

  mock_app_client().GrantOAuthTokenFor(kTestUserEmail,
                                       /* expiry_time = */
                                       base::Time::Now());
  run_loop1.Run();

  base::RunLoop run_loop2;
  fetcher().GetAccessTokenFor(
      kTestUserEmail,
      base::BindOnce(&ProjectorOAuthTokenFetcherTest::OnOAuthTokenFetchCallback,
                     base::Unretained(this), run_loop2.QuitClosure(),
                     kTestUserEmail));

  EXPECT_FALSE(fetcher().HasCachedTokenForTest(kTestUserEmail));

  mock_app_client().GrantOAuthTokenFor(kTestUserEmail,
                                       /* expiry_time = */
                                       base::Time::Now() + kExpiryTimeFromNow);
  run_loop2.Run();

  EXPECT_TRUE(fetcher().HasCachedTokenForTest(kTestUserEmail));
}

TEST_F(ProjectorOAuthTokenFetcherTest, MultipleAccountsRequesting) {
  mock_app_client().AddSecondaryAccount(kTestUser2Email);

  base::RunLoop run_loop1;
  fetcher().GetAccessTokenFor(
      kTestUserEmail,
      base::BindOnce(&ProjectorOAuthTokenFetcherTest::OnOAuthTokenFetchCallback,
                     base::Unretained(this), run_loop1.QuitClosure(),
                     kTestUserEmail));

  base::RunLoop run_loop2;
  fetcher().GetAccessTokenFor(
      kTestUser2Email,
      base::BindOnce(&ProjectorOAuthTokenFetcherTest::OnOAuthTokenFetchCallback,
                     base::Unretained(this), run_loop2.QuitClosure(),
                     kTestUser2Email));

  EXPECT_TRUE(fetcher().HasPendingRequestForTest(kTestUserEmail));
  EXPECT_TRUE(fetcher().HasPendingRequestForTest(kTestUser2Email));

  mock_app_client().GrantOAuthTokenFor(kTestUserEmail,
                                       /* expiry_time = */
                                       base::Time::Now() + kExpiryTimeFromNow);
  mock_app_client().GrantOAuthTokenFor(kTestUser2Email,
                                       /* expiry_time = */
                                       base::Time::Now() + kExpiryTimeFromNow);
  run_loop1.Run();
  run_loop2.Run();

  // Now let's check that the tokens are present for both accounts.
  EXPECT_TRUE(fetcher().HasCachedTokenForTest(kTestUserEmail));
  EXPECT_TRUE(fetcher().HasCachedTokenForTest(kTestUser2Email));
}

TEST_F(ProjectorOAuthTokenFetcherTest, ValidCachedToken) {
  base::RunLoop run_loop1;
  fetcher().GetAccessTokenFor(
      kTestUserEmail,
      base::BindOnce(&ProjectorOAuthTokenFetcherTest::OnOAuthTokenFetchCallback,
                     base::Unretained(this), run_loop1.QuitClosure(),
                     kTestUserEmail));

  mock_app_client().GrantOAuthTokenFor(kTestUserEmail,
                                       /* expiry_time = */
                                       base::Time::Now() + kExpiryTimeFromNow);
  run_loop1.Run();

  EXPECT_TRUE(fetcher().HasCachedTokenForTest(kTestUserEmail));

  base::RunLoop run_loop2;
  fetcher().GetAccessTokenFor(
      kTestUserEmail,
      base::BindOnce(&ProjectorOAuthTokenFetcherTest::OnOAuthTokenFetchCallback,
                     base::Unretained(this), run_loop2.QuitClosure(),
                     kTestUserEmail));

  // A valid token for `kTestUserEmail` is cached in ProjectorOAuthTokenFetcher.
  // Therefore, the request should be granted immediately and the callback
  // executed with the results without the need to go through
  // signin::IdentityManager.
  run_loop2.Run();

  EXPECT_TRUE(fetcher().HasCachedTokenForTest(kTestUserEmail));
}

}  // namespace ash
