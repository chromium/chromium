// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_service_factory.h"

#include <stddef.h>

#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/sync_service_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/sync/wifi_configuration_sync_service_factory.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/network_handler.h"
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
    // Some services will only be created if there is a WebDataService.
    builder.AddTestingFactory(WebDataServiceFactory::GetInstance(),
                              WebDataServiceFactory::GetDefaultFactory());
    profile_ = builder.Build();
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    app_list::AppListSyncableServiceFactory::SetUseInTesting(false);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    base::ThreadPoolInstance::Get()->FlushForTesting();
  }

 protected:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  SyncServiceFactoryTest() {
    // Fake network stack is required for WIFI_CONFIGURATIONS datatype.
    ash::NetworkHandler::Initialize();
  }
  ~SyncServiceFactoryTest() override { ash::NetworkHandler::Shutdown(); }
#else
  SyncServiceFactoryTest() = default;
  ~SyncServiceFactoryTest() override = default;
#endif

  // Returns the collection of default datatypes.
  syncer::ModelTypeSet DefaultDatatypes() {
    static_assert(45 == syncer::GetNumModelTypes(),
                  "When adding a new type, you probably want to add it here as "
                  "well (assuming it is already enabled).");

    syncer::ModelTypeSet datatypes;

    // These preprocessor conditions and their order should be in sync with
    // preprocessor conditions in ChromeSyncClient::CreateDataTypeControllers:

    // ChromeSyncClient types.
    datatypes.Put(syncer::READING_LIST);
    datatypes.Put(syncer::SECURITY_EVENTS);
    if (base::FeatureList::IsEnabled(syncer::kSyncSegmentationDataType)) {
      datatypes.Put(syncer::SEGMENTATION);
    }

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    datatypes.Put(syncer::SUPERVISED_USER_SETTINGS);
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

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
    if (features::kTabGroupsSaveSyncIntegration.Get()) {
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
    datatypes.Put(syncer::OS_PREFERENCES);
    datatypes.Put(syncer::OS_PRIORITY_PREFERENCES);
    datatypes.Put(syncer::PRINTERS);
    if (ash::features::IsOAuthIppEnabled()) {
      datatypes.Put(syncer::PRINTERS_AUTHORIZATION_SERVERS);
    }
    datatypes.Put(syncer::WIFI_CONFIGURATIONS);
    datatypes.Put(syncer::WORKSPACE_DESK);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // Common types. This excludes PASSWORDS because the password store factory
    // is null for testing and hence no controller gets instantiated.
    datatypes.Put(syncer::AUTOFILL);
    datatypes.Put(syncer::AUTOFILL_PROFILE);
    datatypes.Put(syncer::AUTOFILL_WALLET_DATA);
    datatypes.Put(syncer::AUTOFILL_WALLET_METADATA);
    datatypes.Put(syncer::AUTOFILL_WALLET_OFFER);
    datatypes.Put(syncer::BOOKMARKS);
    if (base::FeatureList::IsEnabled(syncer::kSyncEnableContactInfoDataType)) {
      datatypes.Put(syncer::CONTACT_INFO);
    }
    datatypes.Put(syncer::DEVICE_INFO);
    if (base::FeatureList::IsEnabled(syncer::kSyncEnableHistoryDataType)) {
      datatypes.Put(syncer::HISTORY);
    }
    datatypes.Put(syncer::HISTORY_DELETE_DIRECTIVES);
    datatypes.Put(syncer::PREFERENCES);
    datatypes.Put(syncer::PRIORITY_PREFERENCES);
    datatypes.Put(syncer::SESSIONS);
    datatypes.Put(syncer::PROXY_TABS);
    datatypes.Put(syncer::TYPED_URLS);
    datatypes.Put(syncer::USER_EVENTS);
    datatypes.Put(syncer::USER_CONSENTS);
    datatypes.Put(syncer::SEND_TAB_TO_SELF);
    datatypes.Put(syncer::SHARING_MESSAGE);
    return datatypes;
  }

  Profile* profile() { return profile_.get(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  syncer::ModelTypeSet types = sync_service->GetRegisteredDataTypesForTest();
  const syncer::ModelTypeSet default_types = DefaultDatatypes();
  EXPECT_EQ(default_types.Size(), types.Size());
  for (syncer::ModelType type : default_types) {
    EXPECT_TRUE(types.Has(type)) << type << " not found in datatypes map";
  }
  sync_service->Shutdown();
  RunUntilIdle();
}
