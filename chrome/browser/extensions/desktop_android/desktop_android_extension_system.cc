// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/desktop_android_extension_system.h"

#include <memory>
#include <string>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "chrome/browser/extensions/chrome_extension_registrar_delegate.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/ui/webui/extensions/extensions_internals_source.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/value_store/value_store_factory_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/url_data_source.h"
#include "extensions/browser/api/app_runtime/app_runtime_api.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/null_app_sorting.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/service_worker_manager.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

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

}  // namespace

DesktopAndroidExtensionSystem::DesktopAndroidExtensionSystem(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      // TODO(crbug.com/356905053): Provide real sorting once the web app story
      // on Android is finalized.
      app_sorting_(std::make_unique<NullAppSorting>()),
      // TODO(crbug.com/408523607): Populate ManagementPolicy with actual
      // policies.
      management_policy_(std::make_unique<ManagementPolicy>()),
      store_factory_(base::MakeRefCounted<value_store::ValueStoreFactoryImpl>(
          browser_context->GetPath())) {}

DesktopAndroidExtensionSystem::~DesktopAndroidExtensionSystem() = default;

// static
ExtensionSystemProvider* DesktopAndroidExtensionSystem::GetFactory() {
  return DesktopAndroidExtensionSystemFactory::GetInstance();
}

void DesktopAndroidExtensionSystem::Shutdown() {}

void DesktopAndroidExtensionSystem::InitForRegularProfile(
    bool extensions_enabled) {
  if (is_ready()) {
    return;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool allow_noisy_errors =
      !command_line->HasSwitch(::switches::kNoErrorDialogs);
  LoadErrorReporter::Init(allow_noisy_errors);

  registrar_delegate_ = std::make_unique<ChromeExtensionRegistrarDelegate>(
      Profile::FromBrowserContext(browser_context_));
  registrar_ = ExtensionRegistrar::Get(browser_context_);
  registrar_->Init(
      registrar_delegate_.get(), extensions_enabled,
      base::CommandLine::ForCurrentProcess(),
      browser_context_->GetPath().AppendASCII(kInstallDirectoryName),
      browser_context_->GetPath().AppendASCII(kUnpackedInstallDirectoryName));
  registrar_delegate_->Init(registrar_.get());

  service_worker_manager_ =
      std::make_unique<ServiceWorkerManager>(browser_context_);
  quota_service_ = std::make_unique<QuotaService>();
  user_script_manager_ = std::make_unique<UserScriptManager>(browser_context_);

  if (!browser_context_->IsOffTheRecord()) {
    // Make the chrome://extension-icon/ resource available. Only do this for
    // non-incognito profiles because OffTheRecordProfileImpl adds its own.
    // Also, this mimics the behavior of ChromeExtensionSystem.
    Profile* profile = Profile::FromBrowserContext(browser_context_);
    content::URLDataSource::Add(browser_context_,
                                std::make_unique<ExtensionIconSource>(profile));

    // Register the source for the chrome://extensions-internals page.
    content::URLDataSource::Add(
        profile, std::make_unique<ExtensionsInternalsSource>(profile));
  }

  ready_.Signal();
}

ExtensionService* DesktopAndroidExtensionSystem::extension_service() {
  return nullptr;
}

ManagementPolicy* DesktopAndroidExtensionSystem::management_policy() {
  return management_policy_.get();
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
  return app_sorting_.get();
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
  return SharedModuleService::Get(browser_context_)
      ->GetDependentExtensions(extension);
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

}  // namespace extensions
