// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lens/lens_identity_delegation_helper.h"

#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "build/branding_buildflags.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_options.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::IsEmpty;

namespace lens {

class LensIdentityDelegationHelperTest : public testing::Test {
 protected:
  LensIdentityDelegationHelperTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    testing::Test::SetUp();
    profile_ = std::make_unique<TestingProfile>();
  }

  void TearDown() override {
    profile_.reset();
    testing::Test::TearDown();
  }

  void SetSapisidCookie(const std::string& value) {
    base::test::TestFuture<net::CookieAccessResult> future;
    auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
        "SAPISID", value, ".google.com", "/", base::Time(), base::Time(),
        base::Time(), base::Time(), /*secure=*/true, /*httponly=*/false,
        net::CookieSameSite::NO_RESTRICTION,
        net::CookiePriority::COOKIE_PRIORITY_DEFAULT,
        net::CookieSourceType::kHTTP);

    profile_->GetDefaultStoragePartition()
        ->GetCookieManagerForBrowserProcess()
        ->SetCanonicalCookie(*cookie, GURL("https://google.com"),
                             net::CookieOptions::MakeAllInclusive(),
                             future.GetCallback());
    EXPECT_TRUE(future.Get().status.IsInclude());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_SignedOut) {
  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(
      profile_.get(), identity_test_env_.identity_manager(),
      "https://www.google.com", std::nullopt, future.GetCallback());

  // Signed out: should only return Origin header.
  EXPECT_THAT(future.Get(), ElementsAre("Origin", "https://www.google.com"));
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_OriginCanonicalization) {
  base::test::TestFuture<std::vector<std::string>> future;
  // Pass an origin with trailing slash and path.
  FetchIdentityDelegationHeaders(
      profile_.get(), identity_test_env_.identity_manager(),
      "https://www.google.com/search?q=test/", std::nullopt,
      future.GetCallback());

  // Origin should be normalized and canonicalized without trailing slash or path.
  EXPECT_THAT(future.Get(), ElementsAre("Origin", "https://www.google.com"));
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_SignedIn_NoCookie) {
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  // Force update cookie jar accounts in IdentityManager.
  identity_test_env_.SetCookieAccounts(
      {{std::string(account_info.GetEmail()), account_info.GetGaiaId()}});

  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(
      profile_.get(), identity_test_env_.identity_manager(),
      "https://www.google.com", std::nullopt, future.GetCallback());

  // Signed in but no cookie: should only return Origin header.
  EXPECT_THAT(future.Get(), ElementsAre("Origin", "https://www.google.com"));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(LensIdentityDelegationHelperTest, GenerateSapisidHash_GoldenTest) {
  // Use fixed inputs to verify the hash algorithm.
  std::string email = "user@gmail.com";
  std::string sapisid = "sapisid_cookie_value";
  std::string origin = "https://www.google.com";
  // 2026-06-12 12:00:00 UTC
  base::Time timestamp;
  ASSERT_TRUE(base::Time::FromUTCString("2026-06-12 12:00:00 UTC", &timestamp));

  // Expected values for testing:
  // timestamp_millis = 1781265600000
  // hash_source = "user@gmail.com 1781265600000 sapisid_cookie_value
  // https://www.google.com" SHA1(hash_source) = ... Expected output starts
  // with: "SAPISIDHASH 1781265600000_"

  std::optional<std::string> hash =
      GenerateSapisidHash(email, sapisid, origin, timestamp);
  ASSERT_TRUE(hash.has_value());

  // Verify the exact string that is deterministically generated.
  EXPECT_EQ(
      hash.value(),
      "SAPISIDHASH 1781265600000_9bd27681bae726e0f13c8da3f7ec536243912710_e");
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_SignedIn_WithCookie) {
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env_.SetCookieAccounts(
      {{std::string(account_info.GetEmail()), account_info.GetGaiaId()}});
  SetSapisidCookie("sapisid_value");

  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(
      profile_.get(), identity_test_env_.identity_manager(),
      "https://www.google.com", std::nullopt, future.GetCallback());

  std::vector<std::string> headers = future.Get();
  ASSERT_EQ(headers.size(), 6u);
  EXPECT_EQ(headers[0], "Origin");
  EXPECT_EQ(headers[1], "https://www.google.com");
  EXPECT_EQ(headers[2], "Authorization");
  EXPECT_TRUE(headers[3].starts_with("SAPISIDHASH "));
  EXPECT_EQ(headers[4], "X-Goog-AuthUser");
  EXPECT_EQ(headers[5], "0");  // Index 0 in cookie jar
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_MultipleAccounts_PrimaryMatches) {
  // Primary is user2
  signin::SimpleAccountAvailabilityOptions options;

  options.primary_account_consent_level = signin::ConsentLevel::kSignin;

  options.gaia_id = GaiaId("gaia_id_2");

  identity_test_env_.MakeAccountAvailable("user2@gmail.com", options);
  // Cookie jar has user1 (index 0) and user2 (index 1)
  identity_test_env_.SetCookieAccounts(
      {{"user1@gmail.com", GaiaId("gaia_id_1")},
       {"user2@gmail.com", GaiaId("gaia_id_2")}});
  SetSapisidCookie("sapisid_value");

  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(
      profile_.get(), identity_test_env_.identity_manager(),
      "https://www.google.com", std::nullopt, future.GetCallback());

  std::vector<std::string> headers = future.Get();
  ASSERT_EQ(headers.size(), 6u);
  EXPECT_EQ(headers[4], "X-Goog-AuthUser");
  EXPECT_EQ(headers[5], "1");  // user2 is at index 1
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_MultipleAccounts_NoPrimaryMatch) {
  // No primary account (web-only sign-in)
  // Cookie jar has user1 (index 0) and user2 (index 1)
  identity_test_env_.SetCookieAccounts(
      {{"user1@gmail.com", GaiaId("gaia_id_1")},
       {"user2@gmail.com", GaiaId("gaia_id_2")}});
  SetSapisidCookie("sapisid_value");

  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(
      profile_.get(), identity_test_env_.identity_manager(),
      "https://www.google.com", std::nullopt, future.GetCallback());

  std::vector<std::string> headers = future.Get();
  EXPECT_TRUE(headers.empty());
}

TEST_F(LensIdentityDelegationHelperTest,
       FetchIdentityDelegationHeaders_MultipleAccounts_SpecificIndex) {
  identity_test_env_.SetCookieAccounts(
      {{"user1@gmail.com", GaiaId("gaia_id_1")},
       {"user2@gmail.com", GaiaId("gaia_id_2")}});
  SetSapisidCookie("sapisid_value");

  base::test::TestFuture<std::vector<std::string>> future;
  FetchIdentityDelegationHeaders(
      profile_.get(), identity_test_env_.identity_manager(),
      "https://www.google.com", /*authuser_index=*/1, future.GetCallback());

  std::vector<std::string> headers = future.Get();
  ASSERT_EQ(headers.size(), 6u);
  EXPECT_EQ(headers[4], "X-Goog-AuthUser");
  EXPECT_EQ(headers[5], "1");
}
#endif

}  // namespace lens
