// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_H_
#define CHROME_BROWSER_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/cancelable_callback.h"
#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/skia/include/core/SkColor.h"

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

struct AccountInfo;
class Browser;
class DiceSignedInProfileCreator;
class DiceInterceptedSessionStartupHelper;
class Profile;
class ProfileAttributesEntry;
class ProfileAttributesStorage;

// Outcome of the interception heuristic (decision whether the interception
// bubble is shown or not).
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SigninInterceptionHeuristicOutcome {
  // Interception succeeded:
  kInterceptProfileSwitch = 0,
  kInterceptMultiUser = 1,
  kInterceptEnterprise = 2,

  // Interception aborted:
  // This is a "Sync" sign in and not a "web" sign in.
  kAbortSyncSignin = 3,
  // Another interception is already in progress.
  kAbortInterceptInProgress = 4,
  // This is not a new account (reauth).
  kAbortAccountNotNew = 5,
  // New profile is not offered when there is only one account.
  kAbortSingleAccount = 6,
  // Extended account info could not be downloaded.
  kAbortAccountInfoTimeout = 7,
  // Account info not compatible with interception (e.g. same Gaia name).
  kAbortAccountInfoNotCompatible = 8,
  // Profile creation disallowed.
  kAbortProfileCreationDisallowed = 9,
  // The interceptor was shut down before the heuristic completed.
  kAbortShutdown = 10,
  // The interceptor is not offered when WebContents has no browser associated.
  kAbortNoBrowser = 11,
  // A password update is required for the account, and this takes priority over
  // signin interception.
  kAbortPasswordUpdate = 12,
  // A password update will be required for the account: the password used on
  // the form does not match the stored password.
  kAbortPasswordUpdatePending = 13,
  // The user already declined a new profile for this account, the UI is not
  // shown again.
  kAbortUserDeclinedProfileForAccount = 14,
  // Signin interception is disabled by the SigninInterceptionEnabled policy.
  kAbortInterceptionDisabled = 15,

  kMaxValue = kAbortInterceptionDisabled,
};

// User selection in the interception bubble.
enum class SigninInterceptionUserChoice { kAccept, kDecline, kGuest };

// User action resulting from the interception bubble.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SigninInterceptionResult {
  kAccepted = 0,
  kDeclined = 1,
  kIgnored = 2,

  // Used when the bubble was not shown because it's not implemented.
  kNotDisplayed = 3,

  // Accepted to be opened in Guest profile.
  kAcceptedWithGuest = 4,

  kMaxValue = kAcceptedWithGuest,
};

// The ScopedDiceWebSigninInterceptionBubbleHandle closes the signin intercept
// bubble when it is destroyed, if the bubble is still opened. Note that this
// handle does not prevent the bubble from being closed for other reasons.
class ScopedDiceWebSigninInterceptionBubbleHandle {
 public:
  virtual ~ScopedDiceWebSigninInterceptionBubbleHandle() = 0;
};

// Returns whether the heuristic outcome is a success (the signin should be
// intercepted).
bool SigninInterceptionHeuristicOutcomeIsSuccess(
    SigninInterceptionHeuristicOutcome outcome);

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
                                 public content::WebContentsObserver,
                                 public signin::IdentityManager::Observer {
 public:
  enum class SigninInterceptionType { kProfileSwitch, kEnterprise, kMultiUser };

  // Delegate class responsible for showing the various interception UIs.
  class Delegate {
   public:
    // Parameters for interception bubble UIs.
    struct BubbleParameters {
      SigninInterceptionType interception_type;
      AccountInfo intercepted_account;
      AccountInfo primary_account;
      SkColor profile_highlight_color;
      bool show_guest_option;
    };

    virtual ~Delegate() = default;

    // Shows the signin interception bubble and calls |callback| to indicate
    // whether the user should continue in a new profile.
    // The callback is never called if the delegate is deleted before it
    // completes.
    // May return a nullptr handle if the bubble cannot be shown.
    // Warning: the handle closes the bubble when it is destroyed ; it is the
    // responsibility of the caller to keep the handle alive until the bubble
    // should be closed.
    // The callback must not be called synchronously if this function returns a
    // valid handle (because the caller needs to be able to close the bubble
    // from the callback).
    virtual std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle>
    ShowSigninInterceptionBubble(
        content::WebContents* web_contents,
        const BubbleParameters& bubble_parameters,
        base::OnceCallback<void(SigninInterceptionResult)> callback) = 0;

    // Shows the profile customization bubble.
    virtual void ShowProfileCustomizationBubble(Browser* browser) = 0;
  };

  DiceWebSigninInterceptor(Profile* profile,
                           std::unique_ptr<Delegate> delegate);
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
      std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle>
          bubble_handle,
      bool is_new_profile);

  // Returns the outcome of the interception heuristic.
  // If the outcome is kInterceptProfileSwitch, the target profile is returned
  // in |entry|.
  // In some cases the outcome cannot be fully computed synchronously, when this
  // happens, the signin interception is highly likely (but not guaranteed).
  base::Optional<SigninInterceptionHeuristicOutcome> GetHeuristicOutcome(
      bool is_new_account,
      bool is_sync_signin,
      const std::string& email,
      const ProfileAttributesEntry** entry = nullptr) const;

  // Returns true if the interception is in progress (running the heuristic or
  // showing on screen).
  bool is_interception_in_progress() const {
    return is_interception_in_progress_;
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

  // Cancels any current signin interception and resets the interceptor to its
  // initial state.
  void Reset();

  // Helper functions to determine which interception UI should be shown.
  const ProfileAttributesEntry* ShouldShowProfileSwitchBubble(
      const std::string& intercepted_email,
      ProfileAttributesStorage* profile_attribute_storage) const;
  bool ShouldShowEnterpriseBubble(
      const AccountInfo& intercepted_account_info) const;
  bool ShouldShowMultiUserBubble(
      const AccountInfo& intercepted_account_info) const;

  // signin::IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // Called when the extended account info was not updated after a timeout.
  void OnExtendedAccountInfoFetchTimeout();

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
  void OnNewSignedInProfileCreated(base::Optional<SkColor> profile_color,
                                   Profile* new_profile);

  // Called when the new browser is created after interception. Passed as
  // callback to `session_startup_helper_`.
  void OnNewBrowserCreated(bool is_new_profile);

  // Returns a 8-bit hash of the email that can be persisted.
  static std::string GetPersistentEmailHash(const std::string& email);

  // Should be called when the user declines profile creation or profile switch,
  // in order to remember their decision. This information is stored in prefs.
  // Only a hash of the email is saved, as Chrome does not need to store the
  // actual email, but only need to compare emails. The hash has low entropy to
  // ensure it cannot be reversed.
  void RecordProfileCreationDeclined(const std::string& email);
  void RecordProfileSwitchDeclined(const std::string& email);

  // Checks if the user previously declined 2 times creating a new profile for
  // this account.
  bool HasUserDeclinedProfileCreation(const std::string& email) const;

  // Checks if the user previously declined more than a threshold number of
  // times switching to a new profile for this account. The limit is set up
  // via an experiment parameter.
  bool HasUserDeclinedProfileSwitch(const std::string& email) const;

  Profile* const profile_;
  signin::IdentityManager* const identity_manager_;
  std::unique_ptr<Delegate> delegate_;

  // Used in the profile that was created after the interception succeeded.
  std::unique_ptr<DiceInterceptedSessionStartupHelper> session_startup_helper_;

  // Members below are related to the interception in progress.
  bool is_interception_in_progress_ = false;
  CoreAccountId account_id_;
  ScopedObserver<signin::IdentityManager, signin::IdentityManager::Observer>
      account_info_update_observer_{this};
  // Timeout for the fetch of the extended account info. The signin interception
  // is cancelled if the account info cannot be fetched quickly.
  base::CancelableOnceCallback<void()> on_account_info_update_timeout_;
  std::unique_ptr<DiceSignedInProfileCreator> dice_signed_in_profile_creator_;
  // Used to retain the interception UI bubble until profile creation completes.
  std::unique_ptr<ScopedDiceWebSigninInterceptionBubbleHandle>
      interception_bubble_handle_;
  // Used for metrics:
  bool was_interception_ui_displayed_ = false;
  base::TimeTicks account_info_fetch_start_time_;
  base::TimeTicks profile_creation_start_time_;
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_H_
