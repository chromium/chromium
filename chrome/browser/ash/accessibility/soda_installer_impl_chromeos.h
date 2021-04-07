// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SODA_INSTALLER_IMPL_CHROMEOS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SODA_INSTALLER_IMPL_CHROMEOS_H_

#include "base/files/file_path.h"
#include "chrome/browser/accessibility/soda_installer.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"

class PrefService;
class OnDeviceSpeechRecognizerTest;

namespace ash {
class DictationTest;
}  // namespace ash

namespace speech {

// Installer of SODA (Speech On-Device API) for the Live Caption feature on
// ChromeOS.
class SodaInstallerImplChromeOS : public SodaInstaller {
 public:
  SodaInstallerImplChromeOS();
  ~SodaInstallerImplChromeOS() override;
  SodaInstallerImplChromeOS(const SodaInstallerImplChromeOS&) = delete;
  SodaInstallerImplChromeOS& operator=(const SodaInstallerImplChromeOS&) =
      delete;

  // Where the SODA DLC was installed. Cached on completed installation.
  // Empty if SODA DLC not installed yet.
  base::FilePath GetSodaBinaryPath() const override;

  // Where the SODA language pack DLC was installed. Cached on completed
  // installation. Empty if not installed yet.
  base::FilePath GetLanguagePath() const override;

  // SodaInstaller:
  void InstallSoda(PrefService* prefs) override;
  void InstallLanguage(PrefService* prefs) override;
  bool IsSodaInstalled() const override;
  bool IsLanguageInstalled(
      const std::string& locale_or_language) const override;

 private:
  friend class ::ash::DictationTest;
  friend class ::OnDeviceSpeechRecognizerTest;

  // SodaInstaller:
  // Here "uninstall" is used in the DLC sense of the term: Uninstallation will
  // disable a DLC but not immediately remove it from disk.
  // Once a refcount to the DLC reaches 0 (meaning all profiles which had it
  // installed have called to uninstall it), the DLC will remain in cache; if it
  // is then not installed within a (DLC-service-defined) window of time, the
  // DLC is automatically purged from disk.
  void UninstallSoda(PrefService* global_prefs) override;

  void SetSodaBinaryPath(base::FilePath new_path);
  void SetLanguagePath(base::FilePath new_path);

  // These functions are the InstallCallbacks for DlcserviceClient::Install().
  void OnSodaInstalled(
      const chromeos::DlcserviceClient::InstallResult& install_result);
  void OnLanguageInstalled(
      const chromeos::DlcserviceClient::InstallResult& install_result);

  // These functions are the ProgressCallbacks for DlcserviceClient::Install().
  void OnSodaProgress(double progress);
  void OnLanguageProgress(double progress);

  void OnSodaCombinedProgress();

  // This is the UninstallCallback for DlcserviceClient::Uninstall().
  void OnDlcUninstalled(const std::string& dlc_id, const std::string& err);

  // When true, IsSodaInstalled() will return true. This may be used by tests
  // that need to pretend soda is installed before using
  // FakeSpeechRecognitionService.
  bool soda_installed_for_test_ = false;

  bool is_soda_downloading_ = false;
  bool is_language_downloading_ = false;

  double soda_progress_ = 0.0;
  double language_progress_ = 0.0;

  base::FilePath soda_lib_path_;
  base::FilePath language_path_;
};

}  // namespace speech

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SODA_INSTALLER_IMPL_CHROMEOS_H_
