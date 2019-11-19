// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/profile_auth_data.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

const char kProxyAuthURL[] = "https://example.com/";
const char kProxyAuthRealm[] = "realm";
const char kProxyAuthChallenge[] = "challenge";
const char kProxyAuthPassword1[] = "password 1";
const char kProxyAuthPassword2[] = "password 2";

const char kGAIACookieURL[] = "https://google.com/";
const char kSAMLIdPCookieURL[] = "https://example.com/";
const char kCookieName[] = "cookie";
const char kCookieValue1[] = "value 1";
const char kCookieValue2[] = "value 2";
const char kGAIACookieDomain[] = "google.com";
const char kSAMLIdPCookieDomain[] = "example.com";
const char kSAMLIdPCookieDomainWithWildcard[] = ".example.com";

class TestingProfileWithNetworkContext : public TestingProfile {
 public:
  explicit TestingProfileWithNetworkContext(
      network::NetworkService* network_service) {
    auto network_context = std::make_unique<network::NetworkContext>(
        network_service, network_context_remote_.BindNewPipeAndPassReceiver(),
        network::mojom::NetworkContextParams::New());
    network_context_ = network_context.get();
    SetNetworkContext(std::move(network_context));
  }

  network::NetworkContext* network_context() { return network_context_; }

 private:
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
  network::NetworkContext* network_context_;
};

network::NetworkService* GetNetworkService() {
  content::GetNetworkService();
  // Wait for the Network Service to initialize on the IO thread.
  content::RunAllPendingInMessageLoop(content::BrowserThread::IO);
  return network::NetworkService::GetNetworkServiceForTesting();
}

}  // namespace

class ProfileAuthDataTest : public testing::Test {
 public:
  ProfileAuthDataTest();

  // testing::Test:
  void SetUp() override;

  void PopulateUserBrowserContext();

  void Transfer(bool transfer_auth_cookies_on_first_login,
                bool transfer_saml_auth_cookies_on_subsequent_login);

  net::CookieList GetUserCookies();

  void VerifyTransferredUserProxyAuthEntry();
  void VerifyUserCookies(const std::string& expected_gaia_cookie_value,
                         const std::string& expected_saml_idp_cookie_value);

 private:
  void PopulateBrowserContext(TestingProfileWithNetworkContext* browser_context,
                              const std::string& proxy_auth_password,
                              const std::string& cookie_value);

  net::HttpAuthCache* GetAuthCache(network::NetworkContext* network_context);
  network::mojom::CookieManager* GetCookies(
      content::BrowserContext* browser_context);

  content::BrowserTaskEnvironment task_environment_;

  network::NetworkService* network_service_;
  TestingProfileWithNetworkContext login_browser_context_;
  TestingProfileWithNetworkContext user_browser_context_;
};

ProfileAuthDataTest::ProfileAuthDataTest()
    : network_service_(GetNetworkService()),
      login_browser_context_(network_service_),
      user_browser_context_(network_service_) {}

void ProfileAuthDataTest::SetUp() {
  PopulateBrowserContext(&login_browser_context_, kProxyAuthPassword1,
                         kCookieValue1);
}

void ProfileAuthDataTest::PopulateUserBrowserContext() {
  PopulateBrowserContext(&user_browser_context_, kProxyAuthPassword2,
                         kCookieValue2);
}

void ProfileAuthDataTest::Transfer(
    bool transfer_auth_cookies_on_first_login,
    bool transfer_saml_auth_cookies_on_subsequent_login) {
  base::RunLoop run_loop;
  ProfileAuthData::Transfer(content::BrowserContext::GetDefaultStoragePartition(
                                &login_browser_context_),
                            content::BrowserContext::GetDefaultStoragePartition(
                                &user_browser_context_),
                            transfer_auth_cookies_on_first_login,
                            transfer_saml_auth_cookies_on_subsequent_login,
                            run_loop.QuitClosure());
  run_loop.Run();
  if (!transfer_auth_cookies_on_first_login &&
      !transfer_saml_auth_cookies_on_subsequent_login) {
    // When only proxy auth state is being transferred, the completion callback
    // is invoked before the transfer has actually completed. Spin the loop once
    // more to allow the transfer to complete.
    base::RunLoop().RunUntilIdle();
  }
}

net::CookieList ProfileAuthDataTest::GetUserCookies() {
  base::RunLoop run_loop;
  net::CookieList result;
  GetCookies(&user_browser_context_)
      ->GetAllCookies(
          base::BindLambdaForTesting([&](const net::CookieList& cookie_list) {
            result = cookie_list;
            run_loop.Quit();
          }));
  run_loop.Run();
  return result;
}

void ProfileAuthDataTest::VerifyTransferredUserProxyAuthEntry() {
  net::HttpAuthCache::Entry* entry =
      GetAuthCache(user_browser_context_.network_context())
          ->Lookup(GURL(kProxyAuthURL), net::HttpAuth::AUTH_PROXY,
                   kProxyAuthRealm, net::HttpAuth::AUTH_SCHEME_BASIC,
                   net::NetworkIsolationKey());
  ASSERT_TRUE(entry);
  EXPECT_EQ(base::ASCIIToUTF16(kProxyAuthPassword1),
            entry->credentials().password());
}

void ProfileAuthDataTest::VerifyUserCookies(
    const std::string& expected_gaia_cookie_value,
    const std::string& expected_saml_idp_cookie_value) {
  net::CookieList user_cookies = GetUserCookies();
  ASSERT_EQ(3u, user_cookies.size());

  // Cookies are returned chronoligically, in the order they were set.
  net::CanonicalCookie* cookie = &user_cookies[0];
  EXPECT_EQ(kCookieName, cookie->Name());
  EXPECT_EQ(expected_saml_idp_cookie_value, cookie->Value());
  EXPECT_EQ(kSAMLIdPCookieDomainWithWildcard, cookie->Domain());

  cookie = &user_cookies[1];
  EXPECT_EQ(kCookieName, cookie->Name());
  EXPECT_EQ(expected_saml_idp_cookie_value, cookie->Value());
  EXPECT_EQ(kSAMLIdPCookieDomain, cookie->Domain());

  cookie = &user_cookies[2];
  EXPECT_EQ(kCookieName, cookie->Name());
  EXPECT_EQ(expected_gaia_cookie_value, cookie->Value());
  EXPECT_EQ(kGAIACookieDomain, cookie->Domain());
}

void ProfileAuthDataTest::PopulateBrowserContext(
    TestingProfileWithNetworkContext* browser_context,
    const std::string& proxy_auth_password,
    const std::string& cookie_value) {
  GetAuthCache(browser_context->network_context())
      ->Add(GURL(kProxyAuthURL), net::HttpAuth::AUTH_PROXY, kProxyAuthRealm,
            net::HttpAuth::AUTH_SCHEME_BASIC, net::NetworkIsolationKey(),
            kProxyAuthChallenge,
            net::AuthCredentials(base::string16(),
                                 base::ASCIIToUTF16(proxy_auth_password)),
            std::string());

  network::mojom::CookieManager* cookies = GetCookies(browser_context);
  // Ensure |cookies| is fully initialized.
  base::RunLoop run_loop;
  cookies->GetAllCookies(base::BindLambdaForTesting(
      [&](const net::CookieList& cookies) { run_loop.Quit(); }));
  run_loop.Run();

  net::CookieOptions options;
  options.set_include_httponly();
  cookies->SetCanonicalCookie(
      *net::CanonicalCookie::CreateSanitizedCookie(
          GURL(kSAMLIdPCookieURL), kCookieName, cookie_value,
          kSAMLIdPCookieDomainWithWildcard, std::string(), base::Time(),
          base::Time(), base::Time(), true, false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT),
      "https", options, base::DoNothing());

  cookies->SetCanonicalCookie(
      *net::CanonicalCookie::CreateSanitizedCookie(
          GURL(kSAMLIdPCookieURL), kCookieName, cookie_value, std::string(),
          std::string(), base::Time(), base::Time(), base::Time(), true, false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT),
      "https", options, base::DoNothing());

  cookies->SetCanonicalCookie(
      *net::CanonicalCookie::CreateSanitizedCookie(
          GURL(kGAIACookieURL), kCookieName, cookie_value, std::string(),
          std::string(), base::Time(), base::Time(), base::Time(), true, false,
          net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT),
      "https", options, base::DoNothing());
}

net::HttpAuthCache* ProfileAuthDataTest::GetAuthCache(
    network::NetworkContext* network_context) {
  return network_context->url_request_context()
      ->http_transaction_factory()
      ->GetSession()
      ->http_auth_cache();
}

network::mojom::CookieManager* ProfileAuthDataTest::GetCookies(
    content::BrowserContext* browser_context) {
  return content::BrowserContext::GetDefaultStoragePartition(browser_context)
      ->GetCookieManagerForBrowserProcess();
}

// Verifies that when no transfer of auth cookies is requested, only the proxy
// auth state is transferred.
TEST_F(ProfileAuthDataTest, DoNotTransfer) {
  Transfer(false, false);

  VerifyTransferredUserProxyAuthEntry();
  EXPECT_TRUE(GetUserCookies().empty());
}

// Verifies that when the transfer of auth cookies on first login is requested,
// they do get transferred along with the proxy auth state on first login.
TEST_F(ProfileAuthDataTest, TransferOnFirstLoginWithNewProfile) {
  Transfer(true, false);

  VerifyTransferredUserProxyAuthEntry();
  VerifyUserCookies(kCookieValue1, kCookieValue1);
}

// Verifies that even if the transfer of auth cookies on first login is
// requested, only the proxy auth state is transferred on subsequent login.
TEST_F(ProfileAuthDataTest, TransferOnFirstLoginWithExistingProfile) {
  PopulateUserBrowserContext();

  Transfer(true, false);

  VerifyTransferredUserProxyAuthEntry();
  VerifyUserCookies(kCookieValue2, kCookieValue2);
}

// Verifies that when the transfer of auth cookies set by a SAML IdP on
// subsequent login is requested, they do get transferred along with the proxy
// auth state on subsequent login.
TEST_F(ProfileAuthDataTest, TransferOnSubsequentLogin) {
  PopulateUserBrowserContext();

  Transfer(false, true);

  VerifyTransferredUserProxyAuthEntry();
  VerifyUserCookies(kCookieValue2, kCookieValue1);
}

}  // namespace chromeos
