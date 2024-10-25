// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_COMPONENT_MANAGER_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_COMPONENT_MANAGER_H_

#include <optional>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom-forward.h"

namespace on_device_translation {

enum class LanguagePackKey;

// This class handles the TranslateKit component and the language pack
// components.
class ComponentManager {
 public:
  // Returns the singleton instance of ComponentManager.
  static ComponentManager& GetInstance();

  // Sets the singleton instance of ComponentManager for testing.
  static void SetForTesting(ComponentManager* manager);

  // Returns the path of the TranslateKit library. If the path is set by the
  // command line, returns the path from the command line
  // `--translate-kit-binary-path`. Otherwise, returns the path from the global
  // prefs.
  static base::FilePath GetTranslateKitLibraryPath();

  // Returns true if the path of the TranslateKit library is set by the command
  // line `--translate-kit-binary-path`.
  static bool HasTranslateKitLibraryPathFromCommandLine();

  // Returns the language packs that were registered.
  static std::set<LanguagePackKey> GetRegisteredLanguagePacks();
  // Returns the language packs that were installed and ready to use.
  static std::set<LanguagePackKey> GetInstalledLanguagePacks();

  ComponentManager();
  virtual ~ComponentManager();

  // Disallow copy and assign.
  ComponentManager(const ComponentManager&) = delete;
  ComponentManager& operator=(const ComponentManager&) = delete;

  // Registers the TranslateKit component and returns true if this is called for
  // the first time. Otherwise returns false.
  bool RegisterTranslateKitComponent();

  // Registers the language pack component.
  virtual void RegisterTranslateKitLanguagePackComponent(
      LanguagePackKey language_pack) = 0;

  // Uninstalls the language pack component.
  virtual void UninstallTranslateKitLanguagePackComponent(
      LanguagePackKey language_pack) = 0;

  // If the TranslateKit binary path is passed via the command line
  // `--translate-kit-binary-path`, returns the binary path. Otherwise, returns
  // the directory path of the installation location of the TranslateKit binary
  // component.
  // This is called from a launcher thread to allow reading the files under the
  // returned path in the sandboxed process on macOS.
  base::FilePath GetTranslateKitComponentPath();

  // Get the language pack info. If the language pack info is set by the
  // command line `--translate-kit-packages`, returns the language pack info
  // from the command line. Otherwise, returns the language pack info from the
  // global prefs.
  void GetLanguagePackInfo(
      std::vector<mojom::OnDeviceTranslationLanguagePackagePtr>& packages,
      std::vector<base::FilePath>& package_pathes);

  // Returns true if the language pack information is from the command line
  // `--translate-kit-packages`.
  bool HasLanguagePackInfoFromCommandLine() const {
    return !!language_packs_from_command_line_;
  }

 protected:
  // This is called when RegisterTranslateKitComponent() is called for the first
  // time.
  virtual void RegisterTranslateKitComponentImpl() = 0;

  // This is called from GetTranslateKitComponentPath() when the TranslateKit
  // binary path is not passed via the command line.
  virtual base::FilePath GetTranslateKitComponentPathImpl() = 0;

 private:
  // The information of a language pack.
  struct LanguagePackInfo {
    std::string language1;
    std::string language2;
    base::FilePath package_path;
  };

  // Get a list of LanguagePackInfo from the command line flag
  // `--translate-kit-packages`.
  static std::optional<std::vector<LanguagePackInfo>>
  GetLanguagePackInfoFromCommandLine();

  // Whether RegisterTranslateKitComponent() was called.
  bool translate_kit_component_registered_ = false;

  // The LanguagePackInfo from the command line. This is nullopt if the command
  // line flag `--translate-kit-packages` is not set.
  const std::optional<std::vector<LanguagePackInfo>>
      language_packs_from_command_line_;

  // The singleton instance of ComponentManager for testing.
  static ComponentManager* component_manager_for_test_;
};
}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_COMPONENT_MANAGER_H_
