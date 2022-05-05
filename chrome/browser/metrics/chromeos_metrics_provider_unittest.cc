// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chromeos_metrics_provider.h"

#include <string>

#include "ash/components/multidevice/remote_device_test_util.h"
#include "ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "ash/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

using Hardware = metrics::SystemProfileProto::Hardware;

namespace {

constexpr int kTpmV1Family = 0x312e3200;
constexpr int kTpmV2Family = 0x322e3000;

class FakeMultiDeviceSetupClientImplFactory
    : public ash::multidevice_setup::MultiDeviceSetupClientImpl::Factory {
 public:
  FakeMultiDeviceSetupClientImplFactory(
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

// Wrapper around ChromeOSMetricsProvider that initializes
// hardware class in the constructor.
class TestChromeOSMetricsProvider : public ChromeOSMetricsProvider {
 public:
  TestChromeOSMetricsProvider()
      : ChromeOSMetricsProvider(metrics::MetricsLogUploader::UMA) {
    AsyncInit(base::BindOnce(&TestChromeOSMetricsProvider::GetIdleCallback,
                             base::Unretained(this)));
    base::RunLoop().Run();
  }

  void GetIdleCallback() {
    ASSERT_TRUE(base::RunLoop::IsRunningOnCurrentThread());
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }
};

const AccountId account_id1(AccountId::FromUserEmail("user1@example.com"));
const AccountId account_id2(AccountId::FromUserEmail("user2@example.com"));
const AccountId account_id3(AccountId::FromUserEmail("user3@example.com"));

}  // namespace

class ChromeOSMetricsProviderTest : public testing::Test {
 public:
  ChromeOSMetricsProviderTest() {}

  ChromeOSMetricsProviderTest(const ChromeOSMetricsProviderTest&) = delete;
  ChromeOSMetricsProviderTest& operator=(const ChromeOSMetricsProviderTest&) =
      delete;

 protected:
  void SetUp() override {
    // Set up a PowerManagerClient instance for PerfProvider.
    chromeos::PowerManagerClient::InitializeFake();
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
    chromeos::system::StatisticsProvider::SetTestProvider(
        &fake_statistics_provider_);

    // Initialize the login state trackers.
    if (!chromeos::LoginState::IsInitialized())
      chromeos::LoginState::Initialize();
  }

  void CheckTpmType(const Hardware::TpmType& expected_tpm_type) {
    TestChromeOSMetricsProvider provider;
    provider.OnDidCreateMetricsLog();
    metrics::SystemProfileProto system_profile;
    provider.ProvideSystemProfileMetrics(&system_profile);

    ASSERT_TRUE(system_profile.has_hardware());
    const Hardware::TpmType proto_tpm_type =
        system_profile.hardware().tpm_type();

    EXPECT_EQ(expected_tpm_type, proto_tpm_type);
  }

  void TearDown() override {
    // Destroy the login state tracker if it was initialized.
    chromeos::LoginState::Shutdown();
    chromeos::TpmManagerClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    ash::multidevice_setup::MultiDeviceSetupClientImpl::Factory::
        SetFactoryForTesting(nullptr);
    profile_manager_.reset();
  }

 protected:
  ash::multidevice_setup::FakeMultiDeviceSetupClient*
      fake_multidevice_setup_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* testing_profile_ = nullptr;
  std::unique_ptr<FakeMultiDeviceSetupClientImplFactory>
      fake_multidevice_setup_client_impl_factory_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ChromeOSMetricsProviderTest, MultiProfileUserCount) {
  // |scoped_enabler| takes over the lifetime of |user_manager|.
  auto* user_manager = new ash::FakeChromeUserManager();
  // TODO(crbug/1154780): Overload operator-> in ScopedUserManager.
  user_manager::ScopedUserManager scoped_enabler(
      base::WrapUnique(user_manager));
  user_manager->AddKioskAppUser(account_id1);
  user_manager->AddKioskAppUser(account_id2);
  user_manager->AddKioskAppUser(account_id3);

  user_manager->LoginUser(account_id1);
  user_manager->LoginUser(account_id3);

  TestChromeOSMetricsProvider provider;
  provider.OnDidCreateMetricsLog();
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(2u, system_profile.multi_profile_user_count());
}

TEST_F(ChromeOSMetricsProviderTest, MultiProfileCountInvalidated) {
  // |scoped_enabler| takes over the lifetime of |user_manager|.
  auto* user_manager = new ash::FakeChromeUserManager();
  // TODO(crbug/1154780): Overload operator-> in ScopedUserManager.
  user_manager::ScopedUserManager scoped_enabler(
      base::WrapUnique(user_manager));
  user_manager->AddKioskAppUser(account_id1);
  user_manager->AddKioskAppUser(account_id2);
  user_manager->AddKioskAppUser(account_id3);

  user_manager->LoginUser(account_id1);

  TestChromeOSMetricsProvider provider;
  provider.OnDidCreateMetricsLog();

  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(1u, system_profile.multi_profile_user_count());

  user_manager->LoginUser(account_id2);
  provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(0u, system_profile.multi_profile_user_count());
}

TEST_F(ChromeOSMetricsProviderTest, HasLinkedAndroidPhoneAndEnabledFeatures) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(
      std::make_pair(ash::multidevice_setup::mojom::HostStatus::kHostVerified,
                     ash::multidevice::CreateRemoteDeviceRefForTest()));
  fake_multidevice_setup_client_->SetFeatureState(
      ash::multidevice_setup::mojom::Feature::kInstantTethering,
      ash::multidevice_setup::mojom::FeatureState::kEnabledByUser);
  fake_multidevice_setup_client_->SetFeatureState(
      ash::multidevice_setup::mojom::Feature::kSmartLock,
      ash::multidevice_setup::mojom::FeatureState::kEnabledByUser);
  fake_multidevice_setup_client_->SetFeatureState(
      ash::multidevice_setup::mojom::Feature::kMessages,
      ash::multidevice_setup::mojom::FeatureState::kFurtherSetupRequired);

  // |scoped_enabler| takes over the lifetime of |user_manager|.
  auto* user_manager = new ash::FakeChromeUserManager();
  // TODO(crbug/1154780): Overload operator-> in ScopedUserManager.
  user_manager::ScopedUserManager scoped_enabler(
      base::WrapUnique(user_manager));
  user_manager->AddKioskAppUser(account_id1);
  user_manager->LoginUser(account_id1);
  const user_manager::User* primary_user = user_manager->GetPrimaryUser();
  ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
      primary_user, testing_profile_);

  TestChromeOSMetricsProvider provider;
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

TEST_F(ChromeOSMetricsProviderTest, FullHardwareClass) {
  const std::string expected_full_hw_class = "feature_enabled";
  fake_statistics_provider_.SetMachineStatistic("hardware_class",
                                                expected_full_hw_class);

  TestChromeOSMetricsProvider provider;
  provider.OnDidCreateMetricsLog();
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  ASSERT_TRUE(system_profile.has_hardware());
  std::string proto_full_hw_class =
      system_profile.hardware().full_hardware_class();

  EXPECT_EQ(expected_full_hw_class, proto_full_hw_class);
}

TEST_F(ChromeOSMetricsProviderTest, TpmTypeRuntimeSelection) {
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_supported_features_reply()
      ->set_support_runtime_selection(true);

  CheckTpmType(Hardware::TPM_TYPE_RUNTIME_SELECTION);
}

TEST_F(ChromeOSMetricsProviderTest, TpmTypeV1) {
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_version_info_reply()
      ->set_family(kTpmV1Family);

  CheckTpmType(Hardware::TPM_TYPE_1);
}

TEST_F(ChromeOSMetricsProviderTest, TpmTypeCr50) {
  tpm_manager::GetVersionInfoReply* reply = chromeos::TpmManagerClient::Get()
                                                ->GetTestInterface()
                                                ->mutable_version_info_reply();
  reply->set_gsc_version(tpm_manager::GSC_VERSION_CR50);
  reply->set_family(kTpmV2Family);

  CheckTpmType(Hardware::TPM_TYPE_CR50);
}

TEST_F(ChromeOSMetricsProviderTest, TpmTypeTi50) {
  tpm_manager::GetVersionInfoReply* reply = chromeos::TpmManagerClient::Get()
                                                ->GetTestInterface()
                                                ->mutable_version_info_reply();
  reply->set_gsc_version(tpm_manager::GSC_VERSION_TI50);
  reply->set_family(kTpmV2Family);

  CheckTpmType(Hardware::TPM_TYPE_TI50);
}

TEST_F(ChromeOSMetricsProviderTest, TpmTypeInvalidFamily) {
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_version_info_reply()
      ->set_family(100);

  CheckTpmType(Hardware::TPM_TYPE_UNKNOWN);
}

TEST_F(ChromeOSMetricsProviderTest, TpmTypeGenericTpm2) {
  tpm_manager::GetVersionInfoReply* reply = chromeos::TpmManagerClient::Get()
                                                ->GetTestInterface()
                                                ->mutable_version_info_reply();
  reply->set_family(kTpmV2Family);

  CheckTpmType(Hardware::TPM_TYPE_UNKNOWN);
}

TEST_F(ChromeOSMetricsProviderTest, TpmTypeInvalidGscWithFamilyV1) {
  tpm_manager::GetVersionInfoReply* reply = chromeos::TpmManagerClient::Get()
                                                ->GetTestInterface()
                                                ->mutable_version_info_reply();
  reply->set_family(kTpmV1Family);
  reply->set_gsc_version(tpm_manager::GSC_VERSION_CR50);

  CheckTpmType(Hardware::TPM_TYPE_UNKNOWN);
}
