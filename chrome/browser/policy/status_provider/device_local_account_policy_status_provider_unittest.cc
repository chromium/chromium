// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/device_local_account_policy_status_provider.h"

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "chrome/browser/ash/settings/cros_settings_holder.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/policy/status_provider/status_provider_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/invalidation/test_support/fake_invalidation_listener.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kAccountId[] = "public_session_account@localhost";

}  // namespace

class DeviceLocalAccountPolicyStatusProviderTest
    : public ash::DeviceSettingsTestBase {
 public:
  DeviceLocalAccountPolicyStatusProviderTest()
      : ash::DeviceSettingsTestBase(/*profile_creation_enabled=*/false),
        user_id_(GenerateDeviceLocalAccountUserId(
            kAccountId,
            DeviceLocalAccountType::kPublicSession)) {}
  DeviceLocalAccountPolicyStatusProviderTest(
      const DeviceLocalAccountPolicyStatusProviderTest&) = delete;
  DeviceLocalAccountPolicyStatusProviderTest& operator=(
      const DeviceLocalAccountPolicyStatusProviderTest&) = delete;
  ~DeviceLocalAccountPolicyStatusProviderTest() override = default;

  void SetUp() override {
    ash::DeviceSettingsTestBase::SetUp();
    install_attributes_ = std::make_unique<ash::ScopedStubInstallAttributes>();
    cros_settings_holder_ = std::make_unique<ash::CrosSettingsHolder>(
        device_settings_service_.get(),
        TestingBrowserProcess::GetGlobal()->local_state());

    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());

    service_ = std::make_unique<DeviceLocalAccountPolicyService>(
        TestingBrowserProcess::GetGlobal()->shared_url_loader_factory(),
        &session_manager_client_, device_settings_service_.get(),
        ash::CrosSettings::Get(), &invalidation_listener_,
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  void TearDown() override {
    if (service_) {
      service_->Shutdown();
    }
    service_.reset();
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(nullptr);
    cros_settings_holder_.reset();
    install_attributes_.reset();
    ash::DeviceSettingsTestBase::TearDown();
  }

  void AddDeviceLocalAccount() {
    enterprise_management::DeviceLocalAccountInfoProto* account =
        ash::DeviceSettingsTestBase::device_policy_->payload()
            .mutable_device_local_accounts()
            ->add_account();
    account->set_account_id(kAccountId);
    account->set_type(enterprise_management::DeviceLocalAccountInfoProto::
                          ACCOUNT_TYPE_PUBLIC_SESSION);
    ash::DeviceSettingsTestBase::device_policy_->Build();
    session_manager_client_.set_device_policy(
        ash::DeviceSettingsTestBase::device_policy_->GetBlob());
    ash::DeviceSettingsTestBase::ReloadDeviceSettings();
  }

  void InstallUserPolicy() {
    UserPolicyBuilder user_policy;
    user_policy.policy_data().set_policy_type(
        dm_protocol::kChromePublicAccountPolicyType);
    user_policy.policy_data().set_settings_entity_id(kAccountId);
    user_policy.policy_data().set_username(kAccountId);
    user_policy.Build();
    session_manager_client_.set_device_local_account_policy(
        kAccountId, user_policy.GetBlob());
  }

 protected:
  const std::string user_id_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<ash::ScopedStubInstallAttributes> install_attributes_;
  std::unique_ptr<ash::CrosSettingsHolder> cros_settings_holder_;
  invalidation::FakeInvalidationListener invalidation_listener_;
  std::unique_ptr<DeviceLocalAccountPolicyService> service_;
};

TEST_F(DeviceLocalAccountPolicyStatusProviderTest, GetStatusMojo_NoBroker) {
  DeviceLocalAccountPolicyStatusProvider provider("nonexistent@example.com",
                                                  service_.get());
  policy::mojom::StatusPtr status = provider.GetStatusMojo();

  EXPECT_TRUE(status->error);
  EXPECT_EQ(status->policy_description_key, kUserPolicyStatusDescription);
  EXPECT_TRUE(status->username.has_value());
  EXPECT_TRUE(status->username->empty());
}

TEST_F(DeviceLocalAccountPolicyStatusProviderTest, GetStatusMojo_WithBroker) {
  InstallUserPolicy();
  AddDeviceLocalAccount();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return service_->IsPolicyAvailableForUser(user_id_); }));

  DeviceLocalAccountPolicyBroker* broker = service_->GetBrokerForUser(user_id_);
  ASSERT_TRUE(broker);
  auto fake_client = std::make_unique<CloudPolicyClient>(
      /*service=*/nullptr, /*url_loader_factory=*/nullptr);
  broker->core()->Connect(std::move(fake_client));

  DeviceLocalAccountPolicyStatusProvider provider(user_id_, service_.get());
  policy::mojom::StatusPtr status = provider.GetStatusMojo();

  EXPECT_FALSE(status->error);
  EXPECT_EQ(status->policy_description_key, kUserPolicyStatusDescription);
  EXPECT_TRUE(status->username.has_value());
  EXPECT_EQ(status->username.value(), kAccountId);
  EXPECT_EQ(status->domain, "localhost");
}

}  // namespace policy
