// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/signin_investigator_android.h"

#include "base/android/jni_string.h"
#include "base/optional.h"
#include "chrome/android/chrome_jni_headers/SigninInvestigator_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/investigator_dependency_provider.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

// static
jint JNI_SigninInvestigator_Investigate(
    JNIEnv* env,
    const JavaParamRef<jstring>& current_email) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  InvestigatorDependencyProvider provider(profile);
  const std::string email = ConvertJavaStringToUTF8(env, current_email);

  // It is possible that the Identity Service is not aware of that account
  // yet. In that case, pass an empty account_id to the investigator, so
  // that it falls back to email comparison.
  base::Optional<AccountInfo> maybe_account_info =
      IdentityManagerFactory::GetForProfile(profile)
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
              email);
  std::string account_id;
  if (maybe_account_info.has_value())
    account_id = maybe_account_info.value().account_id;

  return static_cast<int>(
      SigninInvestigator(email, account_id, &provider).Investigate());
}
