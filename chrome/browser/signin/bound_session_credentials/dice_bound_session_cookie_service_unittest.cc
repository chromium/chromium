// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/dice_bound_session_cookie_service.h"

#include <memory>

#include "base/test/task_environment.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/fake_bound_session_cookie_refresh_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DiceBoundSessionCookieServiceTest : public ::testing::Test {
 public:
  DiceBoundSessionCookieServiceTest()
      : identity_test_env_(&url_loader_factory_) {
    dice_bound_session_cookie_service_ =
        std::make_unique<DiceBoundSessionCookieService>(
            bound_session_cookie_refresh_service_,
            *identity_test_env_.identity_manager());
    // Ensure any pending list accounts request is completed.
    identity_test_env().SetCookieAccounts({});
    CHECK(!url_loader_factory().IsPending(list_accounts_url()));

    // Erase response so that the expected list accounts request remains
    // pending.
    url_loader_factory().EraseResponse(GURL(list_accounts_url()));
  }

  ~DiceBoundSessionCookieServiceTest() override = default;

  std::string list_accounts_url() {
    return GaiaUrls::GetInstance()
        ->ListAccountsURLWithSource(GaiaConstants::kChromeSource)
        .spec();
  }

  FakeBoundSessionCookieRefreshService* bound_session_cookie_refresh_service() {
    return &bound_session_cookie_refresh_service_;
  }

  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }

  network::TestURLLoaderFactory& url_loader_factory() {
    return url_loader_factory_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  FakeBoundSessionCookieRefreshService bound_session_cookie_refresh_service_;
  std::unique_ptr<DiceBoundSessionCookieService>
      dice_bound_session_cookie_service_;
};

TEST_F(DiceBoundSessionCookieServiceTest, OnBoundSessionTerminated) {
  bound_session_cookie_refresh_service()->SimulateOnBoundSessionTerminated(
      GaiaUrls::GetInstance()->google_url(), base::flat_set<std::string>());
  ASSERT_TRUE(url_loader_factory().IsPending(list_accounts_url()));
  // Let the request complete.
  identity_test_env().SetCookieAccounts({});
}

TEST_F(DiceBoundSessionCookieServiceTest,
       NoneGaiaIncludedBoundSessionTerminated) {
  bound_session_cookie_refresh_service()->SimulateOnBoundSessionTerminated(
      GURL("wwww.youtube.com"), base::flat_set<std::string>());
  EXPECT_FALSE(url_loader_factory().IsPending(list_accounts_url()));
}
}  // namespace
