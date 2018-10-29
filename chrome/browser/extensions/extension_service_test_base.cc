// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service_test_base.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_garbage_collector_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/install_limiter.h"
#endif

namespace extensions {

namespace {

// By default, we run on the IO loop.
const int kThreadOptions = content::TestBrowserThreadBundle::IO_MAINLOOP;

// Create a testing profile according to |params|.
std::unique_ptr<TestingProfile> BuildTestingProfile(
    const ExtensionServiceTestBase::ExtensionServiceInitParams& params) {
  TestingProfile::Builder profile_builder;
  // Create a PrefService that only contains user defined preference values.
  sync_preferences::PrefServiceMockFactory factory;
  // If pref_file is empty, TestingProfile automatically creates
  // sync_preferences::TestingPrefServiceSyncable instance.
  if (!params.pref_file.empty()) {
    factory.SetUserPrefsFile(params.pref_file,
                             base::ThreadTaskRunnerHandle::Get().get());
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs(
        factory.CreateSyncable(registry.get()));
    RegisterUserProfilePrefs(registry.get());
    profile_builder.SetPrefService(std::move(prefs));
  }

  if (params.profile_is_supervised)
    profile_builder.SetSupervisedUserId("asdf");

  profile_builder.SetPath(params.profile_path);
  return profile_builder.Build();
}

}  // namespace

ExtensionServiceTestBase::ExtensionServiceInitParams::
    ExtensionServiceInitParams() {}

ExtensionServiceTestBase::ExtensionServiceInitParams::
    ExtensionServiceInitParams(const ExtensionServiceInitParams& other) =
        default;

ExtensionServiceTestBase::ExtensionServiceTestBase()
    : thread_bundle_(kThreadOptions),
      service_(nullptr),
      testing_local_state_(TestingBrowserProcess::GetGlobal()),
      registry_(nullptr) {
  base::FilePath test_data_dir;
  if (!base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir)) {
    ADD_FAILURE();
    return;
  }
  data_dir_ = test_data_dir.AppendASCII("extensions");
}

ExtensionServiceTestBase::~ExtensionServiceTestBase() {
  // Why? Because |profile_| has to be destroyed before |at_exit_manager_|, but
  // is declared above it in the class definition since it's protected.
  profile_.reset();
}

ExtensionServiceTestBase::ExtensionServiceInitParams
ExtensionServiceTestBase::CreateDefaultInitParams() {
  ExtensionServiceInitParams params;
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath path = temp_dir_.GetPath();
  path = path.Append(FILE_PATH_LITERAL("TestingExtensionsPath"));
  EXPECT_TRUE(base::DeleteFile(path, true));
  base::File::Error error = base::File::FILE_OK;
  EXPECT_TRUE(base::CreateDirectoryAndGetError(path, &error)) << error;
  base::FilePath prefs_filename =
      path.Append(FILE_PATH_LITERAL("TestPreferences"));
  base::FilePath extensions_install_dir =
      path.Append(FILE_PATH_LITERAL("Extensions"));
  EXPECT_TRUE(base::DeleteFile(extensions_install_dir, true));
  EXPECT_TRUE(base::CreateDirectoryAndGetError(extensions_install_dir, &error))
      << error;

  params.profile_path = path;
  params.pref_file = prefs_filename;
  params.extensions_install_dir = extensions_install_dir;
  return params;
}

void ExtensionServiceTestBase::InitializeExtensionService(
    const ExtensionServiceTestBase::ExtensionServiceInitParams& params) {
  profile_ = BuildTestingProfile(params);
  CreateExtensionService(params);

  extensions_install_dir_ = params.extensions_install_dir;
  registry_ = ExtensionRegistry::Get(profile_.get());

  // Garbage collector is typically NULL during tests, so give it a build.
  ExtensionGarbageCollectorFactory::GetInstance()->SetTestingFactoryAndUse(
      profile_.get(),
      base::BindRepeating(&ExtensionGarbageCollectorFactory::BuildInstanceFor));
}

void ExtensionServiceTestBase::InitializeEmptyExtensionService() {
  InitializeExtensionService(CreateDefaultInitParams());
}

void ExtensionServiceTestBase::InitializeInstalledExtensionService(
    const base::FilePath& prefs_file,
    const base::FilePath& source_install_dir) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath path = temp_dir_.GetPath();

  path = path.Append(FILE_PATH_LITERAL("TestingExtensionsPath"));
  ASSERT_TRUE(base::DeleteFile(path, true));

  base::File::Error error = base::File::FILE_OK;
  ASSERT_TRUE(base::CreateDirectoryAndGetError(path, &error)) << error;

  base::FilePath temp_prefs = path.Append(chrome::kPreferencesFilename);
  ASSERT_TRUE(base::CopyFile(prefs_file, temp_prefs));

  base::FilePath extensions_install_dir =
      path.Append(FILE_PATH_LITERAL("Extensions"));
  ASSERT_TRUE(base::DeleteFile(extensions_install_dir, true));
  ASSERT_TRUE(
      base::CopyDirectory(source_install_dir, extensions_install_dir, true));

  ExtensionServiceInitParams params;
  params.profile_path = path;
  params.pref_file = temp_prefs;
  params.extensions_install_dir = extensions_install_dir;
  InitializeExtensionService(params);
}

void ExtensionServiceTestBase::InitializeGoodInstalledExtensionService() {
  base::FilePath source_install_dir =
      data_dir_.AppendASCII("good").AppendASCII("Extensions");
  base::FilePath pref_path =
      source_install_dir.DirName().Append(chrome::kPreferencesFilename);
  InitializeInstalledExtensionService(pref_path, source_install_dir);
}

void ExtensionServiceTestBase::InitializeExtensionServiceWithUpdater() {
  ExtensionServiceInitParams params = CreateDefaultInitParams();
  params.autoupdate_enabled = true;
  InitializeExtensionService(params);
  service_->updater()->Start();
}

void ExtensionServiceTestBase::
    InitializeExtensionServiceWithExtensionsDisabled() {
  ExtensionServiceInitParams params = CreateDefaultInitParams();
  params.extensions_enabled = false;
  InitializeExtensionService(params);
}

size_t ExtensionServiceTestBase::GetPrefKeyCount() {
  const base::DictionaryValue* dict =
      profile()->GetPrefs()->GetDictionary(pref_names::kExtensions);
  if (!dict) {
    ADD_FAILURE();
    return 0;
  }
  return dict->size();
}

void ExtensionServiceTestBase::ValidatePrefKeyCount(size_t count) {
  EXPECT_EQ(count, GetPrefKeyCount());
}

testing::AssertionResult ExtensionServiceTestBase::ValidateBooleanPref(
    const std::string& extension_id,
    const std::string& pref_path,
    bool expected_val) {
  std::string msg = base::StringPrintf("while checking: %s %s == %s",
                                       extension_id.c_str(), pref_path.c_str(),
                                       expected_val ? "true" : "false");

  PrefService* prefs = profile()->GetPrefs();
  const base::DictionaryValue* dict =
      prefs->GetDictionary(pref_names::kExtensions);
  if (!dict) {
    return testing::AssertionFailure()
        << "extension.settings does not exist " << msg;
  }

  const base::DictionaryValue* pref = NULL;
  if (!dict->GetDictionary(extension_id, &pref)) {
    return testing::AssertionFailure()
        << "extension pref does not exist " << msg;
  }

  bool val = false;
  if (!pref->GetBoolean(pref_path, &val)) {
    return testing::AssertionFailure()
        << pref_path << " pref not found " << msg;
  }

  return expected_val == val
      ? testing::AssertionSuccess()
      : testing::AssertionFailure() << "base::Value is incorrect " << msg;
}

void ExtensionServiceTestBase::ValidateIntegerPref(
    const std::string& extension_id,
    const std::string& pref_path,
    int expected_val) {
  std::string msg = base::StringPrintf("while checking: %s %s == %s",
                                       extension_id.c_str(), pref_path.c_str(),
                                       base::IntToString(expected_val).c_str());

  PrefService* prefs = profile()->GetPrefs();
  const base::DictionaryValue* dict =
      prefs->GetDictionary(pref_names::kExtensions);
  ASSERT_TRUE(dict != NULL) << msg;
  const base::DictionaryValue* pref = NULL;
  ASSERT_TRUE(dict->GetDictionary(extension_id, &pref)) << msg;
  EXPECT_TRUE(pref != NULL) << msg;
  int val;
  ASSERT_TRUE(pref->GetInteger(pref_path, &val)) << msg;
  EXPECT_EQ(expected_val, val) << msg;
}

void ExtensionServiceTestBase::ValidateStringPref(
    const std::string& extension_id,
    const std::string& pref_path,
    const std::string& expected_val) {
  std::string msg = base::StringPrintf("while checking: %s.manifest.%s == %s",
                                       extension_id.c_str(), pref_path.c_str(),
                                       expected_val.c_str());

  const base::DictionaryValue* dict =
      profile()->GetPrefs()->GetDictionary(pref_names::kExtensions);
  ASSERT_TRUE(dict != NULL) << msg;
  const base::DictionaryValue* pref = NULL;
  std::string manifest_path = extension_id + ".manifest";
  ASSERT_TRUE(dict->GetDictionary(manifest_path, &pref)) << msg;
  EXPECT_TRUE(pref != NULL) << msg;
  std::string val;
  ASSERT_TRUE(pref->GetString(pref_path, &val)) << msg;
  EXPECT_EQ(expected_val, val) << msg;
}

void ExtensionServiceTestBase::SetUp() {
  LoadErrorReporter::GetInstance()->ClearErrors();
}

void ExtensionServiceTestBase::TearDown() {
  if (profile_) {
    auto* partition =
        content::BrowserContext::GetDefaultStoragePartition(profile_.get());
    if (partition)
      partition->WaitForDeletionTasksForTesting();
  }
}

void ExtensionServiceTestBase::SetUpTestCase() {
  // Safe to call multiple times.
  LoadErrorReporter::Init(false);  // no noisy errors.
}

// These are declared in the .cc so that all inheritors don't need to know
// that TestingProfile derives Profile derives BrowserContext.
content::BrowserContext* ExtensionServiceTestBase::browser_context() {
  return profile_.get();
}

Profile* ExtensionServiceTestBase::profile() {
  return profile_.get();
}

sync_preferences::TestingPrefServiceSyncable*
ExtensionServiceTestBase::testing_pref_service() {
  return profile_->GetTestingPrefService();
}

void ExtensionServiceTestBase::CreateExtensionService(
    const ExtensionServiceInitParams& params) {
  TestExtensionSystem* system =
      static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_.get()));
  if (!params.is_first_run)
    ExtensionPrefs::Get(profile_.get())->SetAlertSystemFirstRun();

  service_ = system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), params.extensions_install_dir,
      params.autoupdate_enabled, params.extensions_enabled);

  service_->component_loader()->set_ignore_whitelist_for_testing(true);

  // When we start up, we want to make sure there is no external provider,
  // since the ExtensionService on Windows will use the Registry as a default
  // provider and if there is something already registered there then it will
  // interfere with the tests. Those tests that need an external provider
  // will register one specifically.
  service_->ClearProvidersForTesting();

  service_->RegisterInstallGate(ExtensionPrefs::DELAY_REASON_WAIT_FOR_IMPORTS,
                                service_->shared_module_service());

#if defined(OS_CHROMEOS)
  InstallLimiter::Get(profile_.get())->DisableForTest();
#endif
}

}  // namespace extensions
