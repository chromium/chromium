// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_H_
#define CHROME_BROWSER_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/signin/web_signin_interceptor.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_ui.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/signin/profile_separation_policies.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace policy {
class UserCloudSigninRestrictionPolicyFetcher;
}
namespace user_prefs {
class PrefRegistrySyncable;
}

struct AccountInfo;
class DiceSignedInProfileCreator;
class DiceInterceptedSessionStartupHelper;
class Profile;
class ProfileAttributesEntry;
class ProfileAttributesStorage;

// Called after web signed in, after a successful token exchange through Dice.
// The DiceWebSigninInterceptor may offer the user to create a new profile or
// switch to another existing profile.
//
// Implementation notes: here is how an entire interception flow work for the
// enterprise or multi-user case:
// * MaybeInterceptWebSignin() is called when the new signin happens.
// * Wait until the account info is downloaded.
// * Interception UI is shown by the delegate. Keep a handle on the bubble.
// * If the user approved, a new profile is created and the token is moved from
//   this profile to the new profile, using DiceSignedInProfileCreator.
// * At this point, the flow ends in this profile, and continues in the new
//   profile using DiceInterceptedSessionStartupHelper to add the account.
// * When the account is available on the web in the new profile:
//   - A new browser window is created for the new profile,
//   - The tab is moved to the new profile,
//   - The interception bubble is closed by deleting the handle,
//   - The profile customization bubble is shown.
class DiceWebSigninInterceptor : public KeyedService,
                                 public signin::IdentityManager::Observer {
 public:
  DiceWebSigninInterceptor(
      Profile* profile,
      std::unique_ptr<WebSigninInterceptor::Delegate> delegate);
  ~DiceWebSigninInterceptor() override;

  DiceWebSigninInterceptor(const DiceWebSigninInterceptor&) = delete;
  DiceWebSigninInterceptor& operator=(const DiceWebSigninInterceptor&) = delete;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Called when an account has been added in Chrome from the web (using the
  // DICE protocol).
  // |web_contents| is the tab where the signin event happened. It must belong
  // to the profile associated with this service. It may be nullptr if the tab
  // was closed.
  // |is_new_account| is true if the account was not already in Chrome (i.e.
  // this is not a reauth).
  // |is_sync_signin| is true if the user is signing in with the intent of
  // enabling sync for that account.
  // Virtual for testing.
  virtual void MaybeInterceptWebSignin(content::WebContents* web_contents,
                                       CoreAccountId account_id,
                                       bool is_new_account,
                                       bool is_sync_signin);

  // Called after the new profile was created during a signin interception.
  // The token has been moved to the new profile, but the account is not yet in
  // the cookies.
  // `intercepted_contents` may be null if the tab was already closed.
  // The intercepted web contents belong to the source profile (which is not the
  // profile attached to this service).
  void CreateBrowserAfterSigninInterception(
      CoreAccountId account_id,
      content::WebContents* intercepted_contents,
      std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle> bubble_handle,
      bool is_new_profile,
      WebSigninInterceptor::SigninInterceptionType interception_type);

  // Returns the outcome of the interception heuristic.
  // If the outcome is kInterceptProfileSwitch, the target profile is returned
  // in |entry|.
  // In some cases the outcome cannot be fully computed synchronously, when this
  // happens, the signin interception is highly likely (but not guaranteed).
  absl::optional<SigninInterceptionHeuristicOutcome> GetHeuristicOutcome(
      bool is_new_account,
      bool is_sync_signin,
      const std::string& email,
      const ProfileAttributesEntry** entry = nullptr) const;

  // Returns true if the interception is in progress (running the heuristic or
  // showing on screen).
  bool is_interception_in_progress() const {
    return is_interception_in_progress_;
  }

  void SetInterceptedAccountProfileSeparationPoliciesForTesting(
      absl::optional<policy::ProfileSeparationPolicies> value) {
    intercepted_account_profile_separation_policies_for_testing_ =
        std::move(value);
  }

  // KeyedService:
  void Shutdown() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptorTest,
                           ShouldShowProfileSwitchBubble);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptorTest,
                           NoBubbleWithSingleAccount);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptorTest,
                           ShouldShowEnterpriseBubble);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptorTest,
                           ShouldShowEnterpriseBubbleWithoutUPA);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptorTest,
                           ShouldShowMultiUserBubble);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptorTest, PersistentHash);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptorTest,
                           ShouldEnforceEnterpriseProfileSeparation);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptorTest,
                           ShouldEnforceEnterpriseProfileSeparationWithoutUPA);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptorTest,
                           ShouldEnforceEnterpriseProfileSeparationReauth);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptorTest,
                           EnforceManagedAccountAsPrimary);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptorTest,
                           ShouldEnforceEnterpriseProfileSeparationReauth);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptorTest,
                           ForcedEnterpriseInterceptionTestAccountLevelPolicy);
  FRIEND_TEST_ALL_PREFIXES(
      DiceWebSigninInterceptorTest,
      ForcedEnterpriseInterceptionTestNoForcedInterception);

  // Cancels any current signin interception and resets the interceptor to its
  // initial state.
  void Reset();

  // Helper functions to determine which interception UI should be shown.
  const ProfileAttributesEntry* ShouldShowProfileSwitchBubble(
      const std::string& intercepted_email,
      ProfileAttributesStorage* profile_attribute_storage) const;
  bool ShouldEnforceEnterpriseProfileSeparation(
      const AccountInfo& intercepted_account_info) const;
  bool ShouldShowEnterpriseDialog(
      const AccountInfo& intercepted_account_info) const;
  bool ShouldShowEnterpriseBubble(
      const AccountInfo& intercepted_account_info) const;
  bool ShouldShowMultiUserBubble(
      const AccountInfo& intercepted_account_info) const;

  // Helper function to call `delegate_->ShowSigninInterceptionBubble()`.
  void ShowSigninInterceptionBubble(
      const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
      base::OnceCallback<void(SigninInterceptionResult)> callback);

  // Ensure that we are observing changes in extended account info. Idempotent.
  void EnsureObservingExtendedAccountInfo();

  // Can be called at any time, and will either process the interception or
  // register the required observers and wait for async operations to complete.
  void ProcessInterceptionOrWait(const AccountInfo& info, bool timed_out);

  void OnInterceptionReadyToBeProcessed(const AccountInfo& info);

  // signin::IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // Called when one or more of the async info fetches times out.
  void OnInterceptionInfoFetchTimeout();

  // Called after the user chose whether a new profile would be created.
  void OnProfileCreationChoice(const AccountInfo& account_info,
                               SkColor profile_color,
                               SigninInterceptionResult create);
  // Called after the user chose whether the session should continue in a new
  // profile.
  void OnProfileSwitchChoice(const std::string& email,
                             const base::FilePath& profile_path,
                             SigninInterceptionResult switch_profile);

  // Called when the new profile is created or loaded from disk.
  // `profile_color` is set as theme color for the profile ; it should be
  // nullopt if the profile is not new (loaded from disk).
  void OnNewSignedInProfileCreated(absl::optional<SkColor> profile_color,
                                   Profile* new_profile);

  // Called after the user choses whether the session should continue in a new
  // work profile or not. If the user choses not to continue in a work profile,
  // the account is signed out.
  void OnEnterpriseProfileCreationResult(const AccountInfo& account_info,
                                         SkColor profile_color,
                                         SigninInterceptionResult create);

  // Called when the new browser is created after interception. Passed as
  // callback to `session_startup_helper_`.
  void OnNewBrowserCreated(bool is_new_profile);

  // Returns a 8-bit hash of the email that can be persisted.
  static std::string GetPersistentEmailHash(const std::string& email);

  // Should be called when the user declines profile creation, in order to
  // remember their decision. This information is stored in prefs. Only a hash
  // of the email is saved, as Chrome does not need to store the actual email,
  // but only need to compare emails. The hash has low entropy to ensure it
  // cannot be reversed.
  void RecordProfileCreationDeclined(const std::string& email);

  // Checks if the user previously declined 2 times creating a new profile for
  // this account.
  bool HasUserDeclinedProfileCreation(const std::string& email) const;

  // Fetches the value of the cloud user level value of the
  // ManagedAccountsSigninRestriction policy for 'account_info' and runs
  // `callback` with the result. This is a network call that has a 5 seconds
  // timeout.
  void EnsureAccountLevelSigninRestrictionFetchInProgress(
      const AccountInfo& account_info,
      base::OnceCallback<void(const policy::ProfileSeparationPolicies&)>
          callback);

  // Called when the the value of the cloud user level value of the
  // ManagedAccountsSigninRestriction is received.
  void OnAccountLevelManagedAccountsSigninRestrictionReceived(
      const AccountInfo& account_info,
      const policy::ProfileSeparationPolicies& profile_separation_policies);

  // Returns true if enterprise separation is required.
  // Returns false is enterprise separation is not required.
  // Returns no value if info is required to determine if enterprise separation
  // is required. If `profile_separation_policies` is `absl::nullopt` then the
  // user cloud profile separation policies have not yet been fetched.
  absl::optional<bool> EnterpriseSeparationMaybeRequired(
      const std::string& email,
      bool is_new_account_interception,
      const absl::optional<policy::ProfileSeparationPolicies>&
          profile_separation_policies) const;

  // Records the heuristic outcome and latency metrics.
  void RecordSigninInterceptionHeuristicOutcome(
      SigninInterceptionHeuristicOutcome outcome) const;

  // Returns true if we have the minimum extended account information needed to
  // make a best-effort intercept heuristic decision. If we fail to retrieve
  // this information we will cancel the interception completely.
  // Returns false otherwise.
  bool IsRequiredExtendedAccountInfoAvailable(
      const AccountInfo& account_info) const;

  // Returns true if we have all the extended account information which might
  // factor in to the intercept heuristic. If we don't have 'Full' information,
  // but do have the 'Required' information above, we will make a best-effort
  // decision based on sensible defaults.
  // Returns false otherwise.
  bool IsFullExtendedAccountInfoAvailable(
      const AccountInfo& account_info) const;

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  const raw_ptr<signin::IdentityManager, DanglingUntriaged> identity_manager_;
  std::unique_ptr<WebSigninInterceptor::Delegate> delegate_;

  // Used in the profile that was created after the interception succeeded.
  std::unique_ptr<DiceInterceptedSessionStartupHelper> session_startup_helper_;

  // Members below are related to the interception in progress.
  base::WeakPtr<content::WebContents> web_contents_;
  bool is_interception_in_progress_ = false;
  CoreAccountId account_id_;
  bool new_account_interception_ = false;
  bool intercepted_account_management_accepted_ = false;
  absl::optional<WebSigninInterceptor::SigninInterceptionType>
      interception_type_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      account_info_update_observation_{this};

  // Timeout for waiting for full information to be available (see
  // `ProcessInterceptionOrWait()`).
  base::CancelableOnceCallback<void()> interception_info_available_timeout_;

  std::unique_ptr<DiceSignedInProfileCreator> dice_signed_in_profile_creator_;
  // Used to retain the interception UI bubble until profile creation completes.
  std::unique_ptr<ScopedWebSigninInterceptionBubbleHandle>
      interception_bubble_handle_;

  // Used for metrics.
  base::TimeTicks interception_start_time_;
  bool was_interception_ui_displayed_ = false;

  // Used to fetch the cloud user level policy value of
  // ManagedAccountsSigninRestriction. This can only fetch one policy value for
  // one account at the time.
  std::unique_ptr<policy::UserCloudSigninRestrictionPolicyFetcher>
      account_level_signin_restriction_policy_fetcher_;
  // Value of the ManagedAccountsSigninRestriction for the intercepted account.
  // If no value is set, then we have not yet received the policy value.
  absl::optional<policy::ProfileSeparationPolicies>
      intercepted_account_profile_separation_policies_;
  absl::optional<policy::ProfileSeparationPolicies>
      intercepted_account_profile_separation_policies_for_testing_;
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_H_
