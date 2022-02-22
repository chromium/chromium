// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_APP_MANAGER_H_
#define CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_APP_MANAGER_H_

#include "ash/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {
namespace android_sms {

// Manages setup and cookies for the Messages PWA. If the URL of the installed
// PWA changes, observers are notified of the change.
//
// TODO(https://crbug.com/920781): Delete
// multidevice_setup::AndroidSmsAppHelperDelegate and move its functions to this
// class instead, then remove virtual inheritance here.
class AndroidSmsAppManager
    : virtual public multidevice_setup::AndroidSmsAppHelperDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    ~Observer() override = default;
    virtual void OnInstalledAppUrlChanged() = 0;
  };

  AndroidSmsAppManager();

  AndroidSmsAppManager(const AndroidSmsAppManager&) = delete;
  AndroidSmsAppManager& operator=(const AndroidSmsAppManager&) = delete;

  ~AndroidSmsAppManager() override;

  // If no app is installed, null is returned.
  virtual absl::optional<GURL> GetCurrentAppUrl() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyInstalledAppUrlChanged();

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace android_sms
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos {
namespace android_sms {
using ::ash::android_sms::AndroidSmsAppManager;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_ANDROID_SMS_ANDROID_SMS_APP_MANAGER_H_
