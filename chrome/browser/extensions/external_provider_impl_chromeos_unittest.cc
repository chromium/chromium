// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_provider_impl.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kExternalAppId[] = "kekdneafjmhmndejhmbcadfiiofngffo";
const char kStandaloneAppId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char kStandaloneChildAppId[] = "hcglmfcclpfgljeaiahehebeoaiicbko";

const char kTestUserAccount[] = "user@test";

class ExternalProviderImplChromeOSTest : public ExtensionServiceTestBase {
 public:
  ExternalProviderImplChromeOSTest()
      : fake_user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(fake_user_manager_.get())) {}

  ExternalProviderImplChromeOSTest(const ExternalProviderImplChromeOSTest&) =
      delete;
  ExternalProviderImplChromeOSTest& operator=(
      const ExternalProviderImplChromeOSTest&) = delete;

  ~ExternalProviderImplChromeOSTest() override {}

  void InitServiceWithExternalProviders(bool standalone) {
    InitServiceWithExternalProvidersAndUserType(standalone,
                                                false /* is_child */);
  }

  void InitServiceWithExternalProvidersAndUserType(bool standalone,
                                                   bool is_child) {
    InitializeEmptyExtensionService();

    if (is_child) {
      profile_->SetIsSupervisedProfile();
    }

    service_->Init();

    if (standalone) {
      external_externsions_overrides_ =
          std::make_unique<base::ScopedPathOverride>(
              chrome::DIR_STANDALONE_EXTERNAL_EXTENSIONS,
              data_dir().Append("external_standalone"));
    } else {
      external_externsions_overrides_ =
          std::make_unique<base::ScopedPathOverride>(
              chrome::DIR_EXTERNAL_EXTENSIONS, data_dir().Append("external"));
    }

    // This switch is set when creating a TestingProfile, but needs to be
    // removed for some ExternalProviders to be created.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kDisableDefaultApps);

    ProviderCollection providers;
    ExternalProviderImpl::CreateExternalProviders(service_, profile_.get(),
                                                  &providers);

    for (std::unique_ptr<ExternalProviderInterface>& provider : providers) {
      service_->AddProviderForTesting(std::move(provider));
    }
  }

  // ExtensionServiceTestBase overrides:
  void SetUp() override { ExtensionServiceTestBase::SetUp(); }

  void TearDown() override {
    // If some extensions are being installed (on a background thread) and we
    // stop before the intsallation is complete, some installation related
    // objects might be leaked (as the background thread won't block on exit and
    // finish cleanly).
    // So ensure we let pending extension installations finish.
    WaitForPendingStandaloneExtensionsInstalled();
    ExtensionServiceTestBase::TearDown();
  }

  // Waits until all possible standalone extensions are installed.
  void WaitForPendingStandaloneExtensionsInstalled() {
    service_->CheckForExternalUpdates();
    base::RunLoop().RunUntilIdle();
    PendingExtensionManager* const pending_extension_manager =
        service_->pending_extension_manager();
    while (pending_extension_manager->IsIdPending(kStandaloneAppId) ||
           pending_extension_manager->IsIdPending(kStandaloneChildAppId)) {
      base::RunLoop().RunUntilIdle();
    }
  }

  void ValidateExternalProviderCountInAppMode(size_t expected_count) {
    base::CommandLine* command = base::CommandLine::ForCurrentProcess();
    command->AppendSwitchASCII(switches::kForceAppMode, std::string());
    command->AppendSwitchASCII(switches::kAppId, std::string("app_id"));

    InitializeEmptyExtensionService();

    ProviderCollection providers;
    ExternalProviderImpl::CreateExternalProviders(service_, profile_.get(),
                                                  &providers);

    EXPECT_EQ(providers.size(), expected_count);
  }

  ash::FakeChromeUserManager* fake_user_manager() const {
    return fake_user_manager_;
  }

 private:
  std::unique_ptr<base::ScopedPathOverride> external_externsions_overrides_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> fake_user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
};

}  // namespace

// Normal mode, external app should be installed.
TEST_F(ExternalProviderImplChromeOSTest, Normal) {
  InitServiceWithExternalProviders(false);

  TestExtensionRegistryObserver observer(registry(), kExternalAppId);

  service_->CheckForExternalUpdates();

  scoped_refptr<const Extension> loaded_extension =
      observer.WaitForExtensionLoaded();

  EXPECT_EQ(loaded_extension->id(), kExternalAppId);
}

// App mode, no external app should be installed.
TEST_F(ExternalProviderImplChromeOSTest, AppMode) {
  base::CommandLine* command = base::CommandLine::ForCurrentProcess();
  command->AppendSwitchASCII(switches::kForceAppMode, std::string());
  command->AppendSwitchASCII(switches::kAppId, std::string("app_id"));

  InitServiceWithExternalProviders(false);

  service_->CheckForExternalUpdates();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(registry()->GetInstalledExtension(kExternalAppId));
}

// Normal mode, standalone app should be installed, because sync is enabled but
// not running.
// flaky: crbug.com/854206
TEST_F(ExternalProviderImplChromeOSTest, DISABLED_Standalone) {
  InitServiceWithExternalProviders(true);

  WaitForPendingStandaloneExtensionsInstalled();

  EXPECT_TRUE(registry()->GetInstalledExtension(kStandaloneAppId));
  // Also include apps available for child.
  EXPECT_TRUE(registry()->GetInstalledExtension(kStandaloneChildAppId));
}

// Should include only subset of default apps
// flaky: crbug.com/854206
TEST_F(ExternalProviderImplChromeOSTest, DISABLED_StandaloneChild) {
  InitServiceWithExternalProvidersAndUserType(true /* standalone */,
                                              true /* is_child */);

  WaitForPendingStandaloneExtensionsInstalled();

  // kStandaloneAppId is not available for child.
  EXPECT_FALSE(registry()->GetInstalledExtension(kStandaloneAppId));
  EXPECT_TRUE(registry()->GetInstalledExtension(kStandaloneChildAppId));
}

// Normal mode, standalone app should be installed, because sync is disabled.
TEST_F(ExternalProviderImplChromeOSTest, SyncDisabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(syncer::kDisableSync);

  InitServiceWithExternalProviders(true);

  TestExtensionRegistryObserver observer(registry(), kStandaloneAppId);

  service_->CheckForExternalUpdates();

  scoped_refptr<const Extension> loaded_extension =
      observer.WaitForExtensionLoaded();
  EXPECT_EQ(loaded_extension->id(), kStandaloneAppId);
  EXPECT_TRUE(registry()->GetInstalledExtension(kStandaloneAppId));
}

// User signed in, sync service started, install app when sync is disabled by
// policy.
TEST_F(ExternalProviderImplChromeOSTest, PolicyDisabled) {
  InitServiceWithExternalProviders(true);

  // Log user in, start sync.
  TestingBrowserProcess::GetGlobal()->SetProfileManager(
      std::make_unique<ProfileManagerWithoutInit>(temp_dir().GetPath()));

  auto identity_test_env_profile_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
  identity_test_env_profile_adaptor->identity_test_env()
      ->MakePrimaryAccountAvailable("test_user@gmail.com",
                                    signin::ConsentLevel::kSync);

  // Sync is dsabled by policy.
  profile_->GetPrefs()->SetBoolean(syncer::prefs::internal::kSyncManaged, true);

  TestExtensionRegistryObserver observer(registry(), kStandaloneAppId);

  // App sync will wait for priority sync to complete.
  service_->CheckForExternalUpdates();

  scoped_refptr<const Extension> loaded_extension =
      observer.WaitForExtensionLoaded();
  EXPECT_EQ(loaded_extension->id(), kStandaloneAppId);
  EXPECT_TRUE(registry()->GetInstalledExtension(kStandaloneAppId));

  TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
}

// User signed in, sync service started, install app when priority sync is
// completed.
TEST_F(ExternalProviderImplChromeOSTest, PriorityCompleted) {
  InitServiceWithExternalProviders(true);

  // User is logged in.
  auto identity_test_env_profile_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
  identity_test_env_profile_adaptor->identity_test_env()->SetPrimaryAccount(
      "test_user@gmail.com", signin::ConsentLevel::kSync);

  // OOBE screen completed with OS sync enabled.
  PrefService* prefs = profile()->GetPrefs();
  prefs->SetBoolean(ash::prefs::kSyncOobeCompleted, true);

  TestExtensionRegistryObserver observer(registry(), kStandaloneAppId);

  // Priority sync completed.
  PrefServiceSyncableFromProfile(profile())
      ->GetSyncableService(syncer::OS_PRIORITY_PREFERENCES)
      ->MergeDataAndStartSyncing(
          syncer::OS_PRIORITY_PREFERENCES, syncer::SyncDataList(),
          std::make_unique<syncer::FakeSyncChangeProcessor>());

  // App sync will wait for priority sync to complete.
  service_->CheckForExternalUpdates();

  scoped_refptr<const Extension> loaded_extension =
      observer.WaitForExtensionLoaded();
  EXPECT_EQ(loaded_extension->id(), kStandaloneAppId);
  EXPECT_TRUE(registry()->GetInstalledExtension(kStandaloneAppId));
}

// Validate the external providers enabled in the Chrome App Kiosk session. The
// expected number should be 3.
// - |policy_provider|.
// - |kiosk_app_provider|.
// - |secondary_kiosk_app_provider|.
TEST_F(ExternalProviderImplChromeOSTest, ChromeAppKiosk) {
  const AccountId kiosk_account_id(AccountId::FromUserEmail(kTestUserAccount));
  fake_user_manager()->AddKioskAppUser(kiosk_account_id);
  fake_user_manager()->LoginUser(kiosk_account_id);

  ValidateExternalProviderCountInAppMode(3u);
}

// Validate the external providers enabled in the Web App Kiosk session. The
// expected number should be only 1.
// - |policy_provider|.
TEST_F(ExternalProviderImplChromeOSTest, WebAppKiosk) {
  const AccountId kiosk_account_id(AccountId::FromUserEmail(kTestUserAccount));
  fake_user_manager()->AddWebKioskAppUser(kiosk_account_id);
  fake_user_manager()->LoginUser(kiosk_account_id);

  ValidateExternalProviderCountInAppMode(1u);
}

}  // namespace extensions
