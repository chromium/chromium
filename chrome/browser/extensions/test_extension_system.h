// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SYSTEM_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SYSTEM_H_

#include <memory>

#include "base/one_shot_event.h"
#include "extensions/browser/extension_system.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#endif

class Profile;
class TestingValueStore;

namespace base {
class CommandLine;
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class TestValueStoreFactory;

// Test ExtensionSystem, for use with TestingProfile.
class TestExtensionSystem : public ExtensionSystem {
 public:
  using InstallUpdateCallback = ExtensionSystem::InstallUpdateCallback;
  explicit TestExtensionSystem(Profile* profile);
  ~TestExtensionSystem() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Creates an ExtensionService initialized with the testing profile and
  // returns it, and creates ExtensionPrefs if it hasn't been created yet.
  ExtensionService* CreateExtensionService(
      const base::CommandLine* command_line,
      const base::FilePath& install_directory,
      bool autoupdate_enabled,
      bool enable_extensions = true);

  void CreateSocketManager();

  void InitForRegularProfile(bool extensions_enabled) override {}
  void SetExtensionService(ExtensionService* service);
  ExtensionService* extension_service() override;
  RuntimeData* runtime_data() override;
  ManagementPolicy* management_policy() override;
  ServiceWorkerManager* service_worker_manager() override;
  SharedUserScriptMaster* shared_user_script_master() override;
  StateStore* state_store() override;
  StateStore* rules_store() override;
  scoped_refptr<ValueStoreFactory> store_factory() override;
  TestingValueStore* value_store();
  InfoMap* info_map() override;
  QuotaService* quota_service() override;
  AppSorting* app_sorting() override;
  const base::OneShotEvent& ready() const override;
  ContentVerifier* content_verifier() override;
  std::unique_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) override;
  void InstallUpdate(const std::string& extension_id,
                     const std::string& public_key,
                     const base::FilePath& temp_dir,
                     bool install_immediately,
                     InstallUpdateCallback install_update_callback) override;
  bool FinishDelayedInstallationIfReady(const std::string& extension_id,
                                        bool install_immediately) override;

  // Note that you probably want to use base::RunLoop().RunUntilIdle() right
  // after this to run all the accumulated tasks.
  void SetReady() { ready_.Signal(); }

  // Factory method for tests to use with SetTestingProfile.
  static std::unique_ptr<KeyedService> Build(content::BrowserContext* profile);

  // Used by ExtensionPrefsTest to re-create the AppSorting after it has
  // re-created the ExtensionPrefs instance (this can never happen in non-test
  // code).
  void RecreateAppSorting();

 protected:
  Profile* profile_;

 private:
  std::unique_ptr<StateStore> state_store_;
  scoped_refptr<TestValueStoreFactory> store_factory_;
  std::unique_ptr<ManagementPolicy> management_policy_;
  std::unique_ptr<RuntimeData> runtime_data_;
  std::unique_ptr<ExtensionService> extension_service_;
  scoped_refptr<InfoMap> info_map_;
  std::unique_ptr<QuotaService> quota_service_;
  std::unique_ptr<AppSorting> app_sorting_;
  base::OneShotEvent ready_;

  std::unique_ptr<data_decoder::test::InProcessDataDecoder>
      in_process_data_decoder_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_SYSTEM_H_
