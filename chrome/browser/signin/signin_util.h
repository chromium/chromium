// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "base/supports_user_data.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"
class Browser;
#endif

class Profile;

namespace signin_util {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
// Manager that presents the profile will be deleted dialog on the first active
// browser window.
class DeleteProfileDialogManager : public BrowserListObserver {
 public:
  class Delegate {
   public:
    // Called when the profile was marked for deletion. It is safe for the
    // delegate to delete |manager| when this is called.
    virtual void OnProfileDeleted(DeleteProfileDialogManager* manager) = 0;
  };

  DeleteProfileDialogManager(std::string primary_account_email,
                             Delegate* delegate);

  DeleteProfileDialogManager(const DeleteProfileDialogManager&) = delete;
  DeleteProfileDialogManager& operator=(const DeleteProfileDialogManager&) =
      delete;

  ~DeleteProfileDialogManager() override;

  void PresentDialogOnAllBrowserWindows(Profile* profile);

  void OnBrowserSetLastActive(Browser* browser) override;
  // Called immediately after a browser becomes not active.
  void OnBrowserNoLongerActive(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

 private:
  void ShowDeleteProfileDialog(Browser* browser);

  std::string primary_account_email_;
  raw_ptr<Delegate> delegate_;
  base::FilePath profile_path_;
  raw_ptr<Browser> active_browser_;
  base::WeakPtrFactory<DeleteProfileDialogManager> weak_factory_{this};
};
#endif

// TODO(crbug.com/1311656): Split UserSignoutSettings from
// DeleteProfileDialogManager and move the ownership to a keyedService.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
class UserSignoutSetting : public base::SupportsUserData::Data,
                           public DeleteProfileDialogManager::Delegate {
#else
class UserSignoutSetting : public base::SupportsUserData::Data {
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
 public:
  enum class State { kUndefined, kAllowed, kDisallowed };

  // Fetch from Profile. Make and store if not already present.
  static UserSignoutSetting* GetForProfile(Profile* profile);
  UserSignoutSetting();
  ~UserSignoutSetting() override;

  State state() const { return state_; }
  void set_state(State state) { state_ = state; }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  // Shows the delete profile dialog on the first browser active window.
  void ShowDeleteProfileDialog(Profile* profile, const std::string& email);
  void OnProfileDeleted(DeleteProfileDialogManager* dialog_manager) override;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

 private:
  State state_ = State::kUndefined;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  std::unique_ptr<DeleteProfileDialogManager> delete_profile_dialog_manager_;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
};

// This class calls ResetForceSigninForTesting when destroyed, so that
// ForcedSigning doesn't leak across tests.
class ScopedForceSigninSetterForTesting {
 public:
  explicit ScopedForceSigninSetterForTesting(bool enable);
  ~ScopedForceSigninSetterForTesting();
};

// Return whether the force sign in policy is enabled or not.
// The state of this policy will not be changed without relaunch Chrome.
bool IsForceSigninEnabled();

// Enable or disable force sign in for testing. Please use
// ScopedForceSigninSetterForTesting instead, if possible. If not, make sure
// ResetForceSigninForTesting is called before the test finishes.
void SetForceSigninForTesting(bool enable);

// Reset force sign in to uninitialized state for testing.
void ResetForceSigninForTesting();

// Returns true if clearing the primary profile is allowed.
bool IsUserSignoutAllowedForProfile(Profile* profile);

// Sign-out is allowed by default, but some Chrome profiles (e.g. for cloud-
// managed enterprise accounts) may wish to disallow user-initiated sign-out.
// Note that this exempts sign-outs that are not user-initiated (e.g. sign-out
// triggered when cloud policy no longer allows current email pattern). See
// ChromeSigninClient::PreSignOut().
void SetUserSignoutAllowedForProfile(Profile* profile, bool is_allowed);

// Updates the user sign-out state to |true| if is was never initialized.
// This should be called at the end of the flow to initialize a profile to
// ensure that the signout allowed flag is updated.
void EnsureUserSignoutAllowedIsInitializedForProfile(Profile* profile);

// Ensures that the primary account for |profile| is allowed:
// * If profile does not have any primary account, then this is a no-op.
// * If |IsUserSignoutAllowedForProfile| is allowed and the primary account
//   is no longer allowed, then this clears the primary account.
// * If |IsUserSignoutAllowedForProfile| is not allowed and the primary account
//   is not longer allowed, then this removes the profile.
void EnsurePrimaryAccountAllowedForProfile(Profile* profile);

#if !BUILDFLAG(IS_ANDROID)
// Returns true if profile separation is enforced by policy.
bool ProfileSeparationEnforcedByPolicy(
    Profile* profile,
    const std::string& intercepted_account_level_policy_value);

// Records a UMA metric if the user accepts or not to create an enterprise
// profile.
void RecordEnterpriseProfileCreationUserChoice(bool enforced_by_policy,
                                               bool created);
#endif

}  // namespace signin_util

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UTIL_H_
