// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ANDROID_SIGNIN_MANAGER_ANDROID_H_
#define CHROME_BROWSER_SIGNIN_ANDROID_SIGNIN_MANAGER_ANDROID_H_

#include <memory>
#include <optional>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
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

  SigninManagerAndroid(const SigninManagerAndroid&) = delete;
  SigninManagerAndroid& operator=(const SigninManagerAndroid&) = delete;

  ~SigninManagerAndroid() override;

  void Shutdown() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  bool IsSigninAllowedByPolicy(JNIEnv* env) const;

  bool IsForceSigninEnabled(JNIEnv* env);

  // Registers a CloudPolicyClient for fetching policy for a user and fetches
  // the policy if necessary.
  void FetchAndApplyCloudPolicy(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_account_info,
      const base::RepeatingClosure& j_callback);

  void StopApplyingCloudPolicy(JNIEnv* env);

  void IsAccountManaged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_account_info,
      const base::android::JavaParamRef<jobject>& j_callback);

  base::android::ScopedJavaLocalRef<jstring> GetManagementDomain(JNIEnv* env);

  // Delete all data for this profile.
  void WipeProfileData(JNIEnv* env, const base::RepeatingClosure& callback);

  // Delete service worker caches for google.<eTLD>.
  void WipeGoogleServiceWorkerCaches(JNIEnv* env,
                                     const base::RepeatingClosure& callback);

  void SetUserAcceptedAccountManagement(JNIEnv* env,
                                        bool accepted_account_management);

  bool GetUserAcceptedAccountManagement(JNIEnv* env);

 private:
  friend class SigninManagerAndroidTest;
  FRIEND_TEST_ALL_PREFIXES(SigninManagerAndroidTest,
                           DeleteGoogleServiceWorkerCaches);

  struct ManagementCredentials {
    ManagementCredentials(const std::string& dm_token,
                          const std::string& client_id,
                          const std::vector<std::string>& user_affiliation_ids);
    ~ManagementCredentials();
    const std::string dm_token;
    const std::string client_id;
    const std::vector<std::string> user_affiliation_ids;
  };

  // Cached value for a previous execution of IsAccountManaged().
  struct CachedIsAccountManaged {
    std::string gaia_id;
    bool is_account_managed;
    base::Time expiration_time;
  };

  static bool MatchesCachedIsAccountManagedEntry(
      const CachedIsAccountManaged& cached_entry,
      const CoreAccountInfo& account);

  void OnSigninAllowedPrefChanged() const;
  bool IsSigninAllowed() const;

  using RegisterPolicyWithAccountCallback = base::OnceCallback<void(
      const std::optional<ManagementCredentials>& credentials)>;

  // If required registers for policy with given account. callback will be
  // called with credentials if the account is managed.
  void RegisterPolicyWithAccount(const CoreAccountInfo& account,
                                 RegisterPolicyWithAccountCallback callback);

  void OnPolicyRegisterDone(
      const CoreAccountInfo& account_id,
      base::OnceCallback<void()> policy_callback,
      const std::optional<ManagementCredentials>& credentials);

  void OnPolicyRegisterDoneForIsAccountManaged(
      const CoreAccountInfo& account,
      base::android::ScopedJavaGlobalRef<jobject> callback,
      base::Time start_time,
      const std::optional<ManagementCredentials>& credentials);

  void FetchPolicyBeforeSignIn(const CoreAccountInfo& account_id,
                               base::OnceCallback<void()> policy_callback,
                               const ManagementCredentials& credentials);

  static void WipeData(Profile* profile,
                       bool all_data,
                       base::OnceClosure callback);

  const raw_ptr<Profile> profile_ = nullptr;

  // Handler for prefs::kSigninAllowed set in user's profile.
  BooleanPrefMember signin_allowed_;

  // Handler for prefs::kForceBrowserSignin. This preference is set in Local
  // State, not in user prefs.
  BooleanPrefMember force_browser_signin_;

  const raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;
  const raw_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager_ =
      nullptr;
  const raw_ptr<policy::UserPolicySigninService> user_policy_signin_service_ =
      nullptr;

  // Java-side SigninManager object.
  base::android::ScopedJavaGlobalRef<jobject> java_signin_manager_;

  // The last invocation of IsAccountManaged() is cached.
  std::optional<CachedIsAccountManaged> cached_is_account_managed_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SigninManagerAndroid> weak_factory_;
};

#endif  // CHROME_BROWSER_SIGNIN_ANDROID_SIGNIN_MANAGER_ANDROID_H_
