// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_system.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/chrome_app_sorting.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/state_store.h"
#include "extensions/browser/value_store/test_value_store_factory.h"
#include "extensions/browser/value_store/testing_value_store.h"
#include "services/data_decoder/data_decoder_service.h"
#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

using content::BrowserThread;

namespace extensions {

TestExtensionSystem::TestExtensionSystem(Profile* profile)
    : profile_(profile),
      store_factory_(new TestValueStoreFactory()),
      info_map_(new InfoMap()),
      quota_service_(new QuotaService()),
      app_sorting_(new ChromeAppSorting(profile_)) {
#if defined(OS_CHROMEOS)
  if (!user_manager::UserManager::IsInitialized())
    test_user_manager_.reset(new chromeos::ScopedTestUserManager);
#endif
}

TestExtensionSystem::~TestExtensionSystem() = default;

void TestExtensionSystem::Shutdown() {
  if (extension_service_)
    extension_service_->Shutdown();
  in_process_data_decoder_.reset();
}

ExtensionService* TestExtensionSystem::CreateExtensionService(
    const base::CommandLine* command_line,
    const base::FilePath& install_directory,
    bool autoupdate_enabled,
    bool extensions_enabled) {
  state_store_.reset(new StateStore(
      profile_, store_factory_, ValueStoreFrontend::BackendType::RULES, false));
  management_policy_.reset(new ManagementPolicy());
  management_policy_->RegisterProviders(
      ExtensionManagementFactory::GetForBrowserContext(profile_)
          ->GetProviders());
  runtime_data_.reset(new RuntimeData(ExtensionRegistry::Get(profile_)));
  extension_service_.reset(new ExtensionService(
      profile_, command_line, install_directory, ExtensionPrefs::Get(profile_),
      Blacklist::Get(profile_), autoupdate_enabled, extensions_enabled,
      &ready_));

  unzip::SetUnzipperLaunchOverrideForTesting(
      base::BindRepeating(&unzip::LaunchInProcessUnzipper));
  in_process_data_decoder_ =
      std::make_unique<data_decoder::test::InProcessDataDecoder>();

  extension_service_->ClearProvidersForTesting();
  return extension_service_.get();
}

ExtensionService* TestExtensionSystem::extension_service() {
  return extension_service_.get();
}

RuntimeData* TestExtensionSystem::runtime_data() {
  return runtime_data_.get();
}

ManagementPolicy* TestExtensionSystem::management_policy() {
  return management_policy_.get();
}

void TestExtensionSystem::SetExtensionService(ExtensionService* service) {
  extension_service_.reset(service);
}

ServiceWorkerManager* TestExtensionSystem::service_worker_manager() {
  return nullptr;
}

SharedUserScriptMaster* TestExtensionSystem::shared_user_script_master() {
  return NULL;
}

StateStore* TestExtensionSystem::state_store() {
  return state_store_.get();
}

StateStore* TestExtensionSystem::rules_store() {
  return state_store_.get();
}

scoped_refptr<ValueStoreFactory> TestExtensionSystem::store_factory() {
  return store_factory_;
}

InfoMap* TestExtensionSystem::info_map() { return info_map_.get(); }

QuotaService* TestExtensionSystem::quota_service() {
  return quota_service_.get();
}

AppSorting* TestExtensionSystem::app_sorting() {
  return app_sorting_.get();
}

const base::OneShotEvent& TestExtensionSystem::ready() const {
  return ready_;
}

ContentVerifier* TestExtensionSystem::content_verifier() {
  return NULL;
}

std::unique_ptr<ExtensionSet> TestExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  return extension_service()->shared_module_service()->GetDependentExtensions(
      extension);
}

void TestExtensionSystem::InstallUpdate(
    const std::string& extension_id,
    const std::string& public_key,
    const base::FilePath& temp_dir,
    bool install_immediately,
    InstallUpdateCallback install_update_callback) {
  NOTREACHED();
}

bool TestExtensionSystem::FinishDelayedInstallationIfReady(
    const std::string& extension_id,
    bool install_immediately) {
  NOTREACHED();
  return false;
}

TestingValueStore* TestExtensionSystem::value_store() {
  // These tests use TestingValueStore in a way that ensures it only ever mints
  // instances of TestingValueStore.
  return static_cast<TestingValueStore*>(store_factory_->LastCreatedStore());
}

// static
std::unique_ptr<KeyedService> TestExtensionSystem::Build(
    content::BrowserContext* profile) {
  return base::WrapUnique(
      new TestExtensionSystem(static_cast<Profile*>(profile)));
}

void TestExtensionSystem::RecreateAppSorting() {
  app_sorting_.reset(new ChromeAppSorting(profile_));
}

}  // namespace extensions
