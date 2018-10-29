// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PROFILE_SYNC_SERVICE_HARNESS_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PROFILE_SYNC_SERVICE_HARNESS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"

class Profile;

namespace syncer {
class SyncSetupInProgressHandle;
}  // namespace syncer

// An instance of this class is basically our notion of a "sync client" for
// automation purposes. It harnesses the ProfileSyncService member of the
// profile passed to it on construction and automates certain things like setup
// and authentication. It provides ways to "wait" adequate periods of time for
// several clients to get to the same state.
class ProfileSyncServiceHarness {
 public:
  // The type of profile signin method to authenticate a profile.
  enum class SigninType {
    // Fakes user signin process.
    FAKE_SIGNIN,
    // Uses UI signin flow and connects to GAIA servers for authentication.
    UI_SIGNIN
  };

  static std::unique_ptr<ProfileSyncServiceHarness> Create(
      Profile* profile,
      const std::string& username,
      const std::string& password,
      SigninType signin_type);
  ~ProfileSyncServiceHarness();

  // Signs in to a primary account without actually enabling sync the feature.
  bool SignInPrimaryAccount();

#if !defined(OS_CHROMEOS)
  // Signs out of the primary account. ChromeOS doesn't have the concept of
  // sign-out, so this only exists on other platforms.
  void SignOutPrimaryAccount();
#endif  // !OS_CHROMEOS

  // Enables and configures sync for all available datatypes. Returns true only
  // after sync has been fully initialized and authenticated, and we are ready
  // to process changes.
  bool SetupSync();

  // Sets up sync without authenticating through the passphrase encryption.
  // Use this method when you need to setup a client that you're going to call
  // StopSyncService(), StartSyncService() directly after.
  bool SetupSyncForClearingServerData();

  // Enables and configures sync only for the given |synced_datatypes|. Returns
  // true only after sync has been fully initialized and authenticated, and we
  // are ready to process changes.
  bool SetupSync(syncer::ModelTypeSet synced_datatypes);

  // Same as SetupSync(), but also sets the given encryption passphrase during
  // setup.
  bool SetupSyncWithEncryptionPassphrase(syncer::ModelTypeSet synced_datatypes,
                                         const std::string& passphrase);

  // Same as SetupSync(), but also sets the given decryption passphrase during
  // setup. If the passphrase is incorrect, this method will still return true
  // and Sync will be operational but with undecryptable datatypes disabled.
  bool SetupSyncWithDecryptionPassphrase(syncer::ModelTypeSet synced_datatypes,
                                         const std::string& passphrase);

  // Signals that sync setup is complete, and that PSS may begin syncing.
  // Typically SetupSync does this automatically, but if that returned false,
  // then setup may have been left incomplete.
  void FinishSyncSetup();

  // Methods to stop and restart the sync service.
  //
  // For example, this can be used to simulate a sign-in/sign-out or can be
  // useful to recover from a lost birthday.
  // To start from a clear slate, clear server
  // data first, then call StopSyncService(syncer::SyncService::CLEAR_DATA)
  // followed by StartSyncService().
  // To simulate the user being offline for a while, call
  // StopSyncService(syncer::SyncService::KEEP_DATA) followed by
  // StartSyncService();
  // Stops the sync service.
  void StopSyncService(syncer::SyncService::SyncStopDataFate data_fate);
  // Starts the sync service after a previous stop.
  bool StartSyncService();

  // Returns whether this client has unsynced items. Avoid verifying false
  // return values, because tests typically shouldn't make assumptions about
  // other datatypes.
  bool HasUnsyncedItems();

  // Calling this acts as a barrier and blocks the caller until |this| and
  // |partner| have both completed a sync cycle.  When calling this method,
  // the |partner| should be the passive responder who responds to the actions
  // of |this|.  This method relies upon the synchronization of callbacks
  // from the message queue. Returns true if two sync cycles have completed.
  // Note: Use this method when exactly one client makes local change(s), and
  // exactly one client is waiting to receive those changes.
  bool AwaitMutualSyncCycleCompletion(ProfileSyncServiceHarness* partner);

  // Blocks the caller until every client in |clients| completes its ongoing
  // sync cycle and all the clients' progress markers match.  Note: Use this
  // method when more than one client makes local change(s), and more than one
  // client is waiting to receive those changes.
  static bool AwaitQuiescence(
      const std::vector<ProfileSyncServiceHarness*>& clients);

  // Blocks the caller until the sync engine is initialized or some end state
  // (e.g., auth error) is reached. Returns true if and only if the engine
  // initialized successfully. See ProfileSyncService's IsEngineInitialized()
  // method for the definition of engine initialization.
  bool AwaitEngineInitialization(bool skip_passphrase_verification = false);

  // Blocks the caller until sync setup is complete. Returns true if and only
  // if sync setup completed successfully.
  bool AwaitSyncSetupCompletion(bool skip_passphrase_verification);

  // Returns the ProfileSyncService member of the sync client.
  browser_sync::ProfileSyncService* service() const { return service_; }

  // Returns the debug name for this profile. Used for logging.
  const std::string& profile_debug_name() const { return profile_debug_name_; }

  // Enables sync for a particular sync datatype. Returns true on success.
  bool EnableSyncForDatatype(syncer::ModelType datatype);

  // Disables sync for a particular sync datatype. Returns true on success.
  bool DisableSyncForDatatype(syncer::ModelType datatype);

  // Enables sync for all sync datatypes. Returns true on success.
  bool EnableSyncForAllDatatypes();

  // Disables sync for all sync datatypes. Returns true on success.
  bool DisableSyncForAllDatatypes();

  // Returns a snapshot of the current sync session.
  syncer::SyncCycleSnapshot GetLastCycleSnapshot() const;

 private:
  ProfileSyncServiceHarness(Profile* profile,
                            const std::string& username,
                            const std::string& password,
                            SigninType signin_type);

  // If |encryption_passphrase| has a value, it will be set during setup. If
  // not, no custom passphrase will be set. If |skip_passphrase_verification| is
  // true and Sync requires a passphrase, FinishSyncSetup() will not be called,
  // in order to give the caller a chance to provide the passphrase using
  // SetDecryptionPassphrase(). After that, the caller needs to call
  // FinishSyncSetup() manually.
  bool SetupSyncImpl(syncer::ModelTypeSet synced_datatypes,
                     bool skip_passphrase_verification,
                     const base::Optional<std::string>& encryption_passphrase);

  // Gets detailed status from |service_| in pretty-printable form.
  std::string GetServiceStatus();

  // Returns a string with relevant info about client's sync state (if
  // available), annotated with |message|. Useful for logging.
  std::string GetClientInfoString(const std::string& message) const;

  // Returns true if the user has enabled and configured sync for this client.
  // Note that this does not imply sync is actually running.
  bool IsSyncEnabledByUser() const;

  // Profile associated with this sync client.
  Profile* const profile_;

  // ProfileSyncService object associated with |profile_|.
  browser_sync::ProfileSyncService* const service_;

  // Prevents Sync from running until configuration is complete.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> sync_blocker_;

  // Credentials used for GAIA authentication.
  const std::string username_;
  const std::string password_;

  // Used to decide what method of profile signin to use.
  const SigninType signin_type_;

  // Used for logging.
  const std::string profile_debug_name_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceHarness);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PROFILE_SYNC_SERVICE_HARNESS_H_
