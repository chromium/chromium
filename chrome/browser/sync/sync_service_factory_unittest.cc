// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_service_factory.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/testing_profile.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/sync/wifi_configuration_sync_service_factory.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/sync_wifi/wifi_configuration_sync_service.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#endif

class SyncServiceFactoryTest : public testing::Test {
 public:
  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    app_list::AppListSyncableServiceFactory::SetUseInTesting(true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    TestingProfile::Builder builder;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Only the main profile enables syncer::WEB_APPS.
    builder.SetIsMainProfile(true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    builder.AddTestingFactory(FaviconServiceFactory::GetInstance(),
                              FaviconServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
                              HistoryServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              SyncServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    builder.AddTestingFactory(TrustedVaultServiceFactory::GetInstance(),
                              TrustedVaultServiceFactory::GetDefaultFactory());
    // Some services will only be created if there is a WebDataService.
    builder.AddTestingFactory(WebDataServiceFactory::GetInstance(),
                              WebDataServiceFactory::GetDefaultFactory());

#if BUILDFLAG(ENABLE_SPELLCHECK)
    builder.AddTestingFactory(
        SpellcheckServiceFactory::GetInstance(),
        base::BindRepeating(base::BindLambdaForTesting(
            [](content::BrowserContext* browser_context) {
              return std::unique_ptr<KeyedService>(
                  std::make_unique<SpellcheckService>(browser_context));
            })));
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

    profile_ = builder.Build();
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    app_list::AppListSyncableServiceFactory::SetUseInTesting(false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    // There may tasks in flight referencing fields owned by the test fixture.
    // Make sure they are flushed now to prevent memory safety errors, e.g.
    // use-after-destruction errors.
    task_environment_.RunUntilIdle();
  }

 protected:
  SyncServiceFactoryTest() = default;
  ~SyncServiceFactoryTest() override = default;

  // Returns the collection of default datatypes.
  syncer::DataTypeSet DefaultDatatypes() {
    static_assert(53 == syncer::GetNumDataTypes(),
                  "When adding a new type, you probably want to add it here as "
                  "well (assuming it is already enabled). Check similar "
                  "function in "
                  "ios/c/b/sync/model/sync_service_factory_unittest.cc");

    syncer::DataTypeSet datatypes;

    // These preprocessor conditions and their order should be in sync with
    // preprocessor conditions in ChromeSyncClient::CreateDataTypeControllers:

    // ChromeSyncClient types.
    datatypes.Put(syncer::READING_LIST);
    datatypes.Put(syncer::SECURITY_EVENTS);
    datatypes.Put(syncer::SUPERVISED_USER_SETTINGS);

#if BUILDFLAG(ENABLE_EXTENSIONS)
    datatypes.Put(syncer::APPS);
    datatypes.Put(syncer::EXTENSIONS);
    datatypes.Put(syncer::EXTENSION_SETTINGS);
    datatypes.Put(syncer::APP_SETTINGS);
    datatypes.Put(syncer::WEB_APPS);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if !BUILDFLAG(IS_ANDROID)
    datatypes.Put(syncer::THEMES);
    datatypes.Put(syncer::SEARCH_ENGINES);
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
    datatypes.Put(syncer::SAVED_TAB_GROUP);
#elif BUILDFLAG(IS_ANDROID)
    if (base::FeatureList::IsEnabled(tab_groups::kTabGroupSyncAndroid)) {
      datatypes.Put(syncer::SAVED_TAB_GROUP);
    }
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    datatypes.Put(syncer::DICTIONARY);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
    datatypes.Put(syncer::APP_LIST);
    if (arc::IsArcAllowedForProfile(profile())) {
      datatypes.Put(syncer::ARC_PACKAGE);
    }
    if (ash::features::IsFloatingSsoAllowed()) {
      datatypes.Put(syncer::COOKIES);
    }
    datatypes.Put(syncer::OS_PREFERENCES);
    datatypes.Put(syncer::OS_PRIORITY_PREFERENCES);
    datatypes.Put(syncer::PRINTERS);
    if (ash::features::IsOAuthIppEnabled()) {
      datatypes.Put(syncer::PRINTERS_AUTHORIZATION_SERVERS);
    }
    datatypes.Put(syncer::WIFI_CONFIGURATIONS);
    datatypes.Put(syncer::WORKSPACE_DESK);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // Common types. This excludes PASSWORDS,
    // INCOMING_PASSWORD_SHARING_INVITATION and
    // INCOMING_PASSWORD_SHARING_INVITATION, because the password store factory
    // is null for testing and hence no controller gets instantiated for those
    // types.
    datatypes.Put(syncer::AUTOFILL);
    datatypes.Put(syncer::AUTOFILL_PROFILE);
    if (base::FeatureList::IsEnabled(
            syncer::kSyncAutofillWalletCredentialData)) {
      datatypes.Put(syncer::AUTOFILL_WALLET_CREDENTIAL);
    }
    datatypes.Put(syncer::AUTOFILL_WALLET_DATA);
    datatypes.Put(syncer::AUTOFILL_WALLET_METADATA);
    datatypes.Put(syncer::AUTOFILL_WALLET_OFFER);
    datatypes.Put(syncer::BOOKMARKS);
    if (base::FeatureList::IsEnabled(commerce::kProductSpecifications)) {
      datatypes.Put(syncer::PRODUCT_COMPARISON);
    }
    datatypes.Put(syncer::CONTACT_INFO);
    datatypes.Put(syncer::DEVICE_INFO);
    datatypes.Put(syncer::HISTORY);
    datatypes.Put(syncer::HISTORY_DELETE_DIRECTIVES);
    datatypes.Put(syncer::PREFERENCES);
    datatypes.Put(syncer::PRIORITY_PREFERENCES);
    datatypes.Put(syncer::SESSIONS);
    datatypes.Put(syncer::USER_EVENTS);
    datatypes.Put(syncer::USER_CONSENTS);
    datatypes.Put(syncer::SEND_TAB_TO_SELF);
    datatypes.Put(syncer::SHARING_MESSAGE);
#if !BUILDFLAG(IS_ANDROID)
    if (base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)) {
      datatypes.Put(syncer::WEBAUTHN_CREDENTIAL);
    }
#endif  // !BUILDFLAG(IS_ANDROID)
    if (base::FeatureList::IsEnabled(
            data_sharing::features::kDataSharingFeature)) {
      datatypes.Put(syncer::COLLABORATION_GROUP);
      datatypes.Put(syncer::SHARED_TAB_GROUP_DATA);
    }
#if BUILDFLAG(IS_ANDROID)
    if (base::FeatureList::IsEnabled(syncer::kWebApkBackupAndRestoreBackend)) {
      datatypes.Put(syncer::WEB_APKS);
    }
#endif  // BUILDFLAG(IS_ANDROID)

    // syncer::PLUS_ADDRESS is excluded because GoogleGroupsManagerFactory is
    // null for testing and hence no controller gets instantiated for the type.

    if (base::FeatureList::IsEnabled(syncer::kSyncPlusAddressSetting)) {
      datatypes.Put(syncer::PLUS_ADDRESS_SETTING);
    }

    return datatypes;
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Fake network stack is required for WIFI_CONFIGURATIONS datatype. It's also
  // used by `network_config_helper_`
  ash::NetworkHandlerTestHelper network_handler_test_helper_;

  // Sets up  and  tears down the Chrome OS networking mojo service as needed
  // for the WIFI_CONFIGURATIONS sync service.
  ash::network_config::CrosNetworkConfigTestHelper network_config_helper_;
#endif
};

// Verify that the disable sync flag disables creation of the sync service.
TEST_F(SyncServiceFactoryTest, DisableSyncFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(syncer::kDisableSync);
  EXPECT_EQ(nullptr, SyncServiceFactory::GetForProfile(profile()));
}

// Verify that a normal (no command line flags) SyncServiceImpl can be created
// and properly initialized.
TEST_F(SyncServiceFactoryTest, CreateSyncServiceImplDefault) {
  syncer::SyncServiceImpl* sync_service =
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(profile());
  syncer::DataTypeSet types = sync_service->GetRegisteredDataTypesForTest();
  const syncer::DataTypeSet default_types = DefaultDatatypes();
  EXPECT_EQ(default_types.size(), types.size());
  for (syncer::DataType type : default_types) {
    EXPECT_TRUE(types.Has(type))
        << syncer::DataTypeToDebugString(type) << " not found in datatypes map";
  }
}
