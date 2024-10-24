// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/component_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/translate_kit_component_installer.h"
#include "chrome/browser/component_updater/translate_kit_language_pack_component_installer.h"
#include "chrome/browser/on_device_translation/constants.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/services/on_device_translation/public/cpp/features.h"

namespace on_device_translation {
namespace {

// The implementation of ComponentManager.
class ComponentManagerImpl : public ComponentManager {
 public:
  ComponentManagerImpl() = default;
  ~ComponentManagerImpl() override = default;

  ComponentManagerImpl(const ComponentManagerImpl&) = delete;
  ComponentManagerImpl& operator=(const ComponentManagerImpl&) = delete;

  void RegisterTranslateKitComponentImpl() override {
    // Registers the TranslateKit component.
    component_updater::RegisterTranslateKitComponent(
        g_browser_process->component_updater(),
        g_browser_process->local_state(),
        /*force_install=*/true,
        /*registered_callback=*/
        base::BindOnce(
            &component_updater::TranslateKitComponentInstallerPolicy::
                UpdateComponentOnDemand));
  }

  void RegisterTranslateKitLanguagePackComponent(
      LanguagePackKey language_pack) override {
    // Registers the TranslateKit language pack component.
    component_updater::RegisterTranslateKitLanguagePackComponent(
        g_browser_process->component_updater(),
        g_browser_process->local_state(), language_pack,
        base::BindOnce(&component_updater::
                           TranslateKitLanguagePackComponentInstallerPolicy::
                               UpdateComponentOnDemand,
                       language_pack));
  }

  void UninstallTranslateKitLanguagePackComponent(
      LanguagePackKey language_pack) override {
    // Uninstalls the TranslateKit language pack component.
    component_updater::UninstallTranslateKitLanguagePackComponent(
        g_browser_process->component_updater(),
        g_browser_process->local_state(), language_pack);
  }

  base::FilePath GetTranslateKitComponentPath() override {
    // If the path is specified from the command line, use it.
    auto path_from_command_line = GetTranslateKitBinaryPathFromCommandLine();
    if (!path_from_command_line.empty()) {
      return path_from_command_line;
    }

    // Otherwise, use the path from the component updater.
    base::FilePath components_dir;
    base::PathService::Get(component_updater::DIR_COMPONENT_USER,
                           &components_dir);
    CHECK(!components_dir.empty());
    return components_dir.Append(kTranslateKitBinaryInstallationRelativeDir);
  }

 private:
  bool translate_kit_component_registered_ = false;
};

}  // namespace

// static
ComponentManager* ComponentManager::component_manager_for_test_ = nullptr;

// static
ComponentManager& ComponentManager::GetInstance() {
  // If there is a testing manager, use it.
  if (component_manager_for_test_) {
    return *component_manager_for_test_;
  }
  // Otherwise, use the production manager.
  static ComponentManagerImpl manager;
  return manager;
}

// static
void ComponentManager::SetForTesting(ComponentManager* manager) {
  // There should not be any testing manager when setting a testing manager.
  // Also there should be a testing manager when resetting it.
  CHECK_EQ(!component_manager_for_test_, !!manager);
  component_manager_for_test_ = manager;
}

bool ComponentManager::RegisterTranslateKitComponent() {
  // Only register the component once.
  if (translate_kit_component_registered_) {
    return false;
  }
  translate_kit_component_registered_ = true;
  RegisterTranslateKitComponentImpl();
  return true;
}

}  // namespace on_device_translation
