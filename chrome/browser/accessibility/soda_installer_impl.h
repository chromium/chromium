// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_SODA_INSTALLER_IMPL_H_
#define CHROME_BROWSER_ACCESSIBILITY_SODA_INSTALLER_IMPL_H_

#include <map>
#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/component_updater/component_updater_service.h"
#include "components/soda/soda_installer.h"

class PrefService;

namespace update_client {
struct CrxUpdateItem;
}

namespace speech {

// Installer of SODA (Speech On-Device API) for the Live Caption feature on
// non-ChromeOS desktop versions of Chrome browser.
class SodaInstallerImpl : public SodaInstaller,
                          public component_updater::ServiceObserver {
 public:
  SodaInstallerImpl();
  ~SodaInstallerImpl() override;
  SodaInstallerImpl(const SodaInstallerImpl&) = delete;
  SodaInstallerImpl& operator=(const SodaInstallerImpl&) = delete;

  // Currently only implemented in the chromeos-specific subclass.
  base::FilePath GetSodaBinaryPath() const override;

  base::FilePath GetLanguagePath(const std::string& language) const override;

  // SodaInstaller:
  void InstallLanguage(const std::string& language,
                       PrefService* global_prefs) override;
  void UninstallLanguage(const std::string& language,
                         PrefService* global_prefs) override;
  std::vector<std::string> GetAvailableLanguages() const override;

 protected:
  // SodaInstaller:
  void InstallSoda(PrefService* global_prefs) override;
  void UninstallSoda(PrefService* global_prefs) override;

  // component_updater::ServiceObserver:
  void OnEvent(const update_client::CrxUpdateItem& item) override;

  void OnSodaBinaryInstalled();
  void OnSodaLanguagePackInstalled(speech::LanguageCode language_code);

 private:
  void UpdateAndNotifyOnSodaProgress(speech::LanguageCode language_code);

  std::map<speech::LanguageCode, update_client::CrxUpdateItem>
      downloading_components_;

  base::Time soda_binary_install_start_time_;
  base::flat_map<LanguageCode, base::Time> language_pack_install_start_time_;

  base::ScopedObservation<component_updater::ComponentUpdateService,
                          component_updater::ComponentUpdateService::Observer>
      component_updater_observation_{this};

  base::WeakPtrFactory<SodaInstallerImpl> weak_factory_{this};
};

}  // namespace speech

#endif  // CHROME_BROWSER_ACCESSIBILITY_SODA_INSTALLER_IMPL_H_
