// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/net/test/test_auth_helper.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/enterprise/net/browser_test_support_jni/EnterpriseProxyAndroidSigninBridge_jni.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace enterprise::test {

namespace {

class AndroidTestAuthHelper : public TestAuthHelper {
 public:
  AndroidTestAuthHelper() = default;
  ~AndroidTestAuthHelper() override = default;

  void SetUpInProcessBrowserTestFixture(
      std::string_view default_access_token) override {
    Java_EnterpriseProxyAndroidSigninBridge_installFakeAccountManagerFacade(
        base::android::AttachCurrentThread(),
        std::string(default_access_token));
  }

  void SetUpOnMainThread(Profile* /*profile*/) override {}

  void TearDownOnMainThread() override {
    Java_EnterpriseProxyAndroidSigninBridge_resetFakeAccountManagerFacade(
        base::android::AttachCurrentThread());
  }

  void SignInPrimaryAccount(signin::IdentityManager* identity_manager,
                            const std::string& email,
                            const std::string& hosted_domain,
                            const std::string& access_token) override {
    signin::WaitForRefreshTokensLoaded(identity_manager);

    const GaiaId gaia_id = signin::GetTestGaiaIdForEmail(email);
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_EnterpriseProxyAndroidSigninBridge_addAccount(
        env, email, gaia_id.ToString(), hosted_domain);
    Java_EnterpriseProxyAndroidSigninBridge_setOverrideAccessToken(
        env, access_token);
    Java_EnterpriseProxyAndroidSigninBridge_setAccessTokenError(
        env, gaia_id.ToString(),
        static_cast<int>(GoogleServiceAuthError::NONE));

    AccountInfo base_info = signin::MakeAccountAvailable(
        identity_manager, signin::AccountAvailabilityOptionsBuilder()
                              .AsPrimary(signin::ConsentLevel::kSignin)
                              .WithGaiaId(gaia_id)
                              .WithoutRefreshToken()
                              .Build(email));
    signin::SimulateSuccessfulFetchOfAccountInfo(
        identity_manager, base_info.GetAccountId(), email, gaia_id,
        hosted_domain, "Full Name", "Given Name", "en-US", kNoPictureURLFound);
    AccountInfo full_info =
        identity_manager->FindExtendedAccountInfoByAccountId(
            base_info.GetAccountId());
    identity_manager->GetDeviceAccountsSynchronizer()
        ->SeedAccountsThenReloadAllAccountsWithPrimaryAccount(
            {full_info}, full_info.GetAccountId());
  }

  void SimulateInvalidCredentials(const GaiaId& gaia_id) override {
    Java_EnterpriseProxyAndroidSigninBridge_setAccessTokenError(
        base::android::AttachCurrentThread(), gaia_id.ToString(),
        static_cast<int>(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  }

  size_t GetTokenFetchRequestCount() const override {
    return static_cast<size_t>(
        Java_EnterpriseProxyAndroidSigninBridge_getTokenRequestCount(
            base::android::AttachCurrentThread()));
  }
};

}  // namespace

// static
std::unique_ptr<TestAuthHelper> TestAuthHelper::Create() {
  return std::make_unique<AndroidTestAuthHelper>();
}

}  // namespace enterprise::test

DEFINE_JNI(EnterpriseProxyAndroidSigninBridge)
