// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ANDROID_SMS_FAKE_ANDROID_SMS_APP_MANAGER_H_
#define CHROME_BROWSER_ASH_ANDROID_SMS_FAKE_ANDROID_SMS_APP_MANAGER_H_

#include "chrome/browser/ash/android_sms/android_sms_app_manager.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_android_sms_app_helper_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {
namespace android_sms {

// Test AndroidSmsAppManager implementation.
//
// TODO(https://crbug.com/920781): Delete
// multidevice_setup::FakeAndroidSmsAppHelperDelegate and move its functions to
// this class instead, then remove virtual inheritance in that class.
class FakeAndroidSmsAppManager
    : public AndroidSmsAppManager,
      public multidevice_setup::FakeAndroidSmsAppHelperDelegate {
 public:
  FakeAndroidSmsAppManager();

  FakeAndroidSmsAppManager(const FakeAndroidSmsAppManager&) = delete;
  FakeAndroidSmsAppManager& operator=(const FakeAndroidSmsAppManager&) = delete;

  ~FakeAndroidSmsAppManager() override;

  void SetInstalledAppUrl(const absl::optional<GURL>& url);

 private:
  // AndroidSmsAppManager:
  absl::optional<GURL> GetCurrentAppUrl() override;

  absl::optional<GURL> url_;
};

}  // namespace android_sms
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ANDROID_SMS_FAKE_ANDROID_SMS_APP_MANAGER_H_
