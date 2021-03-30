// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/signin_metrics_utils.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/services/android/jni_headers/SigninMetricsUtils_jni.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

using signin_metrics::AccountConsistencyPromoAfterDismissal;

static void JNI_SigninMetricsUtils_LogProfileAccountManagementMenu(
    JNIEnv* env,
    jint metric,
    jint gaia_service_type) {
  ProfileMetrics::LogProfileAndroidAccountManagementMenu(
      static_cast<ProfileMetrics::ProfileAndroidAccountManagementMenu>(metric),
      static_cast<signin::GAIAServiceType>(gaia_service_type));
}

static void JNI_SigninMetricsUtils_LogSigninUserActionForAccessPoint(
    JNIEnv* env,
    jint access_point) {
  signin_metrics::RecordSigninUserActionForAccessPoint(
      static_cast<signin_metrics::AccessPoint>(access_point),
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
}

static jboolean JNI_SigninMetricsUtils_LogWebSignin(
    JNIEnv* env,
    const base::android::JavaParamRef<jobjectArray>& java_gaia_ids) {
  std::vector<std::string> gaia_ids;
  base::android::AppendJavaStringArrayToStringVector(env, java_gaia_ids,
                                                     &gaia_ids);

  return signin_metrics_utils::LogWebSignin(
      IdentityManagerFactory::GetForProfile(
          ProfileManager::GetLastUsedProfile()),
      gaia_ids);
}

bool signin_metrics_utils::LogWebSignin(
    signin::IdentityManager* identity_manager,
    const std::vector<std::string>& gaia_ids) {
  signin::AccountsInCookieJarInfo accounts_in_cookie_jar_info =
      identity_manager->GetAccountsInCookieJar();
  if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // Account is signed in to Chrome. The metrics recorded here are only for
    // web signin.
    return false;
  }
  if (!accounts_in_cookie_jar_info.accounts_are_fresh ||
      accounts_in_cookie_jar_info.signed_in_accounts.empty()) {
    return false;
  }

  gaia::ListedAccount signed_in_account =
      accounts_in_cookie_jar_info.signed_in_accounts[0];
  auto it = std::find_if(gaia_ids.begin(), gaia_ids.end(),
                         [&signed_in_account](const std::string& gaia_id) {
                           return gaia_id == signed_in_account.gaia_id;
                         });
  AccountConsistencyPromoAfterDismissal action;
  if (it == gaia_ids.end()) {
    action =
        AccountConsistencyPromoAfterDismissal::kSignedInOnWebWithOtherAccount;
  } else if (it == gaia_ids.begin()) {
    action = AccountConsistencyPromoAfterDismissal::
        kSignedInOnWebWithDefaultDeviceAccount;
  } else {
    action = AccountConsistencyPromoAfterDismissal::
        kSignedInOnWebWithNonDefaultDeviceAccount;
  }
  base::UmaHistogramEnumeration("Signin.AccountConsistencyPromoAfterDismissal",
                                action);
  return true;
}
