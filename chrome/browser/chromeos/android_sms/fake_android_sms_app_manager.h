// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_FAKE_ANDROID_SMS_APP_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_FAKE_ANDROID_SMS_APP_MANAGER_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/android_sms/android_sms_app_manager.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_android_sms_app_helper_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace chromeos {

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
  ~FakeAndroidSmsAppManager() override;

  void SetInstalledAppUrl(const absl::optional<GURL>& url);

 private:
  // AndroidSmsAppManager:
  absl::optional<GURL> GetCurrentAppUrl() override;

  absl::optional<GURL> url_;

  DISALLOW_COPY_AND_ASSIGN(FakeAndroidSmsAppManager);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_FAKE_ANDROID_SMS_APP_MANAGER_H_
