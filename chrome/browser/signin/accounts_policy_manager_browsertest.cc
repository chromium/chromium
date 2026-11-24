// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/accounts_policy_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/shortcut_manager.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_attributes_storage_observer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/accounts_policy_manager_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser_active_state_manager/browser_active_state_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/simple_message_box_internal.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/browser/ui/webui/signin/signin_utils_desktop.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test_api.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

constexpr char kManagedEmail[] = "user@managed.com";
constexpr char kNonManagedEmail[] = "user@gmail.com";

class ProfileRemovalWaiter : public ProfileAttributesStorageObserver {
 public:
  explicit ProfileRemovalWaiter(const base::FilePath& expected_profile_path)
      : expected_profile_path_(expected_profile_path) {
    observation_.Observe(
        &g_browser_process->profile_manager()->GetProfileAttributesStorage());
  }
  ~ProfileRemovalWaiter() override = default;

  void Wait() { run_loop_.Run(); }

  // ProfileAttributesStorageObserver:
  void OnProfileWasRemoved(const base::FilePath& profile_path,
                           const std::u16string& profile_name) override {
    if (profile_path == expected_profile_path_) {
      run_loop_.Quit();
    }
  }

 private:
  const base::FilePath expected_profile_path_;
  base::RunLoop run_loop_;
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorageObserver>
      observation_{this};
};

class AccountsPolicyManagerBrowserTest : public SigninBrowserTestBase {
 public:
  AccountsPolicyManagerBrowserTest() = default;
  ~AccountsPolicyManagerBrowserTest() override = default;

  AccountsPolicyManagerBrowserTest(const AccountsPolicyManagerBrowserTest&) =
      delete;
  AccountsPolicyManagerBrowserTest& operator=(
      const AccountsPolicyManagerBrowserTest&) = delete;

 private:
  base::AutoReset<bool> skip_message_box_for_test_{
      &chrome::internal::g_should_skip_message_box_for_test, true};
};

class AccountsPolicyManagerDeleteProfileDialogBrowserTest
    : public AccountsPolicyManagerBrowserTest {
 public:
  using DialogShownAction =
      base::RepeatingCallback<void(base::WeakPtr<views::Widget>, int)>;

  AccountsPolicyManagerDeleteProfileDialogBrowserTest() {
    widget_observer_.set_shown_callback(
        base::BindLambdaForTesting([this](views::Widget* widget) {
          if (widget->GetName() != "MessageBoxView") {
            return;
          }
          ++dialog_shown_count_;
          if (on_dialog_shown_action_) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(on_dialog_shown_action_, widget->GetWeakPtr(),
                               dialog_shown_count_));
          }
        }));
    widget_observer_.set_closing_callback(
        base::BindLambdaForTesting([this](views::Widget* widget) {
          if (widget->GetName() == "MessageBoxView" &&
              dialog_shown_count_ == target_dialog_close_count_ &&
              dialog_closed_quit_closure_) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, std::move(dialog_closed_quit_closure_));
          }
        }));
  }

  ~AccountsPolicyManagerDeleteProfileDialogBrowserTest() override = default;

  void SetUpOnMainThread() override {
    AccountsPolicyManagerBrowserTest::SetUpOnMainThread();
    // Ignore spurious window activation changes from parallel browser test
    // shards so modal message boxes and `BrowserActiveStateManager` are not
    // unexpectedly affected while spinning run loops.
    views::DisableActivationChangeHandlingForTests();

    identity_test_env()->MakePrimaryAccountAvailable(
        kManagedEmail, signin::ConsentLevel::kSignin);
    enterprise_util::SetUserAcceptedAccountManagement(GetProfile(), true);
    ASSERT_TRUE(
        identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

    profile_path_ = GetProfile()->GetPath();
    ActivateBrowser();
  }

  void TearDownOnMainThread() override {
    active_lock_.reset();
    AccountsPolicyManagerBrowserTest::TearDownOnMainThread();
  }

  void SetOnDialogShownAction(DialogShownAction action) {
    on_dialog_shown_action_ = std::move(action);
  }

  void TriggerDeleteProfileDialog() {
    g_browser_process->local_state()->SetString(
        prefs::kGoogleServicesUsernamePattern, ".*@otherdomain.com");
  }

  void WaitForDialogClose(int target_dialog_count = 1) {
    target_dialog_close_count_ = target_dialog_count;
    base::RunLoop dialog_closed_run_loop;
    dialog_closed_quit_closure_ = dialog_closed_run_loop.QuitClosure();
    dialog_closed_run_loop.Run();
  }

  void ActivateBrowser() {
    views::Widget* browser_widget = views::Widget::GetWidgetForNativeWindow(
        browser()->GetWindow()->GetNativeWindow());
    ASSERT_TRUE(browser_widget);
    active_lock_ = browser_widget->LockPaintAsActive();
    BrowserActiveStateManager::From(browser())->DidBecomeActive();
  }

  void DeactivateBrowser() {
    active_lock_.reset();
    if (browser()) {
      BrowserActiveStateManager::From(browser())->DidBecomeInactive();
    }
  }

  void ReleaseActiveLock() { active_lock_.reset(); }

  bool IsProfileInStorage() const {
    return g_browser_process->profile_manager()
               ->GetProfileAttributesStorage()
               .GetProfileAttributesWithPath(profile_path_) != nullptr;
  }

  const base::FilePath& profile_path() const { return profile_path_; }
  int dialog_shown_count() const { return dialog_shown_count_; }

 private:
  base::AutoReset<bool> show_message_box_{
      &chrome::internal::g_should_skip_message_box_for_test, false};
  base::FilePath profile_path_;
  std::unique_ptr<views::Widget::PaintAsActiveLock> active_lock_;
  DialogShownAction on_dialog_shown_action_;
  int dialog_shown_count_ = 0;
  int target_dialog_close_count_ = 0;
  base::OnceClosure dialog_closed_quit_closure_;
  views::AnyWidgetObserver widget_observer_{
      views::test::AnyWidgetTestPasskey{}};
};

IN_PROC_BROWSER_TEST_F(AccountsPolicyManagerBrowserTest,
                       PRE_ProfileSigninAllowedPrefChanged_NonManaged) {
  // Sign in with a non-managed account.
  identity_test_env()->MakePrimaryAccountAvailable(
      kNonManagedEmail, signin::ConsentLevel::kSignin);
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // Toggle the pref.
  GetProfile()->GetPrefs()->SetBoolean(prefs::kSigninAllowedOnNextStartup,
                                       false);

  // Verify that the user is still signed in (pref doesn't take effect
  // immediately).
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

IN_PROC_BROWSER_TEST_F(AccountsPolicyManagerBrowserTest,
                       ProfileSigninAllowedPrefChanged_NonManaged) {
  EXPECT_FALSE(GetProfile()->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
  // Confirm that the user is no longer signed in to Chrome.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

// Fixture for startup profile-deletion tests. Unlike `SigninBrowserTestBase`,
// this does not attach `IdentityTestEnvironmentProfileAdaptor` to `browser()`
// in `SetUpOnMainThread()`, because `AccountsPolicyManager` asynchronously
// deletes the initial profile and closes its browser during startup.
class AccountsPolicyManagerStartupBrowserTest : public InProcessBrowserTest {
 public:
  AccountsPolicyManagerStartupBrowserTest() = default;
  ~AccountsPolicyManagerStartupBrowserTest() override = default;

  AccountsPolicyManagerStartupBrowserTest(
      const AccountsPolicyManagerStartupBrowserTest&) = delete;
  AccountsPolicyManagerStartupBrowserTest& operator=(
      const AccountsPolicyManagerStartupBrowserTest&) = delete;

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    // Shortcut deletion delays tests shutdown on Win-7 and results in time out.
    // See crbug.com/40686320.
    AppShortcutManager::SuppressShortcutsForTesting();
#endif
    InProcessBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    url_loader_helper_.SetUp();
    // Ignore native window activation changes from parallel browser test
    // shards so `DeleteProfileDialogManager` is not stalled by focus loss.
    views::DisableActivationChangeHandlingForTests();
  }

 private:
  ChromeSigninClientWithURLLoaderHelper url_loader_helper_;
  base::AutoReset<bool> skip_message_box_for_test_{
      &chrome::internal::g_should_skip_message_box_for_test, true};
};

IN_PROC_BROWSER_TEST_F(AccountsPolicyManagerStartupBrowserTest,
                       PRE_ProfileSigninAllowedPrefChanged_Managed) {
  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);
  ASSERT_TRUE(local_state->GetList(prefs::kProfilesDeleted).empty());

  Profile* profile = browser()->GetProfile();
  EXPECT_EQ(profile->GetPath(),
            g_browser_process->profile_manager()->user_data_dir().AppendASCII(
                chrome::kInitialProfile));

  // Sign in with a managed primary account and a secondary account.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountInfo primary_account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, kManagedEmail, signin::ConsentLevel::kSignin);
  AccountInfo secondary_account_info =
      signin::MakeAccountAvailable(identity_manager, kNonManagedEmail);
  enterprise_util::SetUserAcceptedAccountManagement(profile, true);

  // Prohibit sign-in on next start-up.
  profile->GetPrefs()->SetBoolean(prefs::kSigninAllowedOnNextStartup, false);

  // Verify that the user remains signed in with valid tokens until restart.
  EXPECT_EQ(
      primary_account_info.GetAccountId(),
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
      primary_account_info.GetAccountId()));
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_info.GetAccountId()));
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
      secondary_account_info.GetAccountId()));
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          secondary_account_info.GetAccountId()));
}

IN_PROC_BROWSER_TEST_F(AccountsPolicyManagerStartupBrowserTest,
                       ProfileSigninAllowedPrefChanged_Managed) {
  // Startup deletes the initial profile and closes its browser asynchronously;
  // clear `browser_` until the replacement browser window is ready.
  SetBrowser(nullptr);

  PrefService* local_state = g_browser_process->local_state();
  ASSERT_TRUE(local_state);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const base::FilePath initial_profile_path =
      profile_manager->user_data_dir().AppendASCII(chrome::kInitialProfile);

  // If the initial profile still has an open browser window that has not been
  // activated yet, lock paint-as-active and activate it so
  // `DeleteProfileDialogManager` proceeds with profile deletion without losing
  // active state to transient child widgets.
  std::unique_ptr<views::Widget::PaintAsActiveLock> paint_as_active_lock;
  if (Profile* initial_profile =
          profile_manager->GetProfileByPath(initial_profile_path)) {
    ProfileBrowserCollection::GetForProfile(initial_profile)
        ->ForEach([&](BrowserWindowInterface* browser_window) {
          if (views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
                  browser_window->GetWindow()->GetNativeWindow())) {
            paint_as_active_lock = widget->LockPaintAsActive();
          }
          BrowserActiveStateManager::From(browser_window)->DidBecomeActive();
          return false;
        });
  }

  // Wait for the initial profile to be marked for deletion if startup deletion
  // has not reached `MarkProfileDirectoryForDeletion()` yet.
  if (local_state->GetList(prefs::kProfilesDeleted).empty()) {
    base::RunLoop run_loop;
    PrefChangeRegistrar pref_registrar;
    pref_registrar.Init(local_state);
    pref_registrar.Add(prefs::kProfilesDeleted, run_loop.QuitClosure());
    run_loop.Run();
  }

  const base::ListValue& deleted_profiles =
      local_state->GetList(prefs::kProfilesDeleted);
  ASSERT_EQ(1U, deleted_profiles.size());
  EXPECT_EQ(base::ValueToFilePath(deleted_profiles[0]),
            initial_profile_path.BaseName());
  EXPECT_FALSE(profile_manager->GetProfileAttributesStorage()
                   .GetProfileAttributesWithPath(initial_profile_path));

  // Wait until the deleted profile finishes unloading (`Profile::Wipe()` holds
  // a `ScopedProfileKeepAlive` asynchronously) and a browser window opens for
  // the replacement profile.
  if (base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose)) {
    if (Profile* initial_profile =
            profile_manager->GetProfileByPath(initial_profile_path)) {
      ProfileDestructionWaiter(initial_profile).Wait();
    }
  }

  BrowserWindowInterface* replacement_browser = nullptr;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    GlobalBrowserCollection::GetInstance()->ForEach(
        [&](BrowserWindowInterface* browser_window) {
          if (browser_window->GetProfile()->GetPath() != initial_profile_path) {
            replacement_browser = browser_window;
            return false;
          }
          return true;
        });
    return replacement_browser != nullptr;
  }));
  SetBrowser(replacement_browser);

  // Verify that the replacement profile is active and signed out.
  if (base::FeatureList::IsEnabled(features::kDestroyProfileOnBrowserClose)) {
    EXPECT_EQ(1U, profile_manager->GetLoadedProfiles().size());
  }
  EXPECT_EQ(
      1U, profile_manager->GetProfileAttributesStorage().GetNumberOfProfiles());
  EXPECT_EQ(profile_manager->GetLastUsedProfile(),
            replacement_browser->GetProfile());
  signin::IdentityManager* replacement_identity_manager =
      IdentityManagerFactory::GetForProfile(replacement_browser->GetProfile());
  signin::WaitForRefreshTokensLoaded(replacement_identity_manager);
  EXPECT_FALSE(replacement_identity_manager->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  EXPECT_TRUE(
      replacement_identity_manager->GetAccountsWithRefreshTokens().empty());
}

IN_PROC_BROWSER_TEST_F(AccountsPolicyManagerBrowserTest,
                       PRE_ProfileSigninAllowedForCookies_Managed) {
  // Sign in with a managed account.
  identity_test_env()->MakePrimaryAccountAvailable(
      kManagedEmail, signin::ConsentLevel::kSignin);
  enterprise_util::SetUserAcceptedAccountManagement(GetProfile(), true);
  ASSERT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // Block cookies.
  HostContentSettingsMapFactory::GetForProfile(GetProfile())
      ->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                 CONTENT_SETTING_BLOCK);

  // Verify that the test has been using the initial profile.
  EXPECT_EQ(GetProfile()->GetPath(),
            g_browser_process->profile_manager()->user_data_dir().AppendASCII(
                chrome::kInitialProfile));
}

IN_PROC_BROWSER_TEST_F(AccountsPolicyManagerBrowserTest,
                       ProfileSigninAllowedForCookies_Managed) {
  base::FilePath default_profile_path =
      g_browser_process->profile_manager()->user_data_dir().AppendASCII(
          chrome::kInitialProfile);

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(default_profile_path);

  // The profile should STILL EXIST (not deleted).
  //
  // NOTE: This is a reliable proxy for "the dialog was not shown".
  // In this test suite, `g_should_skip_message_box_for_test` is set to true,
  // which causes `ShowWarningMessageBoxSync` to automatically return `YES`
  // (auto-confirm). Therefore, if the deletion flow were triggered (dialog
  // shown), the profile would be immediately deleted. The fact that it exists
  // proves the flow was not triggered.
  EXPECT_TRUE(entry);

  // Confirm that CanOfferSignin returns a cookies error.
  CoreAccountInfo primary_account =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  SigninUIError error =
      CanOfferSignin(GetProfile(), primary_account.gaia, primary_account.email,
                     /*allow_account_from_other_profile=*/true);
  EXPECT_EQ(error.type(), SigninUIError::Type::kSigninCookiesDisallowed);

  // The current profile should be the default one.
  EXPECT_EQ(browser()->GetProfile()->GetPath(), default_profile_path);

  // The user should still be signed in.
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

// Regression test for crbug.com/577213784: closing the browser window while
// `ShowWarningMessageBoxSync` is running its nested RunLoop must not crash
// with a null dereference on `active_browser_`.
IN_PROC_BROWSER_TEST_F(AccountsPolicyManagerDeleteProfileDialogBrowserTest,
                       CloseBrowserDuringDeleteProfileDialog) {
  SetOnDialogShownAction(base::BindLambdaForTesting(
      [this, weak_browser = browser()->GetWeakPtr()](
          base::WeakPtr<views::Widget> /*widget*/, int /*shown_count*/) {
        ReleaseActiveLock();
        if (weak_browser) {
          weak_browser->GetWindow()->Close();
        }
      }));

  TriggerDeleteProfileDialog();
  WaitForDialogClose(/*target_dialog_count=*/1);

  EXPECT_EQ(dialog_shown_count(), 1);
  EXPECT_TRUE(IsProfileInStorage());
}

// Regression test for crbug.com/577213784: shutting down
// `AccountsPolicyManager` while `ShowWarningMessageBoxSync` is running its
// nested RunLoop destroys `DeleteProfileDialogManager` without causing a
// use-after-free when the dialog subsequently closes.
IN_PROC_BROWSER_TEST_F(AccountsPolicyManagerDeleteProfileDialogBrowserTest,
                       ShutdownDuringDeleteProfileDialog) {
  SetOnDialogShownAction(base::BindLambdaForTesting(
      [this](base::WeakPtr<views::Widget> weak_widget, int /*shown_count*/) {
        AccountsPolicyManagerFactory::GetForProfile(GetProfile())->Shutdown();
        if (weak_widget) {
          weak_widget->widget_delegate()->AsDialogDelegate()->AcceptDialog();
        }
      }));

  TriggerDeleteProfileDialog();
  WaitForDialogClose(/*target_dialog_count=*/1);

  EXPECT_EQ(dialog_shown_count(), 1);
  EXPECT_TRUE(IsProfileInStorage());
}

// Regression test for crbug.com/577213784: dismissing the delete profile
// dialog while the browser is active re-presents it, deactivating the browser
// while the dialog is open does not crash or re-present until the browser is
// reactivated, and accepting the dialog deletes the profile.
IN_PROC_BROWSER_TEST_F(AccountsPolicyManagerDeleteProfileDialogBrowserTest,
                       DismissAndDeactivateDuringDeleteProfileDialog) {
  // Pre-create a second profile so deleting `GetProfile()` does not need to
  // create a new fallback profile from scratch.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());

  ProfileRemovalWaiter profile_removal_waiter(profile_path());
  SetOnDialogShownAction(base::BindLambdaForTesting(
      [this](base::WeakPtr<views::Widget> weak_widget, int shown_count) {
        if (!weak_widget) {
          return;
        }
        if (shown_count == 1) {
          // Clicking the close "X" button while `browser()` remains active
          // should re-present the dialog.
          weak_widget->CloseWithReason(
              views::Widget::ClosedReason::kCloseButtonClicked);
        } else if (shown_count == 2) {
          // Deactivating `browser()` and closing the dialog should clear
          // `active_browser_` without crashing or re-presenting the dialog.
          DeactivateBrowser();
          weak_widget->Close();
        } else if (shown_count == 3) {
          // Accepting the dialog after reactivation should delete the profile.
          weak_widget->widget_delegate()->AsDialogDelegate()->AcceptDialog();
        }
      }));

  TriggerDeleteProfileDialog();
  WaitForDialogClose(/*target_dialog_count=*/2);

  EXPECT_EQ(dialog_shown_count(), 2);
  EXPECT_TRUE(IsProfileInStorage());

  // Reactivating the browser should present the dialog again, and accepting it
  // should schedule the profile for deletion.
  ActivateBrowser();
  profile_removal_waiter.Wait();
  EXPECT_EQ(dialog_shown_count(), 3);
  EXPECT_FALSE(IsProfileInStorage());
}

}  // namespace
