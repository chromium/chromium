// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chromeos_system_profile_provider.h"

#include <stdint.h>

#include <string>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

using Hardware = metrics::SystemProfileProto::Hardware;

namespace {

class FakeMultiDeviceSetupClientImplFactory
    : public ash::multidevice_setup::MultiDeviceSetupClientImpl::Factory {
 public:
  explicit FakeMultiDeviceSetupClientImplFactory(
      std::unique_ptr<ash::multidevice_setup::FakeMultiDeviceSetupClient>
          fake_multidevice_setup_client)
      : fake_multidevice_setup_client_(
            std::move(fake_multidevice_setup_client)) {}

  ~FakeMultiDeviceSetupClientImplFactory() override = default;

  // ash::multidevice_setup::MultiDeviceSetupClientImpl::Factory:
  // NOTE: At most, one client should be created per-test.
  std::unique_ptr<ash::multidevice_setup::MultiDeviceSetupClient>
  CreateInstance(
      mojo::PendingRemote<ash::multidevice_setup::mojom::MultiDeviceSetup>)
      override {
    EXPECT_TRUE(fake_multidevice_setup_client_);
    return std::move(fake_multidevice_setup_client_);
  }

 private:
  std::unique_ptr<ash::multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
};

// Wrapper around ChromeOSSystemProfileProvider that initializes
// hardware class in the constructor.
class TestChromeOSSystemProfileProvider : public ChromeOSSystemProfileProvider {
 public:
  TestChromeOSSystemProfileProvider() {
    base::RunLoop run_loop;
    AsyncInit(run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

  void GetIdleCallback() {
    ASSERT_TRUE(base::RunLoop::IsRunningOnCurrentThread());
    base::RunLoop().QuitWhenIdle();
  }
};

const AccountId account_id1(AccountId::FromUserEmail("user1@example.com"));
const AccountId account_id2(AccountId::FromUserEmail("user2@example.com"));
const AccountId account_id3(AccountId::FromUserEmail("user3@example.com"));

}  // namespace

class ChromeOSSystemProfileProviderTest : public testing::Test {
 public:
  ChromeOSSystemProfileProviderTest() = default;

  ChromeOSSystemProfileProviderTest(const ChromeOSSystemProfileProviderTest&) =
      delete;
  ChromeOSSystemProfileProviderTest& operator=(
      const ChromeOSSystemProfileProviderTest&) = delete;

 protected:
  void SetUp() override {
    chromeos::TpmManagerClient::InitializeFake();

    ash::multidevice_setup::MultiDeviceSetupClientFactory::GetInstance()
        ->SetServiceIsNULLWhileTestingForTesting(false);
    auto fake_multidevice_setup_client =
        std::make_unique<ash::multidevice_setup::FakeMultiDeviceSetupClient>();
    fake_multidevice_setup_client_ = fake_multidevice_setup_client.get();
    fake_multidevice_setup_client_impl_factory_ =
        std::make_unique<FakeMultiDeviceSetupClientImplFactory>(
            std::move(fake_multidevice_setup_client));
    ash::multidevice_setup::MultiDeviceSetupClientImpl::Factory::
        SetFactoryForTesting(fake_multidevice_setup_client_impl_factory_.get());

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ = profile_manager_->CreateTestingProfile("test_name");

    // Set statistic provider for hardware class tests.
    ash::system::StatisticsProvider::SetTestProvider(
        &fake_statistics_provider_);

    // Initialize the login state trackers.
    if (!ash::LoginState::IsInitialized())
      ash::LoginState::Initialize();
  }

  void TearDown() override {
    // Destroy the login state tracker if it was initialized.
    ash::LoginState::Shutdown();
    chromeos::TpmManagerClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    ash::multidevice_setup::MultiDeviceSetupClientImpl::Factory::
        SetFactoryForTesting(nullptr);
    profile_manager_.reset();
  }

 protected:
  raw_ptr<ash::multidevice_setup::FakeMultiDeviceSetupClient, DanglingUntriaged>
      fake_multidevice_setup_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> testing_profile_ = nullptr;
  std::unique_ptr<FakeMultiDeviceSetupClientImplFactory>
      fake_multidevice_setup_client_impl_factory_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ChromeOSSystemProfileProviderTest, MultiProfileUserCount) {
  // |scoped_enabler| takes over the lifetime of |user_manager|.
  auto* user_manager = new ash::FakeChromeUserManager();
  // TODO(crbug.com/40735060): Overload operator-> in ScopedUserManager.
  user_manager::ScopedUserManager scoped_enabler(
      base::WrapUnique(user_manager));
  user_manager->AddKioskAppUser(account_id1);
  user_manager->AddKioskAppUser(account_id2);
  user_manager->AddKioskAppUser(account_id3);

  user_manager->LoginUser(account_id1);
  user_manager->LoginUser(account_id3);

  TestChromeOSSystemProfileProvider provider;
  provider.OnDidCreateMetricsLog();
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(2u, system_profile.multi_profile_user_count());
}

TEST_F(ChromeOSSystemProfileProviderTest, MultiProfileCountInvalidated) {
  // |scoped_enabler| takes over the lifetime of |user_manager|.
  auto* user_manager = new ash::FakeChromeUserManager();
  // TODO(crbug.com/40735060): Overload operator-> in ScopedUserManager.
  user_manager::ScopedUserManager scoped_enabler(
      base::WrapUnique(user_manager));
  user_manager->AddKioskAppUser(account_id1);
  user_manager->AddKioskAppUser(account_id2);
  user_manager->AddKioskAppUser(account_id3);

  user_manager->LoginUser(account_id1);

  TestChromeOSSystemProfileProvider provider;
  provider.OnDidCreateMetricsLog();

  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(1u, system_profile.multi_profile_user_count());

  user_manager->LoginUser(account_id2);
  provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(0u, system_profile.multi_profile_user_count());
}

TEST_F(ChromeOSSystemProfileProviderTest,
       HasLinkedAndroidPhoneAndEnabledFeatures) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(ash::multidevice_setup::mojom::HostStatus::kHostVerified,
                     ash::multidevice::CreateRemoteDeviceRefForTest()));
  fake_multidevice_setup_client_->SetFeatureState(
      ash::multidevice_setup::mojom::Feature::kInstantTethering,
      ash::multidevice_setup::mojom::FeatureState::kEnabledByUser);
  fake_multidevice_setup_client_->SetFeatureState(
      ash::multidevice_setup::mojom::Feature::kSmartLock,
      ash::multidevice_setup::mojom::FeatureState::kEnabledByUser);

  // |scoped_enabler| takes over the lifetime of |user_manager|.
  auto* user_manager = new ash::FakeChromeUserManager();
  // TODO(crbug.com/40735060): Overload operator-> in ScopedUserManager.
  user_manager::ScopedUserManager scoped_enabler(
      base::WrapUnique(user_manager));
  user_manager->AddKioskAppUser(account_id1);
  user_manager->LoginUser(account_id1);
  const user_manager::User* primary_user = user_manager->GetPrimaryUser();
  ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
      primary_user, testing_profile_);

  TestChromeOSSystemProfileProvider provider;
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  EXPECT_TRUE(system_profile.has_linked_android_phone_data());
  EXPECT_TRUE(
      system_profile.linked_android_phone_data().has_phone_model_name_hash());
  EXPECT_TRUE(system_profile.linked_android_phone_data()
                  .is_instant_tethering_enabled());
  EXPECT_TRUE(
      system_profile.linked_android_phone_data().is_smartlock_enabled());
  EXPECT_FALSE(
      system_profile.linked_android_phone_data().is_messages_enabled());
}

TEST_F(ChromeOSSystemProfileProviderTest, FullHardwareClass) {
  const std::string expected_full_hw_class = "feature_enabled";
  fake_statistics_provider_.SetMachineStatistic("hardware_class",
                                                expected_full_hw_class);

  TestChromeOSSystemProfileProvider provider;
  provider.OnDidCreateMetricsLog();
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  ASSERT_TRUE(system_profile.has_hardware());
  std::string proto_full_hw_class =
      system_profile.hardware().full_hardware_class();

  EXPECT_EQ(expected_full_hw_class, proto_full_hw_class);
}

TEST_F(ChromeOSSystemProfileProviderTest, DemoModeDimensions) {
  testing_profile_->ScopedCrosSettingsTestHelper()
      ->InstallAttributes()
      ->SetDemoMode();
  const std::string expected_country = "CA";
  const std::string expected_retailer_id = "ABC";
  const std::string expected_store_id = "12345";
  const std::string app_expected_version = "0.0.0.0";
  const std::string resources_expected_version = "0.0.0.1";
  scoped_feature_list_.InitWithFeatures(
      {chromeos::features::kCloudGamingDevice,
       ash::features::kFeatureManagementFeatureAwareDeviceDemoMode},
      {});
  g_browser_process->local_state()->SetString("demo_mode.country",
                                              expected_country);
  g_browser_process->local_state()->SetString("demo_mode.retailer_id",
                                              expected_retailer_id);
  g_browser_process->local_state()->SetString("demo_mode.store_id",
                                              expected_store_id);
  g_browser_process->local_state()->SetString("demo_mode.app_version",
                                              app_expected_version);
  g_browser_process->local_state()->SetString("demo_mode.resources_version",
                                              resources_expected_version);

  TestChromeOSSystemProfileProvider provider;
  provider.OnDidCreateMetricsLog();
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  ASSERT_TRUE(system_profile.has_demo_mode_dimensions());
  ASSERT_TRUE(system_profile.demo_mode_dimensions().has_country());
  ASSERT_TRUE(system_profile.demo_mode_dimensions().has_retailer());
  ASSERT_TRUE(
      system_profile.demo_mode_dimensions().retailer().has_retailer_id());
  ASSERT_TRUE(system_profile.demo_mode_dimensions().retailer().has_store_id());
  EXPECT_EQ(system_profile.demo_mode_dimensions().customization_facet_size(),
            2);
  ASSERT_EQ(
      system_profile.demo_mode_dimensions().customization_facet().at(0),
      metrics::
          SystemProfileProto_DemoModeDimensions_CustomizationFacet_CLOUD_GAMING_DEVICE);
  ASSERT_EQ(
      system_profile.demo_mode_dimensions().customization_facet().at(1),
      metrics::
          SystemProfileProto_DemoModeDimensions_CustomizationFacet_FEATURE_AWARE_DEVICE);
  std::string country = system_profile.demo_mode_dimensions().country();
  std::string retailer_id =
      system_profile.demo_mode_dimensions().retailer().retailer_id();
  std::string store_id =
      system_profile.demo_mode_dimensions().retailer().store_id();
  std::string app_version = system_profile.demo_mode_dimensions().app_version();
  std::string resources_version =
      system_profile.demo_mode_dimensions().resources_version();

  EXPECT_EQ(country, expected_country);
  EXPECT_EQ(retailer_id, expected_retailer_id);
  EXPECT_EQ(store_id, expected_store_id);
  EXPECT_EQ(app_version, app_expected_version);
  EXPECT_EQ(resources_version, resources_expected_version);
}

TEST_F(ChromeOSSystemProfileProviderTest, TpmRwFirmwareVersion) {
  const std::string expected_rw_firmware_version = "0.5.190";
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_version_info_reply()
      ->set_rw_version(expected_rw_firmware_version);

  TestChromeOSSystemProfileProvider provider;
  provider.OnDidCreateMetricsLog();
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  ASSERT_TRUE(system_profile.has_hardware());
  ASSERT_TRUE(system_profile.hardware().has_tpm_rw_firmware_version());

  EXPECT_EQ(system_profile.hardware().tpm_rw_firmware_version(),
            expected_rw_firmware_version);
}
