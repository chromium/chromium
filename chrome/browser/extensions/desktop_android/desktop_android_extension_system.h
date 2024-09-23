// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_EXTENSION_SYSTEM_H_
#define CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_EXTENSION_SYSTEM_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_system.h"

#if !BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
#error "This file is only used for the experimental desktop-android build."
#endif

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace value_store {
class ValueStoreFactory;
}

namespace extensions {
class ExtensionSystemProvider;

////////////////////////////////////////////////////////////////////////////////
// S  T  O  P
// ALL THIS CODE WILL BE DELETED.
// THINK TWICE (OR THRICE) BEFORE ADDING MORE.
//
// The details:
// This is part of an experimental desktop-android build and allows us to
// bootstrap the extension system by incorporating a lightweight extensions
// runtime into the chrome binary. This allows us to do things like load
// extensions in tests and exercise code in these builds without needing to have
// the entirety of the //chrome/browser/extensions system either compiled and
// implemented (which is a massive undertaking) or gracefully if-def'd out
// (which is a massive amount of technical debt).
// This approach, by comparison, allows us to have a minimal interface in the
// chrome browser that mostly relies on the top-level //extensions layer, along
// with small bits of the //chrome code that compile cleanly on the
// experimental desktop-android build.
//
// This entire class should go away. Instead of adding new functionality here,
// it should be added in a location that can be shared across desktop-android
// and other desktop builds. In practice, this means:
// * Pulling the code up to //extensions. If it can be cleanly segmented from
//   the //chrome layer, this is preferable. It gets cleanly included across
//   all builds, encourages proper separation of concerns, and reduces the
//   interdependency between features.
// * Including the functionality in the desktop-android build. This can be done
//   for //chrome sources that do not have any dependencies on areas that
//   cannot be included in desktop-android (such as dependencies on `Browser`
//   or native UI code).
//
// TODO(https://crbug.com/356905053): Delete this class once desktop-android
// properly leverages the extension system.
////////////////////////////////////////////////////////////////////////////////
class DesktopAndroidExtensionSystem : public ExtensionSystem {
 public:
  using InstallUpdateCallback = ExtensionSystem::InstallUpdateCallback;
  explicit DesktopAndroidExtensionSystem(
      content::BrowserContext* browser_context);

  DesktopAndroidExtensionSystem(const DesktopAndroidExtensionSystem&) = delete;
  DesktopAndroidExtensionSystem& operator=(
      const DesktopAndroidExtensionSystem&) = delete;

  ~DesktopAndroidExtensionSystem() override;

  // Returns the singleton instance of the ExtensionSystemProvider to construct
  // the DesktopAndroidExtensionSystem.
  static ExtensionSystemProvider* GetFactory();

  // KeyedService implementation:
  void Shutdown() override;

  bool AddExtension(scoped_refptr<Extension> extension, std::string& error);

  // ExtensionSystem implementation:
  void InitForRegularProfile(bool extensions_enabled) override;
  ExtensionService* extension_service() override;
  ManagementPolicy* management_policy() override;
  ServiceWorkerManager* service_worker_manager() override;
  UserScriptManager* user_script_manager() override;
  StateStore* state_store() override;
  StateStore* rules_store() override;
  StateStore* dynamic_user_scripts_store() override;
  scoped_refptr<value_store::ValueStoreFactory> store_factory() override;
  QuotaService* quota_service() override;
  AppSorting* app_sorting() override;
  const base::OneShotEvent& ready() const override;
  bool is_ready() const override;
  ContentVerifier* content_verifier() override;
  std::unique_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) override;
  void InstallUpdate(const std::string& extension_id,
                     const std::string& public_key,
                     const base::FilePath& temp_dir,
                     bool install_immediately,
                     InstallUpdateCallback install_update_callback) override;
  void PerformActionBasedOnOmahaAttributes(
      const std::string& extension_id,
      const base::Value::Dict& attributes) override;
  bool FinishDelayedInstallationIfReady(const std::string& extension_id,
                                        bool install_immediately) override;

 private:
  raw_ptr<content::BrowserContext> browser_context_;  // Not owned.

  std::unique_ptr<ServiceWorkerManager> service_worker_manager_;
  std::unique_ptr<QuotaService> quota_service_;
  std::unique_ptr<UserScriptManager> user_script_manager_;

  scoped_refptr<value_store::ValueStoreFactory> store_factory_;

  std::unique_ptr<ExtensionRegistrar::Delegate> registrar_delegate_;
  std::unique_ptr<ExtensionRegistrar> registrar_;

  // Signaled when the extension system has completed its startup tasks.
  base::OneShotEvent ready_;

  base::WeakPtrFactory<DesktopAndroidExtensionSystem> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_DESKTOP_ANDROID_DESKTOP_ANDROID_EXTENSION_SYSTEM_H_
