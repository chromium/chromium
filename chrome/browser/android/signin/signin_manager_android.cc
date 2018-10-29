// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/signin_manager_android.h"

#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/policy/cloud/user_cloud_policy_manager_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_mobile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/oauth2_token_service_delegate_android.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/google/core/common/google_util.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "jni/SigninManager_jni.h"

using base::android::JavaParamRef;
using bookmarks::BookmarkModel;

namespace {

// Clears the information about the last signed-in user from |profile|.
void ClearLastSignedInUserForProfile(Profile* profile) {
  profile->GetPrefs()->ClearPref(prefs::kGoogleServicesLastAccountId);
  profile->GetPrefs()->ClearPref(prefs::kGoogleServicesLastUsername);
}

// A BrowsingDataRemover::Observer that clears Profile data and then invokes
// a callback and deletes itself. It can be configured to delete all data
// (for enterprise users) or only Google's service workers (for all users).
class ProfileDataRemover : public content::BrowsingDataRemover::Observer {
 public:
  ProfileDataRemover(Profile* profile,
                     bool all_data,
                     base::OnceClosure callback)
      : profile_(profile),
        all_data_(all_data),
        callback_(std::move(callback)),
        origin_runner_(base::ThreadTaskRunnerHandle::Get()),
        remover_(content::BrowserContext::GetBrowsingDataRemover(profile)) {
    remover_->AddObserver(this);

    if (all_data) {
      remover_->RemoveAndReply(
          base::Time(), base::Time::Max(),
          ChromeBrowsingDataRemoverDelegate::ALL_DATA_TYPES,
          ChromeBrowsingDataRemoverDelegate::ALL_ORIGIN_TYPES, this);
    } else {
      std::unique_ptr<content::BrowsingDataFilterBuilder> google_tld_filter =
          content::BrowsingDataFilterBuilder::Create(
              content::BrowsingDataFilterBuilder::WHITELIST);

      // TODO(msramek): BrowsingDataFilterBuilder was not designed for
      // large filters. Optimize it.
      for (const std::string& domain :
           google_util::GetGoogleRegistrableDomains()) {
        google_tld_filter->AddRegisterableDomain(domain);
      }

      remover_->RemoveWithFilterAndReply(
          base::Time(), base::Time::Max(),
          content::BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE,
          ChromeBrowsingDataRemoverDelegate::ALL_ORIGIN_TYPES,
          std::move(google_tld_filter), this);
    }
  }

  ~ProfileDataRemover() override {}

  void OnBrowsingDataRemoverDone() override {
    remover_->RemoveObserver(this);

    if (all_data_) {
      // All the Profile data has been wiped. Clear the last signed in username
      // as well, so that the next signin doesn't trigger the account
      // change dialog.
      ClearLastSignedInUserForProfile(profile_);
    }

    origin_runner_->PostTask(FROM_HERE, std::move(callback_));
    origin_runner_->DeleteSoon(FROM_HERE, this);
  }

 private:
  Profile* profile_;
  bool all_data_;
  base::OnceClosure callback_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_runner_;
  content::BrowsingDataRemover* remover_;

  DISALLOW_COPY_AND_ASSIGN(ProfileDataRemover);
};

void UserManagementDomainFetched(
    base::android::ScopedJavaGlobalRef<jobject> callback,
    const std::string& dm_token, const std::string& client_id) {
  base::android::RunBooleanCallbackAndroid(callback, !dm_token.empty());
}

}  // namespace

SigninManagerAndroid::SigninManagerAndroid(JNIEnv* env, jobject obj)
    : profile_(NULL),
      weak_factory_(this) {
  java_signin_manager_.Reset(env, obj);
  profile_ = ProfileManager::GetActiveUserProfile();
  DCHECK(profile_);
  SigninManagerFactory::GetForProfile(profile_)->AddObserver(this);
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSigninAllowed,
      base::Bind(&SigninManagerAndroid::OnSigninAllowedPrefChanged,
                 base::Unretained(this)));
}

SigninManagerAndroid::~SigninManagerAndroid() {}

void SigninManagerAndroid::CheckPolicyBeforeSignIn(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& username) {
  username_ = base::android::ConvertJavaStringToUTF8(env, username);
  policy::UserPolicySigninService* service =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
  service->RegisterForPolicyWithAccountId(
      username_,
      AccountTrackerServiceFactory::GetForProfile(profile_)
          ->FindAccountInfoByEmail(username_)
          .account_id,
      base::Bind(&SigninManagerAndroid::OnPolicyRegisterDone,
                 weak_factory_.GetWeakPtr()));
}

void SigninManagerAndroid::FetchPolicyBeforeSignIn(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!dm_token_.empty()) {
    policy::UserPolicySigninService* service =
        policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        content::BrowserContext::GetDefaultStoragePartition(profile_)
            ->GetURLLoaderFactoryForBrowserProcess();
    service->FetchPolicyForSignedInUser(
        AccountIdFromAccountInfo(
            AccountTrackerServiceFactory::GetForProfile(profile_)
                ->FindAccountInfoByEmail(username_)),
        dm_token_, client_id_, url_loader_factory,
        base::Bind(&SigninManagerAndroid::OnPolicyFetchDone,
                   weak_factory_.GetWeakPtr()));
    dm_token_.clear();
    client_id_.clear();
    return;
  }

  // This shouldn't be called when ShouldLoadPolicyForUser() is false, or when
  // CheckPolicyBeforeSignIn() failed.
  NOTREACHED();
  Java_SigninManager_onPolicyFetchedBeforeSignIn(env, java_signin_manager_);
}

void SigninManagerAndroid::AbortSignIn(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  policy::UserPolicySigninService* service =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile_);
  service->ShutdownUserCloudPolicyManager();
}

void SigninManagerAndroid::OnSignInCompleted(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& username) {
  DVLOG(1) << "SigninManagerAndroid::OnSignInCompleted";
  SigninManagerFactory::GetForProfile(profile_)->OnExternalSigninCompleted(
      base::android::ConvertJavaStringToUTF8(env, username));
}

void SigninManagerAndroid::SignOut(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   jint signoutReason) {
  SigninManagerFactory::GetForProfile(profile_)->SignOut(
      static_cast<signin_metrics::ProfileSignout>(signoutReason),
      // Always use IGNORE_METRIC for the profile deletion argument. Chrome
      // Android has just a single-profile which is never deleted upon
      // sign-out.
      signin_metrics::SignoutDelete::IGNORE_METRIC);
}

base::android::ScopedJavaLocalRef<jstring>
SigninManagerAndroid::GetManagementDomain(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  base::android::ScopedJavaLocalRef<jstring> domain;

  policy::UserCloudPolicyManager* manager =
      policy::UserCloudPolicyManagerFactory::GetForBrowserContext(profile_);
  policy::CloudPolicyStore* store = manager->core()->store();

  if (store && store->is_managed() && store->policy()->has_username()) {
    domain.Reset(
        base::android::ConvertUTF8ToJavaString(
            env, gaia::ExtractDomainName(store->policy()->username())));
  }

  return domain;
}

void SigninManagerAndroid::WipeProfileData(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  WipeData(profile_, true /* all data */,
           base::Bind(&SigninManagerAndroid::OnBrowsingDataRemoverDone,
                      weak_factory_.GetWeakPtr()));
}

void SigninManagerAndroid::WipeGoogleServiceWorkerCaches(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  WipeData(profile_, false /* only Google service worker caches */,
           base::Bind(&SigninManagerAndroid::OnBrowsingDataRemoverDone,
                      weak_factory_.GetWeakPtr()));
}

void SigninManagerAndroid::OnPolicyRegisterDone(
    const std::string& dm_token,
    const std::string& client_id) {
  dm_token_ = dm_token;
  client_id_ = client_id;

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> domain;
  if (!dm_token_.empty()) {
    DCHECK(!username_.empty());
    domain.Reset(
        base::android::ConvertUTF8ToJavaString(
            env, gaia::ExtractDomainName(username_)));
  } else {
    username_.clear();
  }

  Java_SigninManager_onPolicyCheckedBeforeSignIn(env, java_signin_manager_,
                                                 domain);
}

void SigninManagerAndroid::OnPolicyFetchDone(bool success) {
  Java_SigninManager_onPolicyFetchedBeforeSignIn(
      base::android::AttachCurrentThread(), java_signin_manager_);
}

void SigninManagerAndroid::OnBrowsingDataRemoverDone() {
  Java_SigninManager_onProfileDataWiped(base::android::AttachCurrentThread(),
                                        java_signin_manager_);
}

void SigninManagerAndroid::ClearLastSignedInUser(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  ClearLastSignedInUserForProfile(profile_);
}

void SigninManagerAndroid::LogInSignedInUser(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile_);
  // With the account consistency enabled let the account Reconcilor handles
  // everything.
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  const std::string& primary_acct = signin_manager->GetAuthenticatedAccountId();

  static_cast<OAuth2TokenServiceDelegateAndroid*>(token_service->GetDelegate())
      ->ValidateAccounts(primary_acct, true);
}

jboolean SigninManagerAndroid::IsSigninAllowedByPolicy(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return SigninManagerFactory::GetForProfile(profile_)->IsSigninAllowed();
}

jboolean SigninManagerAndroid::IsForceSigninEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  // prefs::kForceBrowserSignin is set in Local State, not in user prefs.
  PrefService* prefs = g_browser_process->local_state();
  return prefs->GetBoolean(prefs::kForceBrowserSignin);
}

jboolean SigninManagerAndroid::IsSignedInOnNative(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return SigninManagerFactory::GetForProfile(profile_)->IsAuthenticated();
}

void SigninManagerAndroid::GoogleSigninFailed(
    const GoogleServiceAuthError& error) {}

void SigninManagerAndroid::GoogleSigninSucceeded(const std::string& account_id,
                                                 const std::string& username) {}

void SigninManagerAndroid::GoogleSignedOut(const std::string& account_id,
                                           const std::string& username) {
  DCHECK(thread_checker_.CalledOnValidThread());
  Java_SigninManager_onNativeSignOut(base::android::AttachCurrentThread(),
                                     java_signin_manager_);
}

void SigninManagerAndroid::OnSigninAllowedPrefChanged() {
  Java_SigninManager_onSigninAllowedByPolicyChanged(
      base::android::AttachCurrentThread(), java_signin_manager_,
      SigninManagerFactory::GetForProfile(profile_)->IsSigninAllowed());
}

// static
void SigninManagerAndroid::WipeData(Profile* profile,
                                    bool all_data,
                                    base::OnceClosure callback) {
  // The ProfileDataRemover deletes itself once done.
  new ProfileDataRemover(profile, all_data, std::move(callback));
}

static jlong JNI_SigninManager_Init(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj) {
  SigninManagerAndroid* signin_manager_android =
      new SigninManagerAndroid(env, obj);
  return reinterpret_cast<intptr_t>(signin_manager_android);
}

static jboolean JNI_SigninManager_ShouldLoadPolicyForUser(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& j_username) {
  std::string username =
      base::android::ConvertJavaStringToUTF8(env, j_username);
  return !policy::BrowserPolicyConnector::IsNonEnterpriseUser(username);
}

static void JNI_SigninManager_IsUserManaged(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& j_username,
    const JavaParamRef<jobject>& j_callback) {
  base::android::ScopedJavaGlobalRef<jobject> callback(env, j_callback);

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::string username =
      base::android::ConvertJavaStringToUTF8(env, j_username);
  policy::UserPolicySigninService* service =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile);
  service->RegisterForPolicyWithAccountId(
      username,
      AccountTrackerServiceFactory::GetForProfile(profile)
          ->FindAccountInfoByEmail(username)
          .account_id,
      base::Bind(&UserManagementDomainFetched, callback));
}

base::android::ScopedJavaLocalRef<jstring> JNI_SigninManager_ExtractDomainName(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& j_email) {
  std::string email = base::android::ConvertJavaStringToUTF8(env, j_email);
  std::string domain = gaia::ExtractDomainName(email);
  return base::android::ConvertUTF8ToJavaString(env, domain);
}
