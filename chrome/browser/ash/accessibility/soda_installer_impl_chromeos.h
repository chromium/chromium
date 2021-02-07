// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_SODA_INSTALLER_IMPL_CHROMEOS_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_SODA_INSTALLER_IMPL_CHROMEOS_H_

#include "base/files/file_path.h"
#include "chrome/browser/accessibility/soda_installer.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"

class PrefService;

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
  void UninstallSoda(PrefService* global_prefs) override;

 private:
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

  bool is_soda_downloading_ = false;
  bool is_language_downloading_ = false;

  double soda_progress_ = 0.0;
  double language_progress_ = 0.0;

  base::FilePath soda_lib_path_;
  base::FilePath language_path_;
};

}  // namespace speech

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_SODA_INSTALLER_IMPL_CHROMEOS_H_
