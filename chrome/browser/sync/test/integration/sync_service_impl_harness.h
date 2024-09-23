// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SERVICE_IMPL_HARNESS_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SERVICE_IMPL_HARNESS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/service/sync_service_impl.h"

class Profile;

namespace syncer {
class SyncSetupInProgressHandle;
class SyncUserSettings;
}  // namespace syncer

class SyncSigninDelegate;

// An instance of this class is basically our notion of a "sync client" for
// automation purposes. It harnesses the SyncServiceImpl member of the
// profile passed to it on construction and automates certain things like setup
// and authentication. It provides ways to "wait" adequate periods of time for
// several clients to get to the same state.
class SyncServiceImplHarness {
 public:
  // The type of profile signin method to authenticate a profile.
  enum class SigninType {
    // Fakes user signin process.
    FAKE_SIGNIN,
    // Uses UI signin flow and connects to GAIA servers for authentication.
    UI_SIGNIN
  };

  using SetUserSettingsCallback =
      base::OnceCallback<void(syncer::SyncUserSettings*)>;

  static std::unique_ptr<SyncServiceImplHarness> Create(
      Profile* profile,
      const std::string& username,
      const std::string& password,
      SigninType signin_type);
  ~SyncServiceImplHarness();

  SyncServiceImplHarness(const SyncServiceImplHarness&) = delete;
  SyncServiceImplHarness& operator=(const SyncServiceImplHarness&) = delete;

  // Change the username to use for future signins. Must only be called while
  // there is no primary account.
  void SetUsernameForFutureSignins(const std::string& username);

  signin::GaiaIdHash GetGaiaIdHashForPrimaryAccount() const;

  // Signs in to a primary account without actually enabling sync the feature.
  [[nodiscard]] bool SignInPrimaryAccount(
      signin::ConsentLevel consent_level = signin::ConsentLevel::kSignin);

  // This is similar to click the reset button on chrome.google.com/sync.
  void ResetSyncForPrimaryAccount();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Signs out of the primary account. ChromeOS doesn't have the concept of
  // sign-out, so this only exists on other platforms.
  void SignOutPrimaryAccount();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // The underlying implementation for mimic-ing persistent auth errors isn't
  // implemented on Android, see https://crbug.com/1373448.
#if !BUILDFLAG(IS_ANDROID)
  // Enters/exits the "Sync paused" state, which in real life happens if a
  // syncing user signs out of the content area.
  void EnterSyncPausedStateForPrimaryAccount();
  bool ExitSyncPausedStateForPrimaryAccount();
#endif  // !BUILDFLAG(IS_ANDROID)

  // Enables and configures sync for all available datatypes. Returns true only
  // after sync has been fully initialized and authenticated, and we are ready
  // to process changes.
  // |user_settings_callback| will be called once the engine is initialized, but
  // before actually starting sync, to give the caller a chance to modify sync
  // settings (mostly the selected data types).
  [[nodiscard]] bool SetupSync(SetUserSettingsCallback user_settings_callback =
                                   SetUserSettingsCallback());

  // Enables and configures sync.
  // Does not wait for sync to be ready to process changes -- callers need to
  // ensure this by calling AwaitSyncSetupCompletion() or
  // AwaitSyncTransportActive().
  // |user_settings_callback| will be called once the engine is initialized, but
  // before actually starting sync, to give the caller a chance to modify sync
  // settings (mostly the selected data types).
  // Returns true on success.
  [[nodiscard]] bool SetupSyncNoWaitForCompletion(
      SetUserSettingsCallback user_settings_callback =
          SetUserSettingsCallback());

  // Signals that sync setup is complete, and that PSS may begin syncing.
  // Typically SetupSync does this automatically, but if that returned false,
  // then setup may have been left incomplete.
  void FinishSyncSetup();

  // Calling this acts as a barrier and blocks the caller until |this| and
  // |partner| have both completed a sync cycle.  When calling this method,
  // the |partner| should be the passive responder who responds to the actions
  // of |this|.  This method relies upon the synchronization of callbacks
  // from the message queue. Returns true if two sync cycles have completed.
  // Note: Use this method when exactly one client makes local change(s), and
  // exactly one client is waiting to receive those changes.
  [[nodiscard]] bool AwaitMutualSyncCycleCompletion(
      SyncServiceImplHarness* partner);

  // Blocks the caller until every client in |clients| completes its ongoing
  // sync cycle and all the clients' progress markers match.  Note: Use this
  // method when more than one client makes local change(s), and more than one
  // client is waiting to receive those changes.
  [[nodiscard]] static bool AwaitQuiescence(
      const std::vector<SyncServiceImplHarness*>& clients);

  // Blocks the caller until the sync engine is initialized or some end state
  // (e.g., auth error) is reached. Returns true only if the engine initialized
  // successfully. See SyncService::IsEngineInitialized() for the definition
  // of engine initialization.
  [[nodiscard]] bool AwaitEngineInitialization();

  // Blocks the caller until sync setup is complete, and sync-the-feature is
  // active. Returns true if and only if sync setup completed successfully. Make
  // sure to actually start sync setup (usually by calling SetupSync() or one of
  // its variants) before.
  [[nodiscard]] bool AwaitSyncSetupCompletion();

  // Blocks the caller until the sync transport layer is active. Returns true if
  // successful.
  [[nodiscard]] bool AwaitSyncTransportActive();

  // Blocks the caller until invalidations are enabled or disabled.
  [[nodiscard]] bool AwaitInvalidationsStatus(bool expected_status);

  // Returns the SyncServiceImpl member of the sync client.
  syncer::SyncServiceImpl* service() { return service_; }
  const syncer::SyncServiceImpl* service() const { return service_; }

  // Returns the debug name for this profile. Used for logging.
  const std::string& profile_debug_name() const { return profile_debug_name_; }

  // Enables sync for a particular selectable sync type (will enable sync for
  // all corresponding datatypes). Returns true on success.
  [[nodiscard]] bool EnableSyncForType(syncer::UserSelectableType type);

  // Disables sync for a particular selectable sync type (will enable sync for
  // all corresponding datatypes). Returns true on success.
  [[nodiscard]] bool DisableSyncForType(syncer::UserSelectableType type);

  // Enables sync for all registered sync datatypes. Returns true on success.
  [[nodiscard]] bool EnableSyncForRegisteredDatatypes();

  // Disables sync for all sync datatypes. Returns true on success.
  [[nodiscard]] bool DisableSyncForAllDatatypes();

  // Returns a snapshot of the current sync session.
  syncer::SyncCycleSnapshot GetLastCycleSnapshot() const;

 private:
  SyncServiceImplHarness(Profile* profile,
                         const std::string& username,
                         const std::string& password,
                         SigninType signin_type);

  // Gets detailed status from |service_| in pretty-printable form.
  std::string GetServiceStatus();

  // Returns a string with relevant info about client's sync state (if
  // available), annotated with |message|. Useful for logging.
  std::string GetClientInfoString(const std::string& message) const;

  // Returns true if the user has enabled and configured sync for this client.
  // Note that this does not imply sync is actually running.
  bool IsSyncEnabledByUser() const;

  // Profile associated with this sync client.
  const raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_;

  // SyncServiceImpl object associated with |profile_|.
  const raw_ptr<syncer::SyncServiceImpl, AcrossTasksDanglingUntriaged> service_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;

  // Credentials to use for signin (and in the case of SIGNIN_UI, for the actual
  // GAIA authentication).
  std::string username_;
  std::string password_;

  // Used to decide what method of profile signin to use.
  const SigninType signin_type_;

  // Used for logging.
  const std::string profile_debug_name_;

  // Delegate to sign-in the test account across platforms.
  std::unique_ptr<SyncSigninDelegate> signin_delegate_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SERVICE_IMPL_HARNESS_H_
