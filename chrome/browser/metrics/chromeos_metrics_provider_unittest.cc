// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chromeos_metrics_provider.h"

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace {

class FakeMultiDeviceSetupClientImplFactory
    : public chromeos::multidevice_setup::MultiDeviceSetupClientImpl::Factory {
 public:
  FakeMultiDeviceSetupClientImplFactory(
      std::unique_ptr<chromeos::multidevice_setup::FakeMultiDeviceSetupClient>
          fake_multidevice_setup_client)
      : fake_multidevice_setup_client_(
            std::move(fake_multidevice_setup_client)) {}

  ~FakeMultiDeviceSetupClientImplFactory() override = default;

  // chromeos::multidevice_setup::MultiDeviceSetupClientImpl::Factory:
  // NOTE: At most, one client should be created per-test.
  std::unique_ptr<chromeos::multidevice_setup::MultiDeviceSetupClient>
  CreateInstance(
      mojo::PendingRemote<chromeos::multidevice_setup::mojom::MultiDeviceSetup>)
      override {
    EXPECT_TRUE(fake_multidevice_setup_client_);
    return std::move(fake_multidevice_setup_client_);
  }

 private:
  std::unique_ptr<chromeos::multidevice_setup::FakeMultiDeviceSetupClient>
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

 protected:
  void SetUp() override {
    // Set up a PowerManagerClient instance for PerfProvider.
    chromeos::PowerManagerClient::InitializeFake();

    chromeos::multidevice_setup::MultiDeviceSetupClientFactory::GetInstance()
        ->SetServiceIsNULLWhileTestingForTesting(false);
    auto fake_multidevice_setup_client = std::make_unique<
        chromeos::multidevice_setup::FakeMultiDeviceSetupClient>();
    fake_multidevice_setup_client_ = fake_multidevice_setup_client.get();
    fake_multidevice_setup_client_impl_factory_ =
        std::make_unique<FakeMultiDeviceSetupClientImplFactory>(
            std::move(fake_multidevice_setup_client));
    chromeos::multidevice_setup::MultiDeviceSetupClientImpl::Factory::
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

  void TearDown() override {
    // Destroy the login state tracker if it was initialized.
    chromeos::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    chromeos::multidevice_setup::MultiDeviceSetupClientImpl::Factory::
        SetFactoryForTesting(nullptr);
    profile_manager_.reset();
  }

 protected:
  chromeos::multidevice_setup::FakeMultiDeviceSetupClient*
      fake_multidevice_setup_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* testing_profile_ = nullptr;
  std::unique_ptr<FakeMultiDeviceSetupClientImplFactory>
      fake_multidevice_setup_client_impl_factory_;

 private:
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSMetricsProviderTest);
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
  fake_multidevice_setup_client_->SetHostStatusWithDevice(std::make_pair(
      chromeos::multidevice_setup::mojom::HostStatus::kHostVerified,
      chromeos::multidevice::CreateRemoteDeviceRefForTest()));
  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kInstantTethering,
      chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser);
  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kSmartLock,
      chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser);
  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kMessages,
      chromeos::multidevice_setup::mojom::FeatureState::kFurtherSetupRequired);

  // |scoped_enabler| takes over the lifetime of |user_manager|.
  auto* user_manager = new ash::FakeChromeUserManager();
  // TODO(crbug/1154780): Overload operator-> in ScopedUserManager.
  user_manager::ScopedUserManager scoped_enabler(
      base::WrapUnique(user_manager));
  user_manager->AddKioskAppUser(account_id1);
  user_manager->LoginUser(account_id1);
  const user_manager::User* primary_user = user_manager->GetPrimaryUser();
  chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
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
