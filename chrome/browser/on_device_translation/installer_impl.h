// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_INSTALLER_IMPL_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_INSTALLER_IMPL_H_

#include "base/observer_list.h"
#include "components/component_updater/component_updater_service.h"
#include "components/on_device_translation/installer.h"

namespace on_device_translation {

// The chrome-browser implementation for the `OnDeviceTranslationInstaller`.
class OnDeviceTranslationInstallerImpl : public OnDeviceTranslationInstaller {
 public:
  explicit OnDeviceTranslationInstallerImpl(
      component_updater::ComponentUpdateService* cus);
  ~OnDeviceTranslationInstallerImpl() override;

  bool IsInit(PrefService* prefs) const override;
  std::set<LanguagePackKey> InstalledLanguagePacks(
      PrefService* prefs) const override;
  std::set<LanguagePackKey> RegisteredLanguagePacks(
      PrefService* prefs) const override;

  void Init(PrefService* pref_service,
            base::RepeatingClosure on_ready_callback) override;
  bool InstallLanguagePack(LanguagePackKey language_pack,
                           PrefService* pref_service) override;
  bool UnInstallLanguagePack(LanguagePackKey language_pack,
                             PrefService* pref_service) override;
  void AddOserver(Observer* observer) override;

 private:
  // Called when a language pack has finished being installed.
  void OnLanguagePackInstalled(LanguagePackKey language_pack);

  std::set<LanguagePackKey> installed_language_packs_;
  raw_ptr<component_updater::ComponentUpdateService> cus_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<OnDeviceTranslationInstallerImpl> weak_ptr_factory_{
      this};
};

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_INSTALLER_IMPL_H_
