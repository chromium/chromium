// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_APP_LAUNCHER_H_
#define CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_APP_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ash/app_mode/kiosk_profile_loader.h"

class AccountId;

namespace base {
class FilePath;
}

namespace chromeos {

// Class responsible for launching the demo app under a kiosk session.
class DemoAppLauncher : public KioskProfileLoader::Delegate {
 public:
  DemoAppLauncher();
  ~DemoAppLauncher() override;

  void StartDemoAppLaunch();

  static bool IsDemoAppSession(const AccountId& account_id);
  static void SetDemoAppPathForTesting(const base::FilePath& path);

  static const char kDemoAppId[];

 private:
  friend class DemoAppLauncherTest;

  // KioskProfileLoader::Delegate overrides:
  void OnProfileLoaded(Profile* profile) override;
  void OnProfileLoadFailed(KioskAppLaunchError::Error error) override;
  void OnOldEncryptionDetected(const UserContext& user_context) override;

  std::unique_ptr<KioskProfileLoader> kiosk_profile_loader_;

  static base::FilePath* demo_app_path_;

  DISALLOW_COPY_AND_ASSIGN(DemoAppLauncher);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to chrome/browser/ash/.
namespace ash {
using ::chromeos::DemoAppLauncher;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_DEMO_MODE_DEMO_APP_LAUNCHER_H_
