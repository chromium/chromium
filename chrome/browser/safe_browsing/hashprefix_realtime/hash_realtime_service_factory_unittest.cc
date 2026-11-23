// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/hashprefix_realtime/hash_realtime_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/safe_browsing/network_context_service.h"
#include "chrome/browser/safe_browsing/network_context_service_factory.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#endif

namespace safe_browsing {

class HashRealTimeServiceFactoryTest : public testing::Test {
 protected:
  HashRealTimeServiceFactoryTest() = default;
  ~HashRealTimeServiceFactoryTest() override = default;
  void SetUp() override {
    SystemNetworkContextManager::CreateInstance(
        TestingBrowserProcess::GetGlobal()->local_state());

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    ASSERT_TRUE(g_browser_process->profile_manager());

    sb_service_ =
        base::MakeRefCounted<safe_browsing::TestSafeBrowsingService>();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        sb_service_.get());
    g_browser_process->safe_browsing_service()->Initialize();
  }
  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    profile_manager_.reset();
    SystemNetworkContextManager::DeleteInstance();
  }

  network::mojom::NetworkContext* GetNetworkContext(Profile* profile) {
    return HashRealTimeServiceFactory::GetNetworkContext(profile);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
#endif
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<TestingProfileManager> profile_manager_;

 private:
  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
};

TEST_F(HashRealTimeServiceFactoryTest, EnabledForRegularProfiles) {
  TestingProfile* profile = profile_manager_->CreateTestingProfile("profile");
  EXPECT_NE(nullptr, HashRealTimeServiceFactory::GetForProfile(profile));
  network::mojom::NetworkContext* network_context = GetNetworkContext(profile);
  EXPECT_NE(nullptr, network_context);
  EXPECT_EQ(NetworkContextServiceFactory::GetForBrowserContext(
                profile, /*use_same_instance_in_off_the_record=*/false)
                ->GetNetworkContext(),
            network_context);
}

TEST_F(HashRealTimeServiceFactoryTest, EnabledForIncognitoMode) {
  TestingProfile* regular_profile =
      profile_manager_->CreateTestingProfile("testing_profile");
  TestingProfile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(regular_profile);
  HashRealTimeService* regular_service =
      HashRealTimeServiceFactory::GetForProfile(regular_profile);
  EXPECT_NE(nullptr, regular_service);
  HashRealTimeService* incognito_service =
      HashRealTimeServiceFactory::GetForProfile(incognito_profile);
  EXPECT_NE(nullptr, incognito_service);
  EXPECT_NE(regular_service, incognito_service);

  network::mojom::NetworkContext* regular_network_context =
      GetNetworkContext(regular_profile);
  network::mojom::NetworkContext* incognito_network_context =
      GetNetworkContext(incognito_profile);
  EXPECT_NE(nullptr, regular_network_context);
  EXPECT_NE(nullptr, incognito_network_context);
  EXPECT_NE(regular_network_context, incognito_network_context);
  EXPECT_EQ(NetworkContextServiceFactory::GetForBrowserContext(
                incognito_profile,
                /*use_same_instance_in_off_the_record=*/false)
                ->GetNetworkContext(),
            incognito_network_context);
}

TEST_F(HashRealTimeServiceFactoryTest, EnabledForGuestMode) {
  TestingProfile* guest_parent_profile = profile_manager_->CreateGuestProfile();
  EXPECT_EQ(nullptr,
            HashRealTimeServiceFactory::GetForProfile(guest_parent_profile));
  Profile* guest_otr_profile =
      guest_parent_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_NE(nullptr,
            HashRealTimeServiceFactory::GetForProfile(guest_otr_profile));

  network::mojom::NetworkContext* guest_otr_network_context =
      GetNetworkContext(guest_otr_profile);
  EXPECT_NE(nullptr, guest_otr_network_context);
  EXPECT_EQ(NetworkContextServiceFactory::GetForBrowserContext(
                guest_otr_profile,
                /*use_same_instance_in_off_the_record=*/false)
                ->GetNetworkContext(),
            guest_otr_network_context);
  EXPECT_NE(NetworkContextServiceFactory::GetForBrowserContext(
                guest_parent_profile,
                /*use_same_instance_in_off_the_record=*/true)
                ->GetNetworkContext(),
            guest_otr_network_context);
}

}  // namespace safe_browsing
