// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_factory.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_switches.h"
#endif

namespace resource_coordinator {

namespace {

// Ensures that a SiteCharacteristicsDataStore respect the |IsOffTheRecord| of
// its corresponding profile.
bool DataStoreRespectsOffTheRecordValue(
    Profile* profile,
    SiteCharacteristicsDataStore* data_store) {
  return profile->IsOffTheRecord() == !data_store->IsRecordingForTesting();
}

}  // namespace

class LocalSiteCharacteristicsDataStoreFactoryTest
    : public InProcessBrowserTest {
 protected:
  LocalSiteCharacteristicsDataStoreFactoryTest() = default;
  ~LocalSiteCharacteristicsDataStoreFactoryTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSiteCharacteristicsDatabase);
    InProcessBrowserTest::SetUp();
  }

#if defined(OS_CHROMEOS)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
  }
#endif

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(LocalSiteCharacteristicsDataStoreFactoryTest);
};

IN_PROC_BROWSER_TEST_F(LocalSiteCharacteristicsDataStoreFactoryTest, EndToEnd) {
  Profile* regular_profile = browser()->profile();
  ASSERT_TRUE(regular_profile);
  SiteCharacteristicsDataStore* recording_data_store =
      LocalSiteCharacteristicsDataStoreFactory::GetForProfile(regular_profile);
  ASSERT_TRUE(recording_data_store);
  EXPECT_TRUE(DataStoreRespectsOffTheRecordValue(regular_profile,
                                                 recording_data_store));

  Profile* incognito_profile = regular_profile->GetOffTheRecordProfile();
  ASSERT_TRUE(incognito_profile);
  SiteCharacteristicsDataStore* incognito_data_store =
      static_cast<SiteCharacteristicsDataStore*>(
          LocalSiteCharacteristicsDataStoreFactory::GetForProfile(
              incognito_profile));
  ASSERT_TRUE(incognito_data_store);
  EXPECT_NE(recording_data_store, incognito_data_store);
  EXPECT_TRUE(DataStoreRespectsOffTheRecordValue(incognito_profile,
                                                 incognito_data_store));

  profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());
  ui_test_utils::WaitForBrowserToOpen();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* guest_profile =
      profile_manager->GetProfileByPath(ProfileManager::GetGuestProfilePath());
  ASSERT_TRUE(guest_profile);
  SiteCharacteristicsDataStore* guest_data_store =
      static_cast<SiteCharacteristicsDataStore*>(
          LocalSiteCharacteristicsDataStoreFactory::GetForProfile(
              guest_profile));
  ASSERT_TRUE(guest_data_store);
  EXPECT_NE(recording_data_store, guest_data_store);
  EXPECT_TRUE(
      DataStoreRespectsOffTheRecordValue(guest_profile, guest_data_store));
}

}  // namespace resource_coordinator
