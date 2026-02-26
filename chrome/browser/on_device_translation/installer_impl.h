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
  OnDeviceTranslationInstallerImpl();
  ~OnDeviceTranslationInstallerImpl() override;

  bool IsInit() const override;
  std::set<LanguagePackKey> InstalledLanguagePacks() const override;
  std::set<LanguagePackKey> RegisteredLanguagePacks() const override;
  base::FilePath GetLibraryPath() const override;
  base::FilePath GetLanguagePackPath(
      LanguagePackKey language_pack) const override;

  void Init(base::RepeatingClosure on_ready_callback) override;
  void InstallLanguagePack(LanguagePackKey language_pack) override;
  void UnInstallLanguagePack(LanguagePackKey language_pack) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  // We hide away the logic to notify observers.
  class Notifier;
  std::unique_ptr<Notifier> notifier_;
  base::WeakPtrFactory<OnDeviceTranslationInstallerImpl> weak_ptr_factory_{
      this};
};

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_INSTALLER_IMPL_H_
