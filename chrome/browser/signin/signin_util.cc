// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_util.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_internal.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/webui/profile_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_MAC)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#define CAN_DELETE_PROFILE
#endif

namespace signin_util {
namespace {

constexpr char kSignoutSettingKey[] = "signout_setting";

#if defined(CAN_DELETE_PROFILE)
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
                             Delegate* delegate)
      : primary_account_email_(primary_account_email), delegate_(delegate) {}

  DeleteProfileDialogManager(const DeleteProfileDialogManager&) = delete;
  DeleteProfileDialogManager& operator=(const DeleteProfileDialogManager&) =
      delete;

  ~DeleteProfileDialogManager() override { BrowserList::RemoveObserver(this); }

  void PresentDialogOnAllBrowserWindows(Profile* profile) {
    DCHECK(profile);
    DCHECK(profile_path_.empty());
    profile_path_ = profile->GetPath();

    BrowserList::AddObserver(this);
    Browser* active_browser = chrome::FindLastActiveWithProfile(profile);
    if (active_browser)
      OnBrowserSetLastActive(active_browser);
  }

  void OnBrowserSetLastActive(Browser* browser) override {
    DCHECK(!profile_path_.empty());

    if (profile_path_ != browser->profile()->GetPath())
      return;

    active_browser_ = browser;

    // Display the dialog on the next run loop as otherwise the dialog can block
    // browser from displaying because the dialog creates a nested run loop.
    //
    // This happens because the browser window is not fully created yet when
    // OnBrowserSetLastActive() is called. To finish the creation, the code
    // needs to return from OnBrowserSetLastActive().
    //
    // However, if we open a warning dialog from OnBrowserSetLastActive()
    // synchronously, it will create a nested run loop that will not return
    // from OnBrowserSetLastActive() until the dialog is dismissed. But the user
    // cannot dismiss the dialog because the browser is not even shown!
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&DeleteProfileDialogManager::ShowDeleteProfileDialog,
                       weak_factory_.GetWeakPtr(), browser));
  }

  // Called immediately after a browser becomes not active.
  void OnBrowserNoLongerActive(Browser* browser) override {
    if (active_browser_ == browser)
      active_browser_ = nullptr;
  }

  void OnBrowserRemoved(Browser* browser) override {
    if (active_browser_ == browser)
      active_browser_ = nullptr;
  }

 private:
  void ShowDeleteProfileDialog(Browser* browser) {
    // Block opening dialog from nested task.
    static bool is_dialog_shown = false;
    if (is_dialog_shown)
      return;
    base::AutoReset<bool> auto_reset(&is_dialog_shown, true);

    // Check that |browser| is still active.
    if (!active_browser_ || active_browser_ != browser)
      return;

    // Show the dialog.
    DCHECK(browser->window()->GetNativeWindow());
    chrome::MessageBoxResult result = chrome::ShowWarningMessageBox(
        browser->window()->GetNativeWindow(),
        l10n_util::GetStringUTF16(IDS_PROFILE_WILL_BE_DELETED_DIALOG_TITLE),
        l10n_util::GetStringFUTF16(
            IDS_PROFILE_WILL_BE_DELETED_DIALOG_DESCRIPTION,
            base::ASCIIToUTF16(primary_account_email_),
            base::ASCIIToUTF16(
                gaia::ExtractDomainName(primary_account_email_))));

    switch (result) {
      case chrome::MessageBoxResult::MESSAGE_BOX_RESULT_NO: {
        // If the warning dialog is automatically dismissed or the user closed
        // the dialog by clicking on the close "X" button, then re-present the
        // dialog (the user should not be able to interact with the browser
        // window as the profile must be deleted).
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(&DeleteProfileDialogManager::ShowDeleteProfileDialog,
                           weak_factory_.GetWeakPtr(), browser));
        break;
      }
      case chrome::MessageBoxResult::MESSAGE_BOX_RESULT_YES:
        webui::DeleteProfileAtPath(
            profile_path_,
            ProfileMetrics::DELETE_PROFILE_PRIMARY_ACCOUNT_NOT_ALLOWED);
        delegate_->OnProfileDeleted(this);
        // |this| may be destroyed at this point. Avoid using it.
        break;
      case chrome::MessageBoxResult::MESSAGE_BOX_RESULT_DEFERRED:
        NOTREACHED() << "Message box must not return deferred result when run "
                        "synchronously";
        break;
    }
  }

  std::string primary_account_email_;
  raw_ptr<Delegate> delegate_;
  base::FilePath profile_path_;
  raw_ptr<Browser> active_browser_;
  base::WeakPtrFactory<DeleteProfileDialogManager> weak_factory_{this};
};
#endif  // defined(CAN_DELETE_PROFILE)

// Per-profile manager for the signout allowed setting.
#if defined(CAN_DELETE_PROFILE)
class UserSignoutSetting : public base::SupportsUserData::Data,
                           public DeleteProfileDialogManager::Delegate {
#else
class UserSignoutSetting : public base::SupportsUserData::Data {
#endif  // defined(CAN_DELETE_PROFILE)
 public:
  enum class State { kUndefined, kAllowed, kDisallowed };

  // Fetch from Profile. Make and store if not already present.
  static UserSignoutSetting* GetForProfile(Profile* profile) {
    UserSignoutSetting* signout_setting = static_cast<UserSignoutSetting*>(
        profile->GetUserData(kSignoutSettingKey));

    if (!signout_setting) {
      profile->SetUserData(kSignoutSettingKey,
                           std::make_unique<UserSignoutSetting>());
      signout_setting = static_cast<UserSignoutSetting*>(
          profile->GetUserData(kSignoutSettingKey));
    }

    return signout_setting;
  }

  State state() const { return state_; }
  void set_state(State state) { state_ = state; }

#if defined(CAN_DELETE_PROFILE)
  // Shows the delete profile dialog on the first browser active window.
  void ShowDeleteProfileDialog(Profile* profile, const std::string& email) {
    if (delete_profile_dialog_manager_)
      return;
    delete_profile_dialog_manager_ =
        std::make_unique<DeleteProfileDialogManager>(email, this);
    delete_profile_dialog_manager_->PresentDialogOnAllBrowserWindows(profile);
  }

  void OnProfileDeleted(DeleteProfileDialogManager* dialog_manager) override {
    DCHECK_EQ(delete_profile_dialog_manager_.get(), dialog_manager);
    delete_profile_dialog_manager_.reset();
  }
#endif

 private:
  State state_ = State::kUndefined;

#if defined(CAN_DELETE_PROFILE)
  std::unique_ptr<DeleteProfileDialogManager> delete_profile_dialog_manager_;
#endif
};

enum ForceSigninPolicyCache {
  NOT_CACHED = 0,
  ENABLE,
  DISABLE
} g_is_force_signin_enabled_cache = NOT_CACHED;

void SetForceSigninPolicy(bool enable) {
  g_is_force_signin_enabled_cache = enable ? ENABLE : DISABLE;
}

}  // namespace

ScopedForceSigninSetterForTesting::ScopedForceSigninSetterForTesting(
    bool enable) {
  SetForceSigninForTesting(enable);  // IN-TEST
}

ScopedForceSigninSetterForTesting::~ScopedForceSigninSetterForTesting() {
  ResetForceSigninForTesting();  // IN-TEST
}

bool IsForceSigninEnabled() {
  if (g_is_force_signin_enabled_cache == NOT_CACHED) {
    PrefService* prefs = g_browser_process->local_state();
    if (prefs)
      SetForceSigninPolicy(prefs->GetBoolean(prefs::kForceBrowserSignin));
    else
      return false;
  }
  return (g_is_force_signin_enabled_cache == ENABLE);
}

void SetForceSigninForTesting(bool enable) {
  SetForceSigninPolicy(enable);
}

void ResetForceSigninForTesting() {
  g_is_force_signin_enabled_cache = NOT_CACHED;
}

bool IsUserSignoutAllowedForProfile(Profile* profile) {
  return UserSignoutSetting::GetForProfile(profile)->state() ==
         UserSignoutSetting::State::kAllowed;
}

void EnsureUserSignoutAllowedIsInitializedForProfile(Profile* profile) {
  if (UserSignoutSetting::GetForProfile(profile)->state() ==
      UserSignoutSetting::State::kUndefined) {
    SetUserSignoutAllowedForProfile(profile, true);
  }
}

void SetUserSignoutAllowedForProfile(Profile* profile, bool is_allowed) {
  UserSignoutSetting::State new_state =
      is_allowed ? UserSignoutSetting::State::kAllowed
                 : UserSignoutSetting::State::kDisallowed;
  UserSignoutSetting::GetForProfile(profile)->set_state(new_state);
}

void EnsurePrimaryAccountAllowedForProfile(Profile* profile) {
// All primary accounts are allowed on ChromeOS, so this method is a no-op on
// ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync))
    return;

  CoreAccountInfo primary_account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  if (profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed) &&
      signin::IsUsernameAllowedByPatternFromPrefs(
          g_browser_process->local_state(), primary_account.email)) {
    return;
  }

  UserSignoutSetting* signout_setting =
      UserSignoutSetting::GetForProfile(profile);
  switch (signout_setting->state()) {
    case UserSignoutSetting::State::kUndefined:
      NOTREACHED();
      break;
    case UserSignoutSetting::State::kAllowed: {
      // Force clear the primary account if it is no longer allowed and if sign
      // out is allowed.
      auto* primary_account_mutator =
          identity_manager->GetPrimaryAccountMutator();
      primary_account_mutator->ClearPrimaryAccount(
          signin_metrics::SIGNIN_NOT_ALLOWED_ON_PROFILE_INIT,
          signin_metrics::SignoutDelete::kIgnoreMetric);
      break;
    }
    case UserSignoutSetting::State::kDisallowed:
#if defined(CAN_DELETE_PROFILE)
      // Force remove the profile if sign out is not allowed and if the
      // primary account is no longer allowed.
      // This may be called while the profile is initializing, so it must be
      // scheduled for later to allow the profile initialization to complete.
      CHECK(profiles::IsMultipleProfilesEnabled());
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&UserSignoutSetting::ShowDeleteProfileDialog,
                         base::Unretained(signout_setting), profile,
                         primary_account.email));
#else
      CHECK(false) << "Deleting profiles is not supported.";
#endif  // defined(CAN_DELETE_PROFILE)
      break;
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

#if !defined(OS_ANDROID)
bool ProfileSeparationEnforcedByPolicy(
    Profile* profile,
    const std::string& intercepted_account_level_policy_value) {
  if (!base::FeatureList::IsEnabled(kAccountPoliciesLoadedWithoutSync))
    return false;
  std::string current_profile_account_restriction =
      profile->GetPrefs()->GetString(prefs::kManagedAccountsSigninRestriction);

  bool is_machine_level_policy = profile->GetPrefs()->GetBoolean(
      prefs::kManagedAccountsSigninRestrictionScopeMachine);

  // Enforce profile separation for all new signins if any restriction is
  // applied at a machine level.
  if (is_machine_level_policy) {
    return !current_profile_account_restriction.empty() &&
           current_profile_account_restriction != "none";
  }

  // Enforce profile separation for all new signins if "primary_account_strict"
  // is set at the user account level.
  return current_profile_account_restriction == "primary_account_strict" ||
         base::StartsWith(intercepted_account_level_policy_value,
                          "primary_account");
}

void RecordEnterpriseProfileCreationUserChoice(bool enforced_by_policy,
                                               bool created) {
  base::UmaHistogramBoolean(
      enforced_by_policy
          ? "Signin.Enterprise.WorkProfile.ProfileCreatedWithPolicySet"
          : "Signin.Enterprise.WorkProfile.ProfileCreatedwithPolicyUnset",
      created);
}

#endif

}  // namespace signin_util
