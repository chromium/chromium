// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service_test_base.h"
#include "base/memory/raw_ptr.h"

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
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
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
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/user_manager/user_manager_impl.h"
#endif

namespace extensions {

namespace {

// Create a testing profile according to |params|.
std::unique_ptr<TestingProfile> BuildTestingProfile(
    ExtensionServiceTestBase::ExtensionServiceInitParams params,
    base::ScopedTempDir& temp_dir,
    policy::PolicyService* policy_service) {
  TestingProfile::Builder profile_builder;

  if (!temp_dir.CreateUniqueTempDir()) {
    return nullptr;
  }

#if BUILDFLAG(IS_MAC)
  // For tests, make sure we're working with the absolute profile path, so that
  // path comparisons don't fail. See https://issues.chromium.org/40916874 for
  // details.
  if (!temp_dir.Set(base::MakeAbsoluteFilePath(temp_dir.Take()))) {
    return nullptr;
  }
#endif

  base::FilePath profile_dir =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("TestingExtensionsPath"));
  if (base::File::Error error = base::File::FILE_OK;
      !base::CreateDirectoryAndGetError(profile_dir, &error)) {
    LOG(ERROR) << "Failed to create profile directory: " << error;
    return nullptr;
  }

  // If pref_file is empty, TestingProfile automatically creates
  // sync_preferences::TestingPrefServiceSyncable instance.
  if (params.prefs_content.has_value()) {
    base::FilePath prefs_path =
        profile_dir.Append(chrome::kPreferencesFilename);
    if (!base::WriteFile(prefs_path, params.prefs_content.value())) {
      LOG(ERROR) << "Failed to write a prefs file";
      return nullptr;
    }

    // Create a PrefService that only contains user defined preference values
    // and policies.
    sync_preferences::PrefServiceMockFactory factory;
    factory.SetUserPrefsFile(
        prefs_path, base::SingleThreadTaskRunner::GetCurrentDefault().get());
    factory.SetManagedPolicies(policy_service,
                               g_browser_process->browser_policy_connector());
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs(
        factory.CreateSyncable(registry.get()));
    RegisterUserProfilePrefs(registry.get());
    profile_builder.SetPrefService(std::move(prefs));
  }

  base::FilePath extensions_install_dir =
      profile_dir.AppendASCII(kInstallDirectoryName);
  if (!base::DeletePathRecursively(extensions_install_dir)) {
    LOG(ERROR) << "Failed to clean extensions directory";
    return nullptr;
  }
  if (params.extensions_dir.empty()) {
    if (base::File::Error error = base::File::FILE_OK;
        !base::CreateDirectoryAndGetError(extensions_install_dir, &error)) {
      LOG(ERROR) << "Failed to create extensions directory: " << error;
      return nullptr;
    }
  } else {
    if (!base::CopyDirectory(params.extensions_dir, extensions_install_dir,
                             true)) {
      LOG(ERROR) << "Failed to copy extensions directory";
      return nullptr;
    }
  }

  // Only perform cleanup and copying of unpacked extensions if the path exists
  // for the test since this is less common than for packed extensions.
  if (base::PathExists(params.unpacked_extensions_dir)) {
    base::FilePath unpacked_extensions_install_dir =
        profile_dir.AppendASCII(kUnpackedInstallDirectoryName);
    if (!base::DeletePathRecursively(unpacked_extensions_install_dir)) {
      LOG(ERROR) << "Failed to clean unpacked extensions directory";
      return nullptr;
    }
    if (params.unpacked_extensions_dir.empty()) {
      if (base::File::Error error = base::File::FILE_OK;
          !base::CreateDirectoryAndGetError(unpacked_extensions_install_dir,
                                            &error)) {
        LOG(ERROR) << "Failed to create unpacked extensions directory: "
                   << error;
        return nullptr;
      }
    } else {
      if (!base::CopyDirectory(params.unpacked_extensions_dir,
                               unpacked_extensions_install_dir, true)) {
        LOG(ERROR) << "Failed to copy unpacked extensions directory";
        return nullptr;
      }
    }
  }

  if (params.profile_is_supervised) {
    profile_builder.SetIsSupervisedProfile();
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
  // TODO(crbug.com/40774163): SyncService (and thus TrustedVaultService)
  // instantiation can be scoped down to a few derived fixtures.
  profile_builder.AddTestingFactory(
      TrustedVaultServiceFactory::GetInstance(),
      TrustedVaultServiceFactory::GetDefaultFactory());
  profile_builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                                    SyncServiceFactory::GetDefaultFactory());
  profile_builder.AddTestingFactory(
      ExtensionGarbageCollectorFactory::GetInstance(),
      base::BindRepeating(&ExtensionGarbageCollectorFactory::BuildInstanceFor));

  profile_builder.AddTestingFactories(std::move(params.testing_factories));

  profile_builder.SetPath(profile_dir);
  return profile_builder.Build();
}

}  // namespace

ExtensionServiceTestBase::ExtensionServiceInitParams::
    ExtensionServiceInitParams() = default;

ExtensionServiceTestBase::ExtensionServiceInitParams::
    ExtensionServiceInitParams(ExtensionServiceInitParams&& other) = default;

ExtensionServiceTestBase::ExtensionServiceInitParams::
    ~ExtensionServiceInitParams() = default;

bool ExtensionServiceTestBase::ExtensionServiceInitParams::
    SetPrefsContentFromFile(const base::FilePath& filepath) {
  std::string content;
  if (!base::ReadFileToString(filepath, &content)) {
    return false;
  }
  prefs_content.emplace(std::move(content));
  return true;
}

bool ExtensionServiceTestBase::ExtensionServiceInitParams::
    ConfigureByTestDataDirectory(const base::FilePath& filepath) {
  if (!SetPrefsContentFromFile(filepath.Append(chrome::kPreferencesFilename))) {
    return false;
  }
  extensions_dir = filepath.AppendASCII(kInstallDirectoryName);
  unpacked_extensions_dir = filepath.AppendASCII(kUnpackedInstallDirectoryName);
  return true;
}

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
      user_manager_(std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<ash::UserManagerDelegateImpl>(),
          testing_local_state_.Get(),
          ash::CrosSettings::Get())),
#endif
      verifier_format_override_(crx_file::VerifierFormat::CRX3) {
  base::FilePath test_data_dir;
  if (!base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir)) {
    ADD_FAILURE();
    return;
  }
  data_dir_ = test_data_dir.AppendASCII("extensions");

  policy_service_ = std::make_unique<policy::PolicyServiceImpl>(
      std::vector<
          raw_ptr<policy::ConfigurationPolicyProvider, VectorExperimental>>{
          &policy_provider_});
}

ExtensionServiceTestBase::~ExtensionServiceTestBase() {
  // Why? Because |profile_| has to be destroyed before |at_exit_manager_|, but
  // is declared above it in the class definition since it's protected.
  // TODO(crbug.com/40205142): Since we're getting rid of at_exit_manager_,
  // perhaps we don't need this call?
  profile_.reset();
}

void ExtensionServiceTestBase::InitializeExtensionService(
    ExtensionServiceTestBase::ExtensionServiceInitParams params) {
  const bool is_first_run = params.is_first_run;
  const bool autoupdate_enabled = params.autoupdate_enabled;
  const bool extensions_enabled = params.extensions_enabled;
  const bool enable_install_limiter = params.enable_install_limiter;

  profile_ =
      BuildTestingProfile(std::move(params), temp_dir_, policy_service_.get());
  extensions_install_dir_ =
      profile_->GetPath().AppendASCII(kInstallDirectoryName);
  unpacked_install_dir_ =
      profile_->GetPath().AppendASCII(kUnpackedInstallDirectoryName);

  CreateExtensionService(is_first_run, autoupdate_enabled, extensions_enabled,
                         enable_install_limiter);
  registry_ = ExtensionRegistry::Get(profile());
}

bool ExtensionServiceTestBase::ShouldAllowMV2Extensions() {
  return true;
}

void ExtensionServiceTestBase::InitializeEmptyExtensionService() {
  ExtensionServiceInitParams params;
  params.prefs_content = "";
  InitializeExtensionService(std::move(params));
}

void ExtensionServiceTestBase::InitializeGoodInstalledExtensionService() {
  ExtensionServiceInitParams params;
  ASSERT_TRUE(
      params.ConfigureByTestDataDirectory(data_dir().AppendASCII("good")));
  InitializeExtensionService(std::move(params));
}

void ExtensionServiceTestBase::InitializeExtensionServiceWithUpdater() {
  ExtensionServiceInitParams params;
  params.autoupdate_enabled = true;
  InitializeExtensionService(std::move(params));
  service_->updater()->Start();
}

void ExtensionServiceTestBase::
    InitializeExtensionServiceWithExtensionsDisabled() {
  ExtensionServiceInitParams params;
  params.extensions_enabled = false;
  InitializeExtensionService(std::move(params));
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
  std::string msg =
      base::StringPrintf("while checking: %s %s == %s", extension_id.c_str(),
                         pref_path.c_str(), expected_val ? "true" : "false");

  PrefService* prefs = profile()->GetPrefs();
  const base::Value::Dict& dict = prefs->GetDict(pref_names::kExtensions);

  const base::Value::Dict* pref = dict.FindDict(extension_id);
  if (!pref) {
    return testing::AssertionFailure()
           << "extension pref does not exist " << msg;
  }

  std::optional<bool> val = pref->FindBoolByDottedPath(pref_path);
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
  // TODO(b/308107135) own KioskController instead of KioskAppManager.
  // A test might have initialized a `KioskAppManager` already.
  if (!ash::KioskChromeAppManager::IsInitialized()) {
    kiosk_chrome_app_manager_ = std::make_unique<ash::KioskChromeAppManager>();
  }
#endif

  if (ShouldAllowMV2Extensions()) {
    mv2_enabler_.emplace();
  }
}

void ExtensionServiceTestBase::TearDown() {
  if (profile_) {
    content::StoragePartitionConfig default_storage_partition_config =
        content::StoragePartitionConfig::CreateDefault(profile());
    auto* partition = profile_->GetStoragePartition(
        default_storage_partition_config, /*can_create=*/false);
    if (partition) {
      partition->WaitForDeletionTasksForTesting();
    }
  }
  policy_provider_.Shutdown();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  kiosk_chrome_app_manager_.reset();
#endif
}

void ExtensionServiceTestBase::SetUpTestSuite() {
  // Safe to call multiple times.
  LoadErrorReporter::Init(false);  // no noisy errors.
}

// These are declared in the .cc so that all inheritors don't need to know
// that TestingProfile derives Profile derives BrowserContext.
content::BrowserContext* ExtensionServiceTestBase::browser_context() {
  return profile();
}

Profile* ExtensionServiceTestBase::profile() {
// TODO(crbug.com/40891982): Refactor this convenience upstream to test callers.
// Possibly just BuiltInAppTest.BuildGuestMode.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (profile_->IsGuestSession()) {
    return profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return profile_.get();
}

sync_preferences::TestingPrefServiceSyncable*
ExtensionServiceTestBase::testing_pref_service() {
  return profile_->GetTestingPrefService();
}

void ExtensionServiceTestBase::CreateExtensionService(
    bool is_first_run,
    bool autoupdate_enabled,
    bool extensions_enabled,
    bool enable_install_limiter) {
  TestExtensionSystem* system =
      static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
  if (!is_first_run) {
    ExtensionPrefs::Get(profile())->SetAlertSystemFirstRun();
  }

  service_ = system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), extensions_install_dir_,
      unpacked_install_dir_, autoupdate_enabled, extensions_enabled);

  service_->component_loader()->set_ignore_allowlist_for_testing(true);

  // When we start up, we want to make sure there is no external provider,
  // since the ExtensionService on Windows will use the Registry as a default
  // provider and if there is something already registered there then it will
  // interfere with the tests. Those tests that need an external provider
  // will register one specifically.
  service_->ClearProvidersForTesting();

  service_->RegisterInstallGate(ExtensionPrefs::DelayReason::kWaitForImports,
                                service_->shared_module_service());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!enable_install_limiter) {
    InstallLimiter::Get(profile())->DisableForTest();
  }
#endif
}

}  // namespace extensions
