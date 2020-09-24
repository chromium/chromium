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

  kMaxValue = kAbortShutdown,
};

// Called after web signed in, after a successful token exchange through Dice.
// The DiceWebSigninInterceptor may offer the user to create a new profile or
// switch to another existing profile.
//
// Implementation notes: here is how an entire interception flow work for the
// enterprise or multi-user case:
// * MaybeInterceptSignin() is called when the new signin happens.
// * Wait until the account info is downloaded.
// * Interception UI is shown by the delegate.
// * If the user approved, a new profile is created and the token is moved from
//   this profile to the new profile, using DiceSignedInProfileCreator.
// * At this point, the flow ends in this profile, and continues in the new
//   profile using DiceInterceptedSessionStartupHelper.
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
    };

    virtual ~Delegate() = default;

    // Shows the signin interception bubble and calls |callback| to indicate
    // whether the user should continue in a new profile.
    // The callback is never called if the delegate is deleted before it
    // completes.
    virtual void ShowSigninInterceptionBubble(
        content::WebContents* web_contents,
        const BubbleParameters& bubble_parameters,
        base::OnceCallback<void(bool)> callback) = 0;

    // Shows the profile customization bubble.
    virtual void ShowProfileCustomizationBubble(Browser* browser) = 0;
  };

  DiceWebSigninInterceptor(Profile* profile,
                           std::unique_ptr<Delegate> delegate);
  ~DiceWebSigninInterceptor() override;

  DiceWebSigninInterceptor(const DiceWebSigninInterceptor&) = delete;
  DiceWebSigninInterceptor& operator=(const DiceWebSigninInterceptor&) = delete;

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
  // `show_customization_bubble` indicates whether the customization bubble
  // should be shown after the browser is opened.
  void CreateBrowserAfterSigninInterception(
      CoreAccountId account_id,
      content::WebContents* intercepted_contents,
      bool show_customization_bubble);

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
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptorTest,
                           InterceptionInProgress);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptorTest,
                           NoInterceptionWithOneAccount);
  FRIEND_TEST_ALL_PREFIXES(DiceWebSigninInterceptorTest,
                           ProfileCreationDisallowed);

  // Cancels any current signin interception and resets the interceptor to its
  // initial state.
  void Reset();

  // Helper functions to determine which interception UI should be shown.
  const ProfileAttributesEntry* ShouldShowProfileSwitchBubble(
      const CoreAccountInfo& intercepted_account_info,
      ProfileAttributesStorage* profile_attribute_storage);
  bool ShouldShowEnterpriseBubble(const AccountInfo& intercepted_account_info);
  bool ShouldShowMultiUserBubble(const AccountInfo& intercepted_account_info);

  // signin::IdentityManager::Observer:
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

  // Called when the extended account info was not updated after a timeout.
  void OnExtendedAccountInfoFetchTimeout();

  // Called after the user chose whether a new profile would be created.
  void OnProfileCreationChoice(SkColor profile_color, bool create);
  // Called after the user chose whether the session should continue in a new
  // profile.
  void OnProfileSwitchChoice(const base::FilePath& profile_path,
                             bool switch_profile);

  // Called when the new profile is created or loaded from disk.
  // `profile_color` is set as theme color for the profile ; it should be
  // nullopt if the profile is not new (loaded from disk).
  void OnNewSignedInProfileCreated(base::Optional<SkColor> profile_color,
                                   Profile* new_profile);

  // Called when the new browser is created after interception. Passed as
  // callback to `session_startup_helper_`.
  void OnNewBrowserCreated(bool show_customization_bubble);

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
  // Used for metrics:
  bool was_interception_ui_displayed_ = false;
  base::TimeTicks account_info_fetch_start_time_;
  base::TimeTicks profile_creation_start_time_;
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_WEB_SIGNIN_INTERCEPTOR_H_
