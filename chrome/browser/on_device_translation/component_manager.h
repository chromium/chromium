// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_COMPONENT_MANAGER_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_COMPONENT_MANAGER_H_

#include "base/files/file_path.h"

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

  ComponentManager() = default;
  virtual ~ComponentManager() = default;

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
  virtual base::FilePath GetTranslateKitComponentPath() = 0;

 protected:
  // This is called when RegisterTranslateKitComponent() is called for the first
  // time.
  virtual void RegisterTranslateKitComponentImpl() = 0;

 private:
  bool translate_kit_component_registered_ = false;

  // The singleton instance of ComponentManager for testing.
  static ComponentManager* component_manager_for_test_;
};
}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_COMPONENT_MANAGER_H_
