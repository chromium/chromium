// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_PAIRING_LOST_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_PAIRING_LOST_NOTIFIER_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace chromeos {

namespace multidevice_setup {
class AndroidSmsAppHelperDelegate;
}  // namespace multidevice_setup

namespace android_sms {

// Displays a notification when a user loses pairing between their phone and
// Chromebook.
class PairingLostNotifier
    : public multidevice_setup::MultiDeviceSetupClient::Observer {
 public:
  PairingLostNotifier(
      Profile* profile,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      PrefService* pref_service,
      multidevice_setup::AndroidSmsAppHelperDelegate*
          android_sms_app_helper_delegate);
  ~PairingLostNotifier() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  // multidevice_setup::MultiDeviceSetupClient::Observer:
  void OnFeatureStatesChanged(
      const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
          feature_states_map) override;

  void HandleMessagesFeatureState();
  void HandleSetUpFeatureState();

  void ShowPairingLostNotification();
  void ClosePairingLostNotificationIfVisible();
  void OnPairingLostNotificationClick(base::Optional<int> button_index);

  Profile* profile_;
  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client_;
  PrefService* pref_service_;
  multidevice_setup::AndroidSmsAppHelperDelegate*
      android_sms_app_helper_delegate_;

  base::WeakPtrFactory<PairingLostNotifier> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PairingLostNotifier);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_PAIRING_LOST_NOTIFIER_H_
