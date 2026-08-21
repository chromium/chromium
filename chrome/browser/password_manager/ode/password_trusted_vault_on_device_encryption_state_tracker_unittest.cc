// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/ode/password_trusted_vault_on_device_encryption_state_tracker.h"

#include "base/test/task_environment.h"
#include "chrome/browser/password_manager/ode/mock_on_device_encryption_state_tracker_observer.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using MockObserver = MockOnDeviceEncryptionStateTrackerObserver;

class PasswordTrustedVaultOnDeviceEncryptionStateTrackerTest
    : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  syncer::TestSyncService sync_service_;
};

TEST_F(PasswordTrustedVaultOnDeviceEncryptionStateTrackerTest,
       NullSyncService) {
  PasswordTrustedVaultOnDeviceEncryptionStateTracker tracker(nullptr);
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
}

TEST_F(PasswordTrustedVaultOnDeviceEncryptionStateTrackerTest,
       EngineNotInitialized) {
  sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);

  PasswordTrustedVaultOnDeviceEncryptionStateTracker tracker(&sync_service_);
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
}

TEST_F(PasswordTrustedVaultOnDeviceEncryptionStateTrackerTest, SignedOut) {
  sync_service_.SetSignedOut();

  PasswordTrustedVaultOnDeviceEncryptionStateTracker tracker(&sync_service_);
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
}

TEST_F(PasswordTrustedVaultOnDeviceEncryptionStateTrackerTest,
       PassphraseTypeNotTrustedVault) {
  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kKeystorePassphrase);
  {
    PasswordTrustedVaultOnDeviceEncryptionStateTracker tracker(&sync_service_);
    EXPECT_EQ(tracker.GetEncryptionState(),
              OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);
  }

  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kCustomPassphrase);
  {
    PasswordTrustedVaultOnDeviceEncryptionStateTracker tracker(&sync_service_);
    EXPECT_EQ(tracker.GetEncryptionState(),
              OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);
  }

  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kFrozenImplicitPassphrase);
  {
    PasswordTrustedVaultOnDeviceEncryptionStateTracker tracker(&sync_service_);
    EXPECT_EQ(tracker.GetEncryptionState(),
              OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);
  }
}

TEST_F(PasswordTrustedVaultOnDeviceEncryptionStateTrackerTest,
       PasswordsNotSelected) {
  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kTrustedVaultPassphrase);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kBookmarks});

  PasswordTrustedVaultOnDeviceEncryptionStateTracker tracker(&sync_service_);
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);
}

TEST_F(PasswordTrustedVaultOnDeviceEncryptionStateTrackerTest,
       DeviceNotReadyWhenKeyRequired) {
  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kTrustedVaultPassphrase);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPasswords});
  sync_service_.GetUserSettings()->SetTrustedVaultKeyRequired(true);

  PasswordTrustedVaultOnDeviceEncryptionStateTracker tracker(&sync_service_);
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kDeviceNotReady);
}

TEST_F(PasswordTrustedVaultOnDeviceEncryptionStateTrackerTest, DeviceReady) {
  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kTrustedVaultPassphrase);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPasswords});
  sync_service_.GetUserSettings()->SetTrustedVaultKeyRequired(false);

  PasswordTrustedVaultOnDeviceEncryptionStateTracker tracker(&sync_service_);
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kDeviceReady);
}

TEST_F(PasswordTrustedVaultOnDeviceEncryptionStateTrackerTest,
       PasswordsNotActive) {
  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kTrustedVaultPassphrase);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPasswords});
  sync_service_.GetUserSettings()->SetTrustedVaultKeyRequired(false);
  sync_service_.SetFailedDataTypes({syncer::PASSWORDS});

  PasswordTrustedVaultOnDeviceEncryptionStateTracker tracker(&sync_service_);
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);
}

TEST_F(PasswordTrustedVaultOnDeviceEncryptionStateTrackerTest,
       StateTransitionsAndObserverNotifications) {
  sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::INITIALIZING);

  PasswordTrustedVaultOnDeviceEncryptionStateTracker tracker(&sync_service_);
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);

  MockObserver observer;
  tracker.AddObserver(&observer);

  // Transition to DeviceNotReady.
  EXPECT_CALL(observer,
              OnDeviceEncryptionStateChanged(
                  &tracker,
                  OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable,
                  OnDeviceEncryptionState::kDeviceNotReady));
  sync_service_.SetMaxTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kTrustedVaultPassphrase);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPasswords});
  sync_service_.GetUserSettings()->SetTrustedVaultKeyRequired(true);
  sync_service_.FireStateChanged();
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kDeviceNotReady);

  // Transition to DeviceReady.
  EXPECT_CALL(observer, OnDeviceEncryptionStateChanged(
                            &tracker, OnDeviceEncryptionState::kDeviceNotReady,
                            OnDeviceEncryptionState::kDeviceReady));
  sync_service_.GetUserSettings()->SetTrustedVaultKeyRequired(false);
  sync_service_.FireStateChanged();
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kDeviceReady);

  // Transition to NotEnabled (passphrase changed to custom).
  EXPECT_CALL(observer,
              OnDeviceEncryptionStateChanged(
                  &tracker, OnDeviceEncryptionState::kDeviceReady,
                  OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled));
  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kCustomPassphrase);
  sync_service_.FireStateChanged();
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kOnDeviceEncryptionNotEnabled);

  // No change should not notify.
  EXPECT_CALL(observer, OnDeviceEncryptionStateChanged).Times(0);
  sync_service_.FireStateChanged();

  tracker.RemoveObserver(&observer);
}

TEST_F(PasswordTrustedVaultOnDeviceEncryptionStateTrackerTest,
       SyncServiceShutdown) {
  sync_service_.GetUserSettings()->SetPassphraseType(
      syncer::PassphraseType::kTrustedVaultPassphrase);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kPasswords});
  sync_service_.GetUserSettings()->SetTrustedVaultKeyRequired(false);

  PasswordTrustedVaultOnDeviceEncryptionStateTracker tracker(&sync_service_);
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kDeviceReady);

  MockObserver observer;
  tracker.AddObserver(&observer);

  EXPECT_CALL(
      observer,
      OnDeviceEncryptionStateChanged(
          &tracker, OnDeviceEncryptionState::kDeviceReady,
          OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable));
  sync_service_.Shutdown();
  EXPECT_EQ(tracker.GetEncryptionState(),
            OnDeviceEncryptionState::kOnDeviceEncryptionStateNotAvailable);

  tracker.RemoveObserver(&observer);
}

}  // namespace

}  // namespace password_manager
