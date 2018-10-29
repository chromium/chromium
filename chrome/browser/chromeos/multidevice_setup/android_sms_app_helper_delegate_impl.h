// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_ANDROID_SMS_APP_HELPER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_ANDROID_SMS_APP_HELPER_DELEGATE_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_app_helper_delegate.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "url/gurl.h"

class Profile;

namespace web_app {
enum class InstallResultCode;
class PendingAppManager;
}  // namespace web_app

namespace chromeos {
namespace multidevice_setup {

class AndroidSmsAppHelperDelegateImpl : public AndroidSmsAppHelperDelegate {
 public:
  explicit AndroidSmsAppHelperDelegateImpl(Profile* profile);
  ~AndroidSmsAppHelperDelegateImpl() override;

 private:
  friend class AndroidSmsAppHelperDelegateImplTest;

  // Note: This constructor should only be used in testing. Right now, objects
  // built using this constructor will segfault on profile_ if
  // LaunchAndroidSmsApp is called. We'll need to fix this once tests for that
  // function are added. See https://crbug.com/876972.
  AndroidSmsAppHelperDelegateImpl(
      web_app::PendingAppManager* pending_app_manager,
      HostContentSettingsMap* host_content_settings_map);
  void OnAppInstalled(bool launch_on_install,
                      const GURL& app_url,
                      web_app::InstallResultCode code);
  void InstallAndroidSmsApp(bool launch_on_install);
  void LaunchAndroidSmsApp();

  // AndroidSmsAppHelperDelegate:
  void InstallAndroidSmsApp() override;
  void InstallAndLaunchAndroidSmsApp() override;

  static const char kMessagesWebAppUrl[];
  web_app::PendingAppManager* pending_app_manager_;
  Profile* profile_;
  HostContentSettingsMap* host_content_settings_map_;
  base::WeakPtrFactory<AndroidSmsAppHelperDelegateImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppHelperDelegateImpl);
};

}  // namespace multidevice_setup
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_ANDROID_SMS_APP_HELPER_DELEGATE_IMPL_H_
