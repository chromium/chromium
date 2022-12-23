// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MULTIDEVICE_SETUP_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MULTIDEVICE_SETUP_SCREEN_H_

#include <string>

#include "base/bind.h"
#include "base/callback.h"
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
    kEncryptedMetadataEmpty = 6,
    kWaitingForGroupPrivateKey = 7,
    kNoEncryptedGroupPrivateKeyReceived = 8,
    kEncryptedGroupPrivateKeyEmpty = 9,
    kLocalDeviceSyncBetterTogetherKeyMissing = 10,
    kGroupPrivateKeyDecryptionFailed = 11,
    kDestroyedBeforeReasonCouldBeDetermined = 12,
    kUnknown = 13,
    kMaxValue = kUnknown
  };

  // Inits `setup_client_` if it was not initialized before.
  void TryInitSetupClient();

  void GetBetterTogetherMetadataStatus();

  void OnGetBetterTogetherMetadataStatus(
      device_sync::BetterTogetherMetadataStatus status);

  void GetGroupPrivateKeyStatus();

  void OnGetGroupPrivateKeyStatus(device_sync::GroupPrivateKeyStatus status);

  void RecordOobeMultideviceScreenSkippedReasonHistogram(
      OobeMultideviceScreenSkippedReason reason);

  static void RecordMultiDeviceSetupOOBEUserChoiceHistogram(
      MultiDeviceSetupOOBEUserChoice value);

  multidevice_setup::MultiDeviceSetupClient* setup_client_ = nullptr;
  device_sync::DeviceSyncClient* device_sync_client_ = nullptr;
  bool skipped_ = false;
  bool skipped_reason_determined_ = false;

  base::WeakPtr<MultiDeviceSetupScreenView> view_;
  ScreenExitCallback exit_callback_;
  base::WeakPtrFactory<MultiDeviceSetupScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MULTIDEVICE_SETUP_SCREEN_H_
