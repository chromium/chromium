// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MULTIDEVICE_SETUP_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MULTIDEVICE_SETUP_SCREEN_H_

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chromeos/ash/services/device_sync/group_private_key_and_better_together_metadata_status.h"

namespace ash {

class MultiDeviceSetupScreenView;

namespace multidevice_setup {
class MultiDeviceSetupClient;
}

namespace device_sync {
class DeviceSyncClient;
}

namespace quick_start {
class QuickStartMetrics;
}

class MultiDeviceSetupScreen : public BaseScreen {
 public:
  enum class Result { NEXT, NOT_APPLICABLE };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  MultiDeviceSetupScreen(base::WeakPtr<MultiDeviceSetupScreenView> view,
                         const ScreenExitCallback& exit_callback);

  MultiDeviceSetupScreen(const MultiDeviceSetupScreen&) = delete;
  MultiDeviceSetupScreen& operator=(const MultiDeviceSetupScreen&) = delete;

  ~MultiDeviceSetupScreen() override;

  void AddExitCallbackForTesting(const ScreenExitCallback& testing_callback) {
    exit_callback_ = base::BindRepeating(
        [](const ScreenExitCallback& original_callback,
           const ScreenExitCallback& testing_callback, Result result) {
          original_callback.Run(result);
          testing_callback.Run(result);
        },
        exit_callback_, testing_callback);
  }

  void set_multidevice_setup_client_for_testing(
      multidevice_setup::MultiDeviceSetupClient* client) {
    setup_client_ = client;
  }

  void set_device_sync_client_for_testing(
      device_sync::DeviceSyncClient* client) {
    device_sync_client_ = client;
  }

 protected:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

 private:
  friend class MultiDeviceSetupScreenTest;

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other).  Entries should be never modified
  // or deleted.  Only additions possible.
  enum class MultiDeviceSetupOOBEUserChoice {
    kAccepted = 0,
    kDeclined = 1,
    kMaxValue = kDeclined
  };

  // This enum is tied directly to the OobeMultideviceScreenSkippedReason UMA
  // enum defined in //tools/metrics/histograms/enums.xml, and should always
  // reflect it (do not change one without changing the other).  Entries should
  // be never modified or deleted.  Only additions possible.
  enum class OobeMultideviceScreenSkippedReason {
    kPublicSessionOrEphemeralLogin = 0,
    kHostPhoneAlreadySet = 1,
    kDeviceSyncFinishedAndNoEligibleHostPhone = 2,
    kSetupClientNotInitialized = 3,
    kDeviceSyncNotInitializedDuringBetterTogetherMetadataStatusFetch = 4,
    kDeviceSyncNotInitializedDuringGroupPrivateKeyStatusFetch = 5,
    kNoDeviceSyncerSetDuringBetterTogetherMetadataStatusFetch = 6,
    kNoDeviceSyncerSetDuringGroupPrivateKeyStatusFetch = 7,
    kEncryptedMetadataEmpty = 8,
    kWaitingForGroupPrivateKey = 9,
    kNoEncryptedGroupPrivateKeyReceived = 10,
    kEncryptedGroupPrivateKeyEmpty = 11,
    kLocalDeviceSyncBetterTogetherKeyMissing = 12,
    kGroupPrivateKeyDecryptionFailed = 13,
    kDestroyedBeforeReasonCouldBeDetermined = 14,
    kUnknown = 15,
    kMaxValue = kUnknown
  };

  // Inits `setup_client_` if it was not initialized before.
  void TryInitSetupClient();

  // Retrieve CryptAuth device sync better together metadata status for a
  // granular understanding of why this screen might be skipped.
  void GetBetterTogetherMetadataStatus();

  // The callback |device_sync_client_| receives to either emit the
  // OobeMultideviceScreenSkippedReason metric or retrieve the group private key
  // status to determine why this screen might be skipped.
  void OnGetBetterTogetherMetadataStatus(
      device_sync::BetterTogetherMetadataStatus status);

  // When the better together metadata status is kGroupPrivateKeyMissing or
  // kWaitingToProcessDeviceMetadata, this will retrieve the CryptAuth device
  // sync group private key status for a granular understanding of why this
  // screen might be skipped.
  void GetGroupPrivateKeyStatus();

  // The callback |device_sync_client_| receives to emit the
  // OobeMultideviceScreenSkippedReason metric depending on the current group
  // private key status.
  void OnGetGroupPrivateKeyStatus(device_sync::GroupPrivateKeyStatus status);

  // Record the underlying reason why MaybeSkip() would return true.
  void RecordOobeMultideviceScreenSkippedReasonHistogram(
      OobeMultideviceScreenSkippedReason reason);

  // Record Quick Start ScreenClosed if the QS screen enhancements were shown.
  void MaybeRecordQuickStartScreenClosed();

  static void RecordMultiDeviceSetupOOBEUserChoiceHistogram(
      MultiDeviceSetupOOBEUserChoice value);

  raw_ptr<multidevice_setup::MultiDeviceSetupClient> setup_client_ = nullptr;
  raw_ptr<device_sync::DeviceSyncClient> device_sync_client_ = nullptr;
  std::unique_ptr<quick_start::QuickStartMetrics> quick_start_metrics_ =
      nullptr;
  bool skipped_ = false;
  bool skipped_reason_determined_ = false;

  base::WeakPtr<MultiDeviceSetupScreenView> view_;
  ScreenExitCallback exit_callback_;
  base::WeakPtrFactory<MultiDeviceSetupScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MULTIDEVICE_SETUP_SCREEN_H_
