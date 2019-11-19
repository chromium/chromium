// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"

namespace policy {
class UserCloudPolicyManager;
class UserPolicySigninService;
}  // namespace policy

namespace signin {
class IdentityManager;
}

struct CoreAccountInfo;
class Profile;

// Android wrapper of Chrome's C++ identity management code which provides
// access from the Java layer. Note that on Android, there's only a single
// profile, and therefore a single instance of this wrapper. The name of the
// Java class is SigninManager. This class should only be accessed from the UI
// thread.
//
// This class implements parts of the sign-in flow, to make sure that policy
// is available before sign-in completes.
class SigninManagerAndroid : public KeyedService {
 public:
  SigninManagerAndroid(Profile* profile,
                       signin::IdentityManager* identity_manager);

  ~SigninManagerAndroid() override;

  void Shutdown() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  jboolean IsSigninAllowedByPolicy(JNIEnv* env) const;

  jboolean IsForceSigninEnabled(JNIEnv* env);

  // Registers a CloudPolicyClient for fetching policy for a user and fetches
  // the policy if necessary.
  void FetchAndApplyCloudPolicy(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_account_info,
      const base::android::JavaParamRef<jobject>& j_callback);

  void StopApplyingCloudPolicy(JNIEnv* env);

  void IsAccountManaged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_account_info,
      const base::android::JavaParamRef<jobject>& j_callback);

  base::android::ScopedJavaLocalRef<jstring> GetManagementDomain(JNIEnv* env);

  // Delete all data for this profile.
  void WipeProfileData(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& j_callback);

  // Delete service worker caches for google.<eTLD>.
  void WipeGoogleServiceWorkerCaches(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_callback);

 private:
  friend class SigninManagerAndroidTest;
  FRIEND_TEST_ALL_PREFIXES(SigninManagerAndroidTest,
                           DeleteGoogleServiceWorkerCaches);

  void OnSigninAllowedPrefChanged() const;
  bool IsSigninAllowed() const;

  struct ManagementCredentials {
    ManagementCredentials(const std::string& dm_token,
                          const std::string& client_id)
        : dm_token(dm_token), client_id(client_id) {}
    const std::string dm_token;
    const std::string client_id;
  };

  using RegisterPolicyWithAccountCallback = base::OnceCallback<void(
      const base::Optional<ManagementCredentials>& credentials)>;

  // If required registers for policy with given account. callback will be
  // called with credentials if the account is managed.
  void RegisterPolicyWithAccount(const CoreAccountInfo& account,
                                 RegisterPolicyWithAccountCallback callback);

  void OnPolicyRegisterDone(
      const CoreAccountInfo& account_id,
      base::OnceCallback<void()> policy_callback,
      const base::Optional<ManagementCredentials>& credentials);

  void FetchPolicyBeforeSignIn(const CoreAccountInfo& account_id,
                               base::OnceCallback<void()> policy_callback,
                               const ManagementCredentials& credentials);

  static void WipeData(Profile* profile,
                       bool all_data,
                       base::OnceClosure callback);

  Profile* const profile_ = nullptr;

  // Handler for prefs::kSigninAllowed set in user's profile.
  BooleanPrefMember signin_allowed_;

  // Handler for prefs::kForceBrowserSignin. This preference is set in Local
  // State, not in user prefs.
  BooleanPrefMember force_browser_signin_;

  signin::IdentityManager* const identity_manager_ = nullptr;
  policy::UserCloudPolicyManager* const user_cloud_policy_manager_ = nullptr;
  policy::UserPolicySigninService* const user_policy_signin_service_ = nullptr;

  // Java-side SigninManager object.
  base::android::ScopedJavaGlobalRef<jobject> java_signin_manager_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SigninManagerAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SigninManagerAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_SIGNIN_SIGNIN_MANAGER_ANDROID_H_
