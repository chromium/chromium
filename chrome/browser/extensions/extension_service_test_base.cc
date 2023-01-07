// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service_test_base.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_garbage_collector_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/crx_verifier.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extensions_client.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/extensions/install_limiter.h"
#endif

namespace extensions {

namespace {

// Create a testing profile according to |params|.
std::unique_ptr<TestingProfile> BuildTestingProfile(
    const ExtensionServiceTestBase::ExtensionServiceInitParams& params) {
  TestingProfile::Builder profile_builder;
  // Create a PrefService that only contains user defined preference values and
  // policies.
  sync_preferences::PrefServiceMockFactory factory;
  // If pref_file is empty, TestingProfile automatically creates
  // sync_preferences::TestingPrefServiceSyncable instance.
  if (!params.pref_file.empty()) {
    factory.SetUserPrefsFile(
        params.pref_file,
        base::SingleThreadTaskRunner::GetCurrentDefault().get());
    if (params.policy_service) {
      factory.SetManagedPolicies(params.policy_service,
                                 g_browser_process->browser_policy_connector());
    }
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs(
        factory.CreateSyncable(registry.get()));
    RegisterUserProfilePrefs(registry.get());
    profile_builder.SetPrefService(std::move(prefs));
  }

  if (params.profile_is_supervised) {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    profile_builder.SetIsSupervisedProfile();
#endif
  }

  if (params.profile_is_guest) {
    profile_builder.SetGuestSession();
  }

  if (params.enable_bookmark_model) {
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        ManagedBookmarkServiceFactory::GetInstance(),
        ManagedBookmarkServiceFactory::GetDefaultFactory());
  }

  profile_builder.AddTestingFactory(
      ChromeSigninClientFactory::GetInstance(),
      base::BindRepeating(&signin::BuildTestSigninClient));
  profile_builder.AddTestingFactories(
      IdentityTestEnvironmentProfileAdaptor::
          GetIdentityTestEnvironmentFactories());
  // TODO(crbug.com/1222596): SyncService instantiation can be scoped down to
  // a few derived fixtures.
  profile_builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                                    SyncServiceFactory::GetDefaultFactory());

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
    : ExtensionServiceTestBase(
          std::make_unique<content::BrowserTaskEnvironment>(
              base::test::TaskEnvironment::MainThreadType::IO)) {}

ExtensionServiceTestBase::ExtensionServiceTestBase(
    std::unique_ptr<content::BrowserTaskEnvironment> task_environment)
    : task_environment_(std::move(task_environment)),
      service_(nullptr),
      testing_local_state_(TestingBrowserProcess::GetGlobal()),
      registry_(nullptr),
      verifier_format_override_(crx_file::VerifierFormat::CRX3) {
  base::FilePath test_data_dir;
  if (!base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir)) {
    ADD_FAILURE();
    return;
  }
  data_dir_ = test_data_dir.AppendASCII("extensions");

  policy_service_ = std::make_unique<policy::PolicyServiceImpl>(
      std::vector<policy::ConfigurationPolicyProvider*>{&policy_provider_});
}

ExtensionServiceTestBase::~ExtensionServiceTestBase() {
  // Why? Because |profile_| has to be destroyed before |at_exit_manager_|, but
  // is declared above it in the class definition since it's protected.
  // TODO(1269752): Since we're getting rid of at_exit_manager_, perhaps
  // we don't need this call?
  profile_.reset();
}

ExtensionServiceTestBase::ExtensionServiceInitParams
ExtensionServiceTestBase::CreateDefaultInitParams() {
  ExtensionServiceInitParams params;
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath path = temp_dir_.GetPath();
  path = path.Append(FILE_PATH_LITERAL("TestingExtensionsPath"));
  EXPECT_TRUE(base::DeletePathRecursively(path));
  base::File::Error error = base::File::FILE_OK;
  EXPECT_TRUE(base::CreateDirectoryAndGetError(path, &error)) << error;
  base::FilePath prefs_filename =
      path.Append(FILE_PATH_LITERAL("TestPreferences"));
  base::FilePath extensions_install_dir =
      path.Append(FILE_PATH_LITERAL("Extensions"));
  EXPECT_TRUE(base::DeletePathRecursively(extensions_install_dir));
  EXPECT_TRUE(base::CreateDirectoryAndGetError(extensions_install_dir, &error))
      << error;

  params.profile_path = path;
  params.pref_file = prefs_filename;
  params.extensions_install_dir = extensions_install_dir;

  params.policy_service = policy_service_.get();
  return params;
}

void ExtensionServiceTestBase::InitializeExtensionService(
    const ExtensionServiceTestBase::ExtensionServiceInitParams& params) {
  profile_ = BuildTestingProfile(params);
  CreateExtensionService(params);

  extensions_install_dir_ = params.extensions_install_dir;
  registry_ = ExtensionRegistry::Get(profile());

  // Garbage collector is typically NULL during tests, so give it a build.
  ExtensionGarbageCollectorFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(&ExtensionGarbageCollectorFactory::BuildInstanceFor));
}

void ExtensionServiceTestBase::InitializeEmptyExtensionService() {
  InitializeExtensionService(CreateDefaultInitParams());
}

void ExtensionServiceTestBase::InitializeInstalledExtensionService(
    const base::FilePath& prefs_file,
    const base::FilePath& source_install_dir,
    const ExtensionServiceInitParams& additional_params) {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath path = temp_dir_.GetPath();

  path = path.Append(FILE_PATH_LITERAL("TestingExtensionsPath"));
  ASSERT_TRUE(base::DeletePathRecursively(path));

  base::File::Error error = base::File::FILE_OK;
  ASSERT_TRUE(base::CreateDirectoryAndGetError(path, &error)) << error;

  base::FilePath temp_prefs = path.Append(chrome::kPreferencesFilename);
  ASSERT_TRUE(base::CopyFile(prefs_file, temp_prefs));

  base::FilePath extensions_install_dir =
      path.Append(FILE_PATH_LITERAL("Extensions"));
  ASSERT_TRUE(base::DeletePathRecursively(extensions_install_dir));
  ASSERT_TRUE(
      base::CopyDirectory(source_install_dir, extensions_install_dir, true));

  ExtensionServiceInitParams params = additional_params;
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
  const base::Value::Dict& dict =
      profile()->GetPrefs()->GetDict(pref_names::kExtensions);
  return dict.size();
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
  const base::Value::Dict& dict = prefs->GetDict(pref_names::kExtensions);

  const base::Value::Dict* pref = dict.FindDict(extension_id);
  if (!pref) {
    return testing::AssertionFailure()
        << "extension pref does not exist " << msg;
  }

  absl::optional<bool> val = pref->FindBoolByDottedPath(pref_path);
  if (!val.has_value()) {
    return testing::AssertionFailure()
        << pref_path << " pref not found " << msg;
  }

  return expected_val == val.value() ? testing::AssertionSuccess()
                                     : testing::AssertionFailure()
                                           << "base::Value is incorrect "
                                           << msg;
}

void ExtensionServiceTestBase::ValidateIntegerPref(
    const std::string& extension_id,
    const std::string& pref_path,
    int expected_val) {
  std::string msg = base::StringPrintf(
      "while checking: %s %s == %s", extension_id.c_str(), pref_path.c_str(),
      base::NumberToString(expected_val).c_str());

  PrefService* prefs = profile()->GetPrefs();
  const base::Value::Dict& dict = prefs->GetDict(pref_names::kExtensions);
  const base::Value::Dict* pref = dict.FindDict(extension_id);
  ASSERT_TRUE(pref) << msg;
  EXPECT_EQ(expected_val, pref->FindIntByDottedPath(pref_path)) << msg;
}

void ExtensionServiceTestBase::ValidateStringPref(
    const std::string& extension_id,
    const std::string& pref_path,
    const std::string& expected_val) {
  std::string msg = base::StringPrintf("while checking: %s.manifest.%s == %s",
                                       extension_id.c_str(), pref_path.c_str(),
                                       expected_val.c_str());

  const base::Value::Dict& dict =
      profile()->GetPrefs()->GetDict(pref_names::kExtensions);
  std::string manifest_path = extension_id + ".manifest";
  const base::Value::Dict* pref = dict.FindDictByDottedPath(manifest_path);
  ASSERT_TRUE(pref) << msg;
  const std::string* val = pref->FindStringByDottedPath(pref_path);
  ASSERT_TRUE(val) << msg;
  EXPECT_EQ(expected_val, *val) << msg;
}

void ExtensionServiceTestBase::SetUp() {
  LoadErrorReporter::GetInstance()->ClearErrors();

  // Force TabManager/TabLifecycleUnitSource creation.
  g_browser_process->resource_coordinator_parts();

  // Update the webstore update url. Some tests leave it set to a non-default
  // webstore_update_url_. This can make extension_urls::IsWebstoreUpdateUrl
  // return a false negative.
  ExtensionsClient::Get()->InitializeWebStoreUrls(
      base::CommandLine::ForCurrentProcess());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  kiosk_app_manager_ = std::make_unique<ash::KioskAppManager>();
#endif
}

void ExtensionServiceTestBase::TearDown() {
  if (profile_) {
    content::StoragePartitionConfig default_storage_partition_config =
        content::StoragePartitionConfig::CreateDefault(profile());
    auto* partition = profile_->GetStoragePartition(
        default_storage_partition_config, /*can_create=*/false);
    if (partition)
      partition->WaitForDeletionTasksForTesting();
  }
  policy_provider_.Shutdown();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  kiosk_app_manager_.reset();
#endif
}

void ExtensionServiceTestBase::SetUpTestCase() {
  // Safe to call multiple times.
  LoadErrorReporter::Init(false);  // no noisy errors.
}

// These are declared in the .cc so that all inheritors don't need to know
// that TestingProfile derives Profile derives BrowserContext.
content::BrowserContext* ExtensionServiceTestBase::browser_context() {
  return profile();
}

Profile* ExtensionServiceTestBase::profile() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (profile_->IsGuestSession())
    return profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return profile_.get();
}

sync_preferences::TestingPrefServiceSyncable*
ExtensionServiceTestBase::testing_pref_service() {
  return profile_->GetTestingPrefService();
}

void ExtensionServiceTestBase::CreateExtensionService(
    const ExtensionServiceInitParams& params) {
  TestExtensionSystem* system =
      static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
  if (!params.is_first_run)
    ExtensionPrefs::Get(profile())->SetAlertSystemFirstRun();

  service_ = system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), params.extensions_install_dir,
      params.autoupdate_enabled, params.extensions_enabled);

  service_->component_loader()->set_ignore_allowlist_for_testing(true);

  // When we start up, we want to make sure there is no external provider,
  // since the ExtensionService on Windows will use the Registry as a default
  // provider and if there is something already registered there then it will
  // interfere with the tests. Those tests that need an external provider
  // will register one specifically.
  service_->ClearProvidersForTesting();

  service_->RegisterInstallGate(ExtensionPrefs::DELAY_REASON_WAIT_FOR_IMPORTS,
                                service_->shared_module_service());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!params.enable_install_limiter)
    InstallLimiter::Get(profile())->DisableForTest();
#endif
}

}  // namespace extensions
