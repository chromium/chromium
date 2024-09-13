// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/desktop_android_extension_system.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/value_store/value_store_factory_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/app_runtime/app_runtime_api.h"
#include "extensions/browser/api/declarative_net_request/install_index_helper.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/null_app_sorting.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/service_worker_manager.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/file_util.h"

using content::BrowserContext;
namespace extensions {

namespace {

// A factory implementation to construct and return the
// DesktopAndroidExtensionSystem.
class DesktopAndroidExtensionSystemFactory : public ExtensionSystemProvider {
 public:
  DesktopAndroidExtensionSystemFactory()
      : ExtensionSystemProvider(
            "DesktopAndroidExtensionSystem",
            BrowserContextDependencyManager::GetInstance()) {
    DependsOn(ExtensionPrefsFactory::GetInstance());
    DependsOn(ExtensionRegistryFactory::GetInstance());
  }
  DesktopAndroidExtensionSystemFactory(
      const DesktopAndroidExtensionSystemFactory&) = delete;
  DesktopAndroidExtensionSystemFactory& operator=(
      const DesktopAndroidExtensionSystemFactory&) = delete;
  ~DesktopAndroidExtensionSystemFactory() override = default;

  // ExtensionSystemProvider implementation:
  ExtensionSystem* GetForBrowserContext(
      content::BrowserContext* context) override {
    return static_cast<DesktopAndroidExtensionSystem*>(
        GetInstance()->GetServiceForBrowserContext(context, true));
  }

  static DesktopAndroidExtensionSystemFactory* GetInstance() {
    static base::NoDestructor<DesktopAndroidExtensionSystemFactory> g_instance;
    return g_instance.get();
  }

 private:
  // BrowserContextKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override {
    return std::make_unique<DesktopAndroidExtensionSystem>(context);
  }
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    // Use a separate instance for incognito.
    return context;
  }
  bool ServiceIsCreatedWithBrowserContext() const override { return true; }
};

// A minimal stub implementation of the ExtensionRegistrar::Delegate.
class DesktopAndroidExtensionRegistrarDelegate
    : public ExtensionRegistrar::Delegate {
 public:
  explicit DesktopAndroidExtensionRegistrarDelegate(
      content::BrowserContext* browser_context)
      : browser_context_(browser_context) {
    DCHECK(browser_context_);
  }
  ~DesktopAndroidExtensionRegistrarDelegate() override = default;

  // ExtensionRegistrar::Delegate:
  void PreAddExtension(const Extension* extension,
                       const Extension* old_extension) override {}
  void PostActivateExtension(
      scoped_refptr<const Extension> extension) override {}
  void PostDeactivateExtension(
      scoped_refptr<const Extension> extension) override {}
  void LoadExtensionForReload(
      const ExtensionId& extension_id,
      const base::FilePath& path,
      ExtensionRegistrar::LoadErrorBehavior load_error_behavior) override {
    NOTIMPLEMENTED();
  }
  bool CanEnableExtension(const Extension* extension) override { return true; }
  bool CanDisableExtension(const Extension* extension) override { return true; }
  bool ShouldBlockExtension(const Extension* extension) override {
    return false;
  }

 private:
  raw_ptr<content::BrowserContext> browser_context_;  // Not owned.
};

}  // namespace

DesktopAndroidExtensionSystem::DesktopAndroidExtensionSystem(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      store_factory_(base::MakeRefCounted<value_store::ValueStoreFactoryImpl>(
          browser_context->GetPath())) {}

DesktopAndroidExtensionSystem::~DesktopAndroidExtensionSystem() = default;

// static
ExtensionSystemProvider* DesktopAndroidExtensionSystem::GetFactory() {
  return DesktopAndroidExtensionSystemFactory::GetInstance();
}

void DesktopAndroidExtensionSystem::Shutdown() {}

bool DesktopAndroidExtensionSystem::AddExtension(
    scoped_refptr<Extension> extension,
    std::string& error) {
  // This code is normally handled as part of the UnpackedInstaller, which is
  // not (yet) included in desktop android builds.
  base::expected<base::Value::Dict, std::string> index_result =
      declarative_net_request::InstallIndexHelper::
          IndexAndPersistRulesOnInstall(*extension);
  if (!index_result.has_value()) {
    error = std::move(index_result.error());
    return false;
  }

  // This is normally handled by ExtensionService, and should likely be moved
  // to ExtensionRegistrar.
  ExtensionPrefs::Get(browser_context_)
      ->OnExtensionInstalled(extension.get(), Extension::ENABLED,
                             syncer::StringOrdinal(),
                             kInstallFlagInstallImmediately, std::string(),
                             std::move(index_result.value()));

  registrar_->AddExtension(std::move(extension));
  return true;
}

void DesktopAndroidExtensionSystem::InitForRegularProfile(
    bool extensions_enabled) {
  registrar_delegate_ =
      std::make_unique<DesktopAndroidExtensionRegistrarDelegate>(
          browser_context_);
  registrar_ = std::make_unique<ExtensionRegistrar>(browser_context_,
                                                    registrar_delegate_.get());

  service_worker_manager_ =
      std::make_unique<ServiceWorkerManager>(browser_context_);
  quota_service_ = std::make_unique<QuotaService>();
  user_script_manager_ = std::make_unique<UserScriptManager>(browser_context_);

  ready_.Signal();
}

ExtensionService* DesktopAndroidExtensionSystem::extension_service() {
  return nullptr;
}

ManagementPolicy* DesktopAndroidExtensionSystem::management_policy() {
  return nullptr;
}

ServiceWorkerManager* DesktopAndroidExtensionSystem::service_worker_manager() {
  return service_worker_manager_.get();
}

UserScriptManager* DesktopAndroidExtensionSystem::user_script_manager() {
  return user_script_manager_.get();
}

StateStore* DesktopAndroidExtensionSystem::state_store() {
  return nullptr;
}

StateStore* DesktopAndroidExtensionSystem::rules_store() {
  return nullptr;
}

StateStore* DesktopAndroidExtensionSystem::dynamic_user_scripts_store() {
  return nullptr;
}

scoped_refptr<value_store::ValueStoreFactory>
DesktopAndroidExtensionSystem::store_factory() {
  return store_factory_;
}

QuotaService* DesktopAndroidExtensionSystem::quota_service() {
  return quota_service_.get();
}

AppSorting* DesktopAndroidExtensionSystem::app_sorting() {
  return nullptr;
}

const base::OneShotEvent& DesktopAndroidExtensionSystem::ready() const {
  return ready_;
}

bool DesktopAndroidExtensionSystem::is_ready() const {
  return ready_.is_signaled();
}

ContentVerifier* DesktopAndroidExtensionSystem::content_verifier() {
  return nullptr;
}

std::unique_ptr<ExtensionSet>
DesktopAndroidExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  return std::make_unique<ExtensionSet>();
}

void DesktopAndroidExtensionSystem::InstallUpdate(
    const std::string& extension_id,
    const std::string& public_key,
    const base::FilePath& temp_dir,
    bool install_immediately,
    InstallUpdateCallback install_update_callback) {
  NOTREACHED();
}

void DesktopAndroidExtensionSystem::PerformActionBasedOnOmahaAttributes(
    const std::string& extension_id,
    const base::Value::Dict& attributes) {
  NOTREACHED();
}

bool DesktopAndroidExtensionSystem::FinishDelayedInstallationIfReady(
    const std::string& extension_id,
    bool install_immediately) {
  NOTREACHED();
}

}  // namespace extensions
