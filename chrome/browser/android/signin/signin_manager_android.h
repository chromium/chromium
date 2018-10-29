// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/core/browser/signin_manager_base.h"

class Profile;

// Android wrapper of the SigninManager which provides access from the Java
// layer. Note that on Android, there's only a single profile, and therefore
// a single instance of this wrapper. The name of the Java class is
// SigninManager.
// This class should only be accessed from the UI thread.
//
// This class implements parts of the sign-in flow, to make sure that policy
// is available before sign-in completes.
class SigninManagerAndroid : public SigninManagerBase::Observer {
 public:
  SigninManagerAndroid(JNIEnv* env, jobject obj);

  void CheckPolicyBeforeSignIn(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& username);

  void FetchPolicyBeforeSignIn(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);

  void AbortSignIn(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);

  // Indicates that the user has made the choice to sign-in. |username|
  // contains the email address of the account to use as primary.
  void OnSignInCompleted(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const base::android::JavaParamRef<jstring>& username);

  void SignOut(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& obj,
               jint signoutReason);

  base::android::ScopedJavaLocalRef<jstring> GetManagementDomain(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Delete all data for this profile.
  void WipeProfileData(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

  // Delete service worker caches for google.<eTLD>.
  void WipeGoogleServiceWorkerCaches(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void LogInSignedInUser(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  void ClearLastSignedInUser(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);

  jboolean IsSigninAllowedByPolicy(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  jboolean IsForceSigninEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  jboolean IsSignedInOnNative(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);

  // SigninManagerBase::Observer implementation.
  void GoogleSigninFailed(const GoogleServiceAuthError& error) override;
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username) override;
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

 private:
  friend class SigninManagerAndroidTest;
  FRIEND_TEST_ALL_PREFIXES(SigninManagerAndroidTest,
                           DeleteGoogleServiceWorkerCaches);

  ~SigninManagerAndroid() override;

  void OnPolicyRegisterDone(const std::string& dm_token,
                            const std::string& client_id);
  void OnPolicyFetchDone(bool success);

  void OnBrowsingDataRemoverDone();

  void OnSigninAllowedPrefChanged();

  static void WipeData(Profile* profile,
                       bool all_data,
                       base::OnceClosure callback);

  Profile* profile_;

  // Java-side SigninManager object.
  base::android::ScopedJavaGlobalRef<jobject> java_signin_manager_;

  // CloudPolicy credentials stored during a pending sign-in, awaiting user
  // confirmation before starting to fetch policies.
  std::string dm_token_;
  std::string client_id_;

  // Username that is pending sign-in. This is used to extract the domain name
  // for the policy dialog, when |username_| corresponds to a managed account.
  std::string username_;

  PrefChangeRegistrar pref_change_registrar_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SigninManagerAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SigninManagerAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_
