// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_system.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/blocklist.h"
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
#include "components/value_store/test_value_store_factory.h"
#include "components/value_store/testing_value_store.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/state_store.h"
#include "extensions/browser/user_script_manager.h"
#include "services/data_decoder/data_decoder_service.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#endif

using content::BrowserThread;

namespace extensions {

TestExtensionSystem::TestExtensionSystem(Profile* profile)
    : profile_(profile),
      store_factory_(new value_store::TestValueStoreFactory()),
      state_store_(new StateStore(profile_,
                                  store_factory_,
                                  StateStore::BackendType::RULES,
                                  false)),
      info_map_(new InfoMap()),
      quota_service_(new QuotaService()),
      app_sorting_(new ChromeAppSorting(profile_)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!user_manager::UserManager::IsInitialized()) {
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<user_manager::FakeUserManager>());
  }
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
  return CreateExtensionService(command_line, install_directory,
                                base::FilePath(), autoupdate_enabled,
                                extensions_enabled);
}

ExtensionService* TestExtensionSystem::CreateExtensionService(
    const base::CommandLine* command_line,
    const base::FilePath& install_directory,
    const base::FilePath& unpacked_install_directory,
    bool autoupdate_enabled,
    bool extensions_enabled) {
  management_policy_ = std::make_unique<ManagementPolicy>();
  management_policy_->RegisterProviders(
      ExtensionManagementFactory::GetForBrowserContext(profile_)
          ->GetProviders());
  extension_service_ = std::make_unique<ExtensionService>(
      profile_, command_line, install_directory, unpacked_install_directory,
      ExtensionPrefs::Get(profile_), Blocklist::Get(profile_),
      autoupdate_enabled, extensions_enabled, &ready_);

  unzip::SetUnzipperLaunchOverrideForTesting(
      base::BindRepeating(&unzip::LaunchInProcessUnzipper));
  in_process_data_decoder_ =
      std::make_unique<data_decoder::test::InProcessDataDecoder>();

  extension_service_->ClearProvidersForTesting();
  return extension_service_.get();
}

void TestExtensionSystem::CreateUserScriptManager() {
  user_script_manager_ = std::make_unique<UserScriptManager>(profile_);
}

ExtensionService* TestExtensionSystem::extension_service() {
  return extension_service_.get();
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

UserScriptManager* TestExtensionSystem::user_script_manager() {
  return user_script_manager_.get();
}

StateStore* TestExtensionSystem::state_store() {
  return state_store_.get();
}

StateStore* TestExtensionSystem::rules_store() {
  return state_store_.get();
}

StateStore* TestExtensionSystem::dynamic_user_scripts_store() {
  return state_store_.get();
}

scoped_refptr<value_store::ValueStoreFactory>
TestExtensionSystem::store_factory() {
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

bool TestExtensionSystem::is_ready() const {
  return ready_.is_signaled();
}

ContentVerifier* TestExtensionSystem::content_verifier() {
  return nullptr;
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

void TestExtensionSystem::PerformActionBasedOnOmahaAttributes(
    const std::string& extension_id,
    const base::Value& attributes) {}

bool TestExtensionSystem::FinishDelayedInstallationIfReady(
    const std::string& extension_id,
    bool install_immediately) {
  NOTREACHED();
  return false;
}

value_store::TestingValueStore* TestExtensionSystem::value_store() {
  // These tests use TestingValueStore in a way that ensures it only ever mints
  // instances of TestingValueStore.
  return static_cast<value_store::TestingValueStore*>(
      store_factory_->LastCreatedStore());
}

// static
std::unique_ptr<KeyedService> TestExtensionSystem::Build(
    content::BrowserContext* profile) {
  return base::WrapUnique(
      new TestExtensionSystem(static_cast<Profile*>(profile)));
}

void TestExtensionSystem::RecreateAppSorting() {
  app_sorting_ = std::make_unique<ChromeAppSorting>(profile_);
}

}  // namespace extensions
