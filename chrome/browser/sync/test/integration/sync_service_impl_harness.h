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
#include "base/memory/weak_ptr.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/sync_test_account.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/service/sync_service_impl.h"
#include "google_apis/gaia/gaia_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class Profile;

namespace signin {
class GaiaIdHash;
}  // namespace signin

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

  static std::unique_ptr<SyncServiceImplHarness> Create(Profile* profile,
                                                        SigninType signin_type);
  ~SyncServiceImplHarness();

  SyncServiceImplHarness(const SyncServiceImplHarness&) = delete;
  SyncServiceImplHarness& operator=(const SyncServiceImplHarness&) = delete;

  signin::GaiaIdHash GetGaiaIdHashForPrimaryAccount() const;

  // Returns GaiaId for `account`. This method can be used when the account is
  // not signed in.
  GaiaId GetGaiaIdForAccount(SyncTestAccount account) const;

  // Returns the email for `account`. This method can be used when the account
  // is not signed in.
  std::string GetEmailForAccount(SyncTestAccount account) const;

  // Signs in to a primary account without enabling sync the feature.
  [[nodiscard]] bool SignInPrimaryAccount(
      SyncTestAccount account = SyncTestAccount::kDefaultAccount);

  // This is similar to click the reset button on chrome.google.com/data.
  [[nodiscard]] bool ResetSyncForPrimaryAccount();

#if !BUILDFLAG(IS_CHROMEOS)
  // Signs out of the primary account. ChromeOS doesn't have the concept of
  // sign-out, so this only exists on other platforms.
  void SignOutPrimaryAccount();
#endif  // !BUILDFLAG(IS_CHROMEOS)

  // The underlying implementation for mimic-ing persistent auth errors isn't
  // implemented on Android, see https://crbug.com/1373448.
#if !BUILDFLAG(IS_ANDROID)
  // Enters/exits the "Sync paused" state, which in real life happens if a
  // syncing user signs out of the content area.
  // TODO(crbug.com/401470426): Replace the usages with
  // Enter/ExitSignInPendingStateForPrimaryAccount().
  bool EnterSyncPausedStateForPrimaryAccount();
  bool ExitSyncPausedStateForPrimaryAccount();

  // Enters the "Sign-in pending" state and waits until the sync transport
  // layer is paused. Returns true if successful.
  bool EnterSignInPendingStateForPrimaryAccount();
  // Exits the "Sign-in pending" state and waits until the sync transport layer
  // is active. Returns true if successful.
  bool ExitSignInPendingStateForPrimaryAccount();
#endif  // !BUILDFLAG(IS_ANDROID)

  // Enables and configures sync for all available datatypes. Returns true only
  // after sync has been fully initialized and authenticated, and we are ready
  // to process changes.
  [[nodiscard]] bool SetupSync(
      SyncTestAccount account = SyncTestAccount::kDefaultAccount);

  // Same as above but allows the modify sync settings (e.g. selected types) as
  // part of the sync flow (advanced flow).
  // `user_settings_callback` will be called once the engine is initialized, but
  // before actually starting sync. Note that the caller is responsible for
  // invoking `SetInitialSyncFeatureSetupComplete()`, if appropriate.
  [[nodiscard]] bool SetupSyncWithCustomSettings(
      SetUserSettingsCallback user_settings_callback,
      SyncTestAccount account = SyncTestAccount::kDefaultAccount);

  // Enables and configures sync.
  // Does not wait for sync to be ready to process changes -- callers need to
  // ensure this by calling AwaitSyncTransportActive().
  [[nodiscard]] bool SetupSyncNoWaitForCompletion(
      SyncTestAccount account = SyncTestAccount::kDefaultAccount);

  // Same as above but allows the modify sync settings (e.g. selected types) as
  // part of the sync flow (advanced flow).
  // `user_settings_callback` will be called once the engine is initialized, but
  // before actually starting sync. Note that the caller is responsible for
  // invoking `SetInitialSyncFeatureSetupComplete()`, if appropriate.
  [[nodiscard]] bool SetupSyncWithCustomSettingsNoWaitForCompletion(
      SetUserSettingsCallback user_settings_callback,
      SyncTestAccount account = SyncTestAccount::kDefaultAccount);

  // Signals that sync setup is complete, and that PSS may begin syncing.
  // Typically SetupSync does this automatically, but if that returned false,
  // then setup may have been left incomplete.
  void FinishSyncSetup();

  // Calling this acts as a barrier and blocks the caller until `this` and
  // `partner` have both completed a sync cycle.  When calling this method,
  // the `partner` should be the passive responder who responds to the actions
  // of `this`.  This method relies upon the synchronization of callbacks
  // from the message queue. Returns true if two sync cycles have completed.
  // Note: Use this method when exactly one client makes local change(s), and
  // exactly one client is waiting to receive those changes.
  [[nodiscard]] bool AwaitMutualSyncCycleCompletion(
      SyncServiceImplHarness* partner);

  // Blocks the caller until every client in `clients` completes its ongoing
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

  // Blocks the caller until the sync transport layer is active. Returns true if
  // successful.
  [[nodiscard]] bool AwaitSyncTransportActive();

  // Blocks the caller until the sync transport layer is paused. Returns true if
  // successful.
  [[nodiscard]] bool AwaitSyncTransportPaused();

  // Blocks the caller until invalidations are enabled or disabled.
  [[nodiscard]] bool AwaitInvalidationsStatus(bool expected_status);

  // Returns the SyncServiceImpl member of the sync client.
  syncer::SyncServiceImpl* service() { return service_; }
  const syncer::SyncServiceImpl* service() const { return service_; }

  // Returns the debug name for this profile. Used for logging.
  const std::string& profile_debug_name() const { return profile_debug_name_; }

  // Enables the history-related sync types. This includes
  // UserSelectableType::kHistory and UserSelectableType::kTabs. The user must
  // already be signed in, or this will have no effect. Returns true on success.
  [[nodiscard]] bool EnableHistorySyncNoWaitForCompletion();

  // Enables Sync-the-feature for all registered sync datatypes. Returns true on
  // success.
  // TODO(crbug.com/353425612): Replace all calls to this with either
  // SetupSync() or EnableAllSelectableTypes().
  [[nodiscard]] bool EnableSyncForRegisteredDatatypes();

  // Disables sync for all sync datatypes. Returns true on success.
  // TODO(crbug.com/353425612): Replace all calls to this with
  // DisableAllSelectableTypes() which is identical.
  [[nodiscard]] bool DisableSyncForAllDatatypes();

  // Enables/disables a particular selectable type. The user must already be
  // signed in, or this has no effect.
  [[nodiscard]] bool EnableSelectableType(syncer::UserSelectableType type);
  [[nodiscard]] bool DisableSelectableType(syncer::UserSelectableType type);

  // Enables/disables all available selectable types. The user must already be
  // signed in, or this has no effect.
  [[nodiscard]] bool EnableAllSelectableTypes();
  [[nodiscard]] bool DisableAllSelectableTypes();

  // Returns a snapshot of the current sync session.
  syncer::SyncCycleSnapshot GetLastCycleSnapshot() const;

  // Returns the datatypes which have local changes that have not yet been
  // synced with the server.
  absl::flat_hash_map<syncer::DataType, size_t> GetTypesWithUnsyncedDataAndWait(
      syncer::DataTypeSet requested_types) const;

  // Retrieves the LocalDataDescription for the specified `data_type`.
  // it assumes the service will provide a unique description for this specific
  // type. Returns this description, or default value (empty value) if the
  // service misbehaves and returns a response that cannot be interpreted.
  syncer::LocalDataDescription GetLocalDataDescriptionAndWait(
      syncer::DataType data_type);

 private:
  // `profile` must not be null and must outlive `this`. `signin_delegate` must
  // not be null.
  SyncServiceImplHarness(Profile* profile,
                         std::unique_ptr<SyncSigninDelegate> signin_delegate);

  // Gets detailed status from `service_` in pretty-printable form.
  std::string GetServiceStatus();

  // Returns a string with relevant info about client's sync state (if
  // available), annotated with `message`. Useful for logging.
  std::string GetClientInfoString(const std::string& message) const;

  // Returns true if the user has enabled and configured sync for this client.
  // Note that this does not imply sync is actually running.
  bool IsSyncEnabledByUser() const;

  // Profile associated with this sync client. WeakPtr is used to allow
  // flexibility in tests: this object may outlive `Profile` as long as it isn't
  // exercised.
  const base::WeakPtr<Profile> profile_;

  // SyncServiceImpl object associated with |profile_|.
  const raw_ptr<syncer::SyncServiceImpl, AcrossTasksDanglingUntriaged> service_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;

  // Used for logging.
  const std::string profile_debug_name_;

  // Delegate to sign-in the test account across platforms.
  const std::unique_ptr<SyncSigninDelegate> signin_delegate_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_SERVICE_IMPL_HARNESS_H_
