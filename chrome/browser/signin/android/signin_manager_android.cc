// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/android/signin_manager_android.h"

#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_mobile.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_id_from_account_info.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/google/core/common/google_util.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/service/sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_auth_util.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SigninManagerImpl_jni.h"

using base::android::JavaParamRef;

namespace {

// The cache expiration time for IsAccountManaged(), i.e. the maximum time
// interval between two calls to IsAccountManaged() where the second may return
// the cached outcome of the first (for the same user).
constexpr base::TimeDelta kIsAccountManagedCacheExpirationTime =
    base::Minutes(1);

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
        origin_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        remover_(profile->GetBrowsingDataRemover()) {
    remover_->AddObserver(this);

    if (all_data) {
      chrome_browsing_data_remover::DataType removed_types =
          chrome_browsing_data_remover::ALL_DATA_TYPES;
      if (password_manager::UsesSplitStoresAndUPMForLocal(
              profile_->GetPrefs())) {
        // If usesSplitStoresAndUPMForLocal() is true, browser sign-in won't
        // upload existing passwords, so there's no reason to wipe them
        // immediately before. Similarly, on browser sign-out, account passwords
        // should survive (outside of the browser) to be used by other apps,
        // until system-level sign-out. In other words, the browser has no
        // business deleting any passwords here.
        removed_types &= ~chrome_browsing_data_remover::DATA_TYPE_PASSWORDS;
      }
      remover_->RemoveAndReply(base::Time(), base::Time::Max(), removed_types,
                               chrome_browsing_data_remover::ALL_ORIGIN_TYPES,
                               this);
    } else {
      std::unique_ptr<content::BrowsingDataFilterBuilder> google_tld_filter =
          content::BrowsingDataFilterBuilder::Create(
              content::BrowsingDataFilterBuilder::Mode::kDelete);

      // TODO(msramek): BrowsingDataFilterBuilder was not designed for
      // large filters. Optimize it.
      for (const std::string& domain :
           google_util::GetGoogleRegistrableDomains()) {
        google_tld_filter->AddRegisterableDomain(domain);
      }

      remover_->RemoveWithFilterAndReply(
          base::Time(), base::Time::Max(),
          content::BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE,
          chrome_browsing_data_remover::ALL_ORIGIN_TYPES,
          std::move(google_tld_filter), this);
    }
  }

  ProfileDataRemover(const ProfileDataRemover&) = delete;
  ProfileDataRemover& operator=(const ProfileDataRemover&) = delete;

  ~ProfileDataRemover() override {}

  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    remover_->RemoveObserver(this);

    if (all_data_) {
      // All the Profile data has been wiped. Clear the last signed in username
      // as well, so that the next signin doesn't trigger the account
      // change dialog.
      profile_->GetPrefs()->ClearPref(prefs::kGoogleServicesLastSyncingGaiaId);
      profile_->GetPrefs()->ClearPref(
          prefs::kGoogleServicesLastSyncingUsername);
      profile_->GetPrefs()->ClearPref(
          prefs::kGoogleServicesLastSignedInUsername);
    }

    origin_runner_->PostTask(FROM_HERE, std::move(callback_));
    origin_runner_->DeleteSoon(FROM_HERE, this);
  }

 private:
  raw_ptr<Profile> profile_;
  bool all_data_;
  base::OnceClosure callback_;
  scoped_refptr<base::SingleThreadTaskRunner> origin_runner_;
  raw_ptr<content::BrowsingDataRemover> remover_;
};

// Returns whether the user *may* be a managed user.
bool ShouldLoadPolicyForUser(const std::string& username) {
  return signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
      username);
}

}  // namespace

SigninManagerAndroid::SigninManagerAndroid(
    Profile* profile,
    signin::IdentityManager* identity_manager)
    : profile_(profile),
      identity_manager_(identity_manager),
      user_cloud_policy_manager_(profile_->GetUserCloudPolicyManager()),
      user_policy_signin_service_(
          policy::UserPolicySigninServiceFactory::GetForProfile(profile_)),
      weak_factory_(this) {
  DCHECK(profile_);
  DCHECK(identity_manager_);
  DCHECK(user_cloud_policy_manager_);
  DCHECK(user_policy_signin_service_);

  signin_allowed_.Init(
      prefs::kSigninAllowed, profile_->GetPrefs(),
      base::BindRepeating(&SigninManagerAndroid::OnSigninAllowedPrefChanged,
                          base::Unretained(this)));

  force_browser_signin_.Init(prefs::kForceBrowserSignin,
                             g_browser_process->local_state());

  java_signin_manager_ = Java_SigninManagerImpl_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this),
      profile_->GetJavaObject(), identity_manager_->GetJavaObject(),
      identity_manager_->GetIdentityMutatorJavaObject(),
      SyncServiceFactory::GetForProfile(profile_)->GetJavaObject());
}

base::android::ScopedJavaLocalRef<jobject>
SigninManagerAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_signin_manager_);
}

SigninManagerAndroid::~SigninManagerAndroid() {}

void SigninManagerAndroid::Shutdown() {
  Java_SigninManagerImpl_destroy(base::android::AttachCurrentThread(),
                                 java_signin_manager_);
}

SigninManagerAndroid::ManagementCredentials::ManagementCredentials(
    const std::string& dm_token,
    const std::string& client_id,
    const std::vector<std::string>& user_affiliation_ids)
    : dm_token(dm_token),
      client_id(client_id),
      user_affiliation_ids(user_affiliation_ids) {}

SigninManagerAndroid::ManagementCredentials::~ManagementCredentials() = default;

bool SigninManagerAndroid::IsSigninAllowed() const {
  return signin_allowed_.GetValue();
}

bool SigninManagerAndroid::IsSigninAllowedByPolicy(JNIEnv* env) const {
  return IsSigninAllowed();
}

bool SigninManagerAndroid::IsForceSigninEnabled(JNIEnv* env) {
  return force_browser_signin_.GetValue();
}

// static
bool SigninManagerAndroid::MatchesCachedIsAccountManagedEntry(
    const CachedIsAccountManaged& cached_entry,
    const CoreAccountInfo& account) {
  return cached_entry.gaia_id == account.gaia &&
         cached_entry.expiration_time > base::Time::Now();
}

void SigninManagerAndroid::OnSigninAllowedPrefChanged() const {
  VLOG(1) << "::OnSigninAllowedPrefChanged() " << IsSigninAllowed();
  Java_SigninManagerImpl_onSigninAllowedByPolicyChanged(
      base::android::AttachCurrentThread(), java_signin_manager_,
      IsSigninAllowed());
}

void SigninManagerAndroid::StopApplyingCloudPolicy(JNIEnv* env) {
  user_policy_signin_service_->ShutdownCloudPolicyManager();
}

void SigninManagerAndroid::RegisterPolicyWithAccount(
    const CoreAccountInfo& account,
    RegisterPolicyWithAccountCallback callback) {
  if (!ShouldLoadPolicyForUser(account.email)) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  user_policy_signin_service_->RegisterForPolicyWithAccountId(
      account.email, account.account_id,
      base::BindOnce(
          [](RegisterPolicyWithAccountCallback callback,
             const std::string& dm_token, const std::string& client_id,
             const std::vector<std::string>& user_affiliation_ids) {
            std::optional<ManagementCredentials> credentials;
            if (!dm_token.empty()) {
              credentials.emplace(dm_token, client_id, user_affiliation_ids);
            }
            std::move(callback).Run(credentials);
          },
          std::move(callback)));
}

void SigninManagerAndroid::FetchAndApplyCloudPolicy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_account_info,
    const base::RepeatingClosure& callback) {
  CoreAccountInfo account = ConvertFromJavaCoreAccountInfo(env, j_account_info);
  RegisterPolicyWithAccount(
      account,
      base::BindOnce(&SigninManagerAndroid::OnPolicyRegisterDone,
                     weak_factory_.GetWeakPtr(), account, std::move(callback)));
}

void SigninManagerAndroid::OnPolicyRegisterDone(
    const CoreAccountInfo& account,
    base::OnceCallback<void()> policy_callback,
    const std::optional<ManagementCredentials>& credentials) {
  if (credentials) {
    FetchPolicyBeforeSignIn(account, std::move(policy_callback),
                            credentials.value());
  } else {
    // User's account does not have a policy to fetch.
    std::move(policy_callback).Run();
  }
}

void SigninManagerAndroid::FetchPolicyBeforeSignIn(
    const CoreAccountInfo& account,
    base::OnceCallback<void()> policy_callback,
    const ManagementCredentials& credentials) {
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();
  user_policy_signin_service_->FetchPolicyForSignedInUser(
      AccountIdFromAccountInfo(account), credentials.dm_token,
      credentials.client_id, credentials.user_affiliation_ids,
      url_loader_factory,
      base::BindOnce([](base::OnceCallback<void()> callback,
                        bool success) { std::move(callback).Run(); },
                     std::move(policy_callback)));
}

void SigninManagerAndroid::IsAccountManaged(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_account_info,
    const JavaParamRef<jobject>& j_callback) {
  base::Time start_time = base::Time::Now();
  CoreAccountInfo account = ConvertFromJavaCoreAccountInfo(env, j_account_info);
  base::android::ScopedJavaGlobalRef<jobject> callback(env, j_callback);

  if (cached_is_account_managed_.has_value() &&
      MatchesCachedIsAccountManagedEntry(*cached_is_account_managed_,
                                         account)) {
    // Cache hit, return cached value without issuing any request.
    bool is_managed = cached_is_account_managed_->is_account_managed;
    base::android::RunBooleanCallbackAndroid(callback, is_managed);
    return;
  }

  RegisterPolicyWithAccount(
      account,
      base::BindOnce(
          &SigninManagerAndroid::OnPolicyRegisterDoneForIsAccountManaged,
          weak_factory_.GetWeakPtr(), account, std::move(callback),
          start_time));
}

void SigninManagerAndroid::OnPolicyRegisterDoneForIsAccountManaged(
    const CoreAccountInfo& account,
    base::android::ScopedJavaGlobalRef<jobject> callback,
    base::Time start_time,
    const std::optional<ManagementCredentials>& credentials) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Signin.Android.IsAccountManagedDuration",
                             (base::Time::Now() - start_time));

  bool is_managed = credentials.has_value();
  // Cache result in case IsAccountManaged() is invoked again for the same user.
  cached_is_account_managed_.emplace(
      account.gaia, is_managed,
      base::Time::Now() + kIsAccountManagedCacheExpirationTime);
  base::android::RunBooleanCallbackAndroid(callback, is_managed);
}

base::android::ScopedJavaLocalRef<jstring>
SigninManagerAndroid::GetManagementDomain(JNIEnv* env) {
  base::android::ScopedJavaLocalRef<jstring> domain;

  policy::CloudPolicyStore* store = user_cloud_policy_manager_->core()->store();

  if (store && store->is_managed() && store->policy()->has_username()) {
    domain.Reset(base::android::ConvertUTF8ToJavaString(
        env, gaia::ExtractDomainName(store->policy()->username())));
  }

  return domain;
}

void SigninManagerAndroid::WipeProfileData(
    JNIEnv* env,
    const base::RepeatingClosure& callback) {
  WipeData(profile_, true /* all data */, callback);
}

void SigninManagerAndroid::WipeGoogleServiceWorkerCaches(
    JNIEnv* env,
    const base::RepeatingClosure& callback) {
  WipeData(profile_, false /* only Google service worker caches */, callback);
}

// static
void SigninManagerAndroid::WipeData(Profile* profile,
                                    bool all_data,
                                    base::OnceClosure callback) {
  // The ProfileDataRemover deletes itself once done.
  new ProfileDataRemover(profile, all_data, std::move(callback));
}

std::string JNI_SigninManagerImpl_ExtractDomainName(JNIEnv* env,
                                                    std::string& email) {
  return gaia::ExtractDomainName(email);
}

void SigninManagerAndroid::SetUserAcceptedAccountManagement(
    JNIEnv* env,
    bool accepted_account_management) {
  enterprise_util::SetUserAcceptedAccountManagement(
      profile_, accepted_account_management);
}

bool SigninManagerAndroid::GetUserAcceptedAccountManagement(JNIEnv* env) {
  return enterprise_util::UserAcceptedAccountManagement(profile_);
}
