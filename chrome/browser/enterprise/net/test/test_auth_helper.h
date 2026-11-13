// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_NET_TEST_TEST_AUTH_HELPER_H_
#define CHROME_BROWSER_ENTERPRISE_NET_TEST_TEST_AUTH_HELPER_H_

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

class GaiaId;
class Profile;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace enterprise::test {

// Platform-specific OAuth2 sign-in and token provisioning delegate owned by
// `EnterpriseProxyBrowserTestBase`:
// - Android (`test_auth_helper_android.cc`): installs a JNI-backed
//   `FakeAccountManagerFacade` (`EnterpriseProxyAndroidSigninBridge`) so
//   `ProfileOAuth2TokenServiceDelegateAndroid` fetches tokens out-of-band just
//   like Android OS Play Services in production.
// - Desktop (`test_auth_helper_desktop.cc`): currently wraps
//   `IdentityTestEnvironmentProfileAdaptor` (see `TODO(crbug.com/532559920)` to
//   migrate Desktop to `MutableProfileOAuth2TokenServiceDelegate` + `FakeGaia`
//   once Desktop OAuth2 token fetches bypass the dynamic routing network
//   pause).
class TestAuthHelper {
 public:
  static std::unique_ptr<TestAuthHelper> Create();

  TestAuthHelper();
  TestAuthHelper(const TestAuthHelper&) = delete;
  TestAuthHelper& operator=(const TestAuthHelper&) = delete;
  virtual ~TestAuthHelper();

  // Initializes test-fixture auth state before the browser starts.
  virtual void SetUpInProcessBrowserTestFixture(
      std::string_view default_access_token) = 0;

  // Attaches any profile-scoped auth state once `profile` is created on the
  // browser main thread.
  virtual void SetUpOnMainThread(Profile* profile) = 0;

  // Tears down platform auth state on the browser main thread.
  virtual void TearDownOnMainThread() = 0;

  // Signs in a managed primary account on `identity_manager` configured to mint
  // `access_token`.
  virtual void SignInPrimaryAccount(signin::IdentityManager* identity_manager,
                                    const std::string& email,
                                    const std::string& hosted_domain,
                                    const std::string& access_token) = 0;

  // Configures the underlying token provider to reject subsequent
  // `cloud_secure_gateway` OAuth2 token requests with
  // `INVALID_GAIA_CREDENTIALS`.
  virtual void SimulateInvalidCredentials(const GaiaId& gaia_id) = 0;

  // Returns the number of OAuth2 access token fetch requests observed for the
  // `cloud_secure_gateway` scope.
  virtual size_t GetTokenFetchRequestCount() const = 0;
};

}  // namespace enterprise::test

#endif  // CHROME_BROWSER_ENTERPRISE_NET_TEST_TEST_AUTH_HELPER_H_
