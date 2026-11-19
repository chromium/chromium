// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_INSTALLER_IMPL_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_INSTALLER_IMPL_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "chrome/browser/on_device_translation/library_source.h"
#include "components/component_updater/component_updater_service.h"
#include "components/on_device_translation/installer.h"
#include "components/on_device_translation/public/language_pack.h"

namespace on_device_translation {

// The chrome-browser implementation for the `OnDeviceTranslationInstaller`.
//
// The TranslateKit library comes from a `TranslateKitLibrarySource`, which
// differs per platform. Language packs are components on all platforms.
class OnDeviceTranslationInstallerImpl
    : public OnDeviceTranslationInstaller,
      public TranslateKitLibrarySource::Client {
 public:
  // Uses `TranslateKitLibrarySource::Create()` to get the library.
  OnDeviceTranslationInstallerImpl();
  explicit OnDeviceTranslationInstallerImpl(
      std::unique_ptr<TranslateKitLibrarySource> library_source);
  ~OnDeviceTranslationInstallerImpl() override;

  // OnDeviceTranslationInstaller:
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

  // TranslateKitLibrarySource::Client:
  void OnLibraryChanged() override;

 private:
  base::FilePath GetFilePathFromCommandLine(
      LanguagePackKey language_pack) const;
  std::set<LanguagePackKey> GetLanguagePackKeysFromCommandLine() const;

  const std::unique_ptr<TranslateKitLibrarySource> library_source_;
  // Callbacks passed to `Init()` that wait for the library.
  std::vector<base::RepeatingClosure> on_ready_callbacks_;
  // We hide away the logic to notify observers.
  class Notifier;
  std::unique_ptr<Notifier> notifier_;
  // The LanguagePackInfo from the command line. This is nullopt if the
  // command line flag `--translate-kit-packages` is not set.
  const std::optional<std::map<LanguagePackKey, base::FilePath>>
      language_packs_from_command_line_;
  base::WeakPtrFactory<OnDeviceTranslationInstallerImpl> weak_ptr_factory_{
      this};
};

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_INSTALLER_IMPL_H_
