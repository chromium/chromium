// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/app_launch_utils.h"

#include "base/macros.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/startup_app_launcher.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_launcher.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash {

namespace {

// The list of prefs that are reset on the start of each kiosk session.
const char* const kPrefsToReset[] = {"settings.accessibility",  // ChromeVox
                                     "settings.a11y", "ash.docked_magnifier",
                                     "settings.tts"};

// This vector is used in tests when they want to replace |kPrefsToReset| with
// their own list.
std::vector<std::string>* test_prefs_to_reset = nullptr;

}  // namespace

// A simple manager for the app launch that starts the launch
// and deletes itself when the launch finishes. On launch failure,
// it exits the browser process.
class AppLaunchManager : public StartupAppLauncher::Delegate {
 public:
  AppLaunchManager(Profile* profile, const KioskAppId& kiosk_app_id) {
    CHECK(kiosk_app_id.type != KioskAppType::kArcApp);

    if (kiosk_app_id.type == KioskAppType::kChromeApp)
      app_launcher_ = std::make_unique<StartupAppLauncher>(
          profile, *kiosk_app_id.app_id, this);
    else
      app_launcher_ = std::make_unique<WebKioskAppLauncher>(
          profile, this, *kiosk_app_id.account_id);
  }

  void Start() { app_launcher_->Initialize(); }

 private:
  ~AppLaunchManager() override {}

  void Cleanup() { delete this; }

  // KioskAppLauncher::Delegate:
  void InitializeNetwork() override {
    // This is on crash-restart path and assumes network is online.
    app_launcher_->ContinueWithNetworkReady();
  }
  bool IsNetworkReady() const override {
    // See comments above. Network is assumed to be online here.
    return true;
  }
  bool ShouldSkipAppInstallation() const override {
    // Given that this delegate does not reliably report whether the network is
    // ready, avoid making app update checks - this might take a while if
    // network is not online. Also, during crash-restart, we should continue
    // with the same app version as the restored session.
    return true;
  }
  void OnAppInstalling() override {}
  void OnAppPrepared() override { app_launcher_->LaunchApp(); }
  void OnAppLaunched() override {}
  void OnAppWindowCreated() override { Cleanup(); }
  void OnLaunchFailed(KioskAppLaunchError::Error error) override {
    KioskAppLaunchError::Save(error);
    chrome::AttemptUserExit();
    Cleanup();
  }
  bool IsShowingNetworkConfigScreen() const override { return false; }

  std::unique_ptr<KioskAppLauncher> app_launcher_;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchManager);
};

void LaunchAppOrDie(Profile* profile, const KioskAppId& kiosk_app_id) {
  // AppLaunchManager manages its own lifetime.
  (new AppLaunchManager(profile, kiosk_app_id))->Start();
}

void ResetEphemeralKioskPreferences(PrefService* prefs) {
  CHECK(prefs);
  CHECK(user_manager::UserManager::IsInitialized() &&
        user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp());
  for (size_t pref_id = 0;
       pref_id < (test_prefs_to_reset ? test_prefs_to_reset->size()
                                      : base::size(kPrefsToReset));
       pref_id++) {
    const std::string branch_path = test_prefs_to_reset
                                        ? (*test_prefs_to_reset)[pref_id]
                                        : kPrefsToReset[pref_id];
    prefs->ClearPrefsWithPrefixSilently(branch_path);
  }
}

void SetEphemeralKioskPreferencesListForTesting(
    std::vector<std::string>* prefs) {
  test_prefs_to_reset = prefs;
}

}  // namespace ash
