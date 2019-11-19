// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_PAIRING_STATE_TRACKER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_PAIRING_STATE_TRACKER_IMPL_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/android_sms/android_sms_app_manager.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_pairing_state_tracker.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

class Profile;

namespace chromeos {

namespace android_sms {

// Concrete AndroidSmsPairingStateTracker implementation.
class AndroidSmsPairingStateTrackerImpl
    : public multidevice_setup::AndroidSmsPairingStateTracker,
      public network::mojom::CookieChangeListener,
      public AndroidSmsAppManager::Observer {
 public:
  AndroidSmsPairingStateTrackerImpl(
      Profile* profile,
      AndroidSmsAppManager* android_sms_app_manager);
  ~AndroidSmsPairingStateTrackerImpl() override;

  // AndroidSmsPairingStateTracker:
  bool IsAndroidSmsPairingComplete() override;

 private:
  // network::mojom::CookieChangeListener:
  void OnCookieChange(const net::CookieChangeInfo& change) override;

  // AndroidSmsAppManager::Observer:
  void OnInstalledAppUrlChanged() override;

  GURL GetPairingUrl();
  network::mojom::CookieManager* GetCookieManager();

  void AttemptFetchMessagesPairingState();
  void OnCookiesRetrieved(const net::CookieStatusList& cookies,
                          const net::CookieStatusList& excluded_cookies);

  void AddCookieChangeListener();

  Profile* profile_;
  AndroidSmsAppManager* android_sms_app_manager_;

  mojo::Receiver<network::mojom::CookieChangeListener>
      cookie_listener_receiver_{this};
  bool was_paired_on_last_update_ = false;
  base::WeakPtrFactory<AndroidSmsPairingStateTrackerImpl> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsPairingStateTrackerImpl);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_PAIRING_STATE_TRACKER_IMPL_H_
