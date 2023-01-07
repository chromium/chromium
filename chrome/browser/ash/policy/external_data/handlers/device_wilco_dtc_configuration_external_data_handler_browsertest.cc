// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/external_data/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/ash/wilco_dtc_supportd/wilco_dtc_supportd_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Class used to test DeviceWilcoDtcConfigurationExternalPolicyHandler depending
// on the feature flag.
class DeviceWilcoDtcConfigurationExternalPolicyHandlerTest
    : public DevicePolicyCrosBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  DeviceWilcoDtcConfigurationExternalPolicyHandlerTest() {
    feature_list_.InitWithFeatureState(::features::kWilcoDtc,
                                       IsWilcoDtcFeatureEnabled());
  }
  ~DeviceWilcoDtcConfigurationExternalPolicyHandlerTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    DevicePolicyCrosBrowserTest::SetUpOnMainThread();

    policy_change_waiting_run_loop_ = std::make_unique<base::RunLoop>();

    BrowserPolicyConnectorAsh* policy_connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    ASSERT_TRUE(policy_connector);
    policy_service_ = policy_connector->GetPolicyService();
    ASSERT_TRUE(
        policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
    policy_change_registrar_ = std::make_unique<PolicyChangeRegistrar>(
        policy_service_, PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
    policy_change_registrar_->Observe(
        key::kDeviceWilcoDtcConfiguration,
        base::BindRepeating(
            &DeviceWilcoDtcConfigurationExternalPolicyHandlerTest ::
                PolicyChangedCallback,
            base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    policy_change_registrar_.reset();
    DevicePolicyCrosBrowserTest::TearDownOnMainThread();
  }

  bool IsWilcoDtcFeatureEnabled() { return GetParam(); }

  void SetDeviceWilcoDtcConfigurationExternalData(const std::string& policy) {
    device_policy()
        ->payload()
        .mutable_device_wilco_dtc_configuration()
        ->set_device_wilco_dtc_configuration(policy);
    RefreshDevicePolicy();
    WaitUntilPolicyChanged();
  }

  void FetchExternalData() {
    const PolicyMap& policies = policy_service_->GetPolicies(
        PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
    const PolicyMap::Entry* policy_entry =
        policies.Get(key::kDeviceWilcoDtcConfiguration);
    EXPECT_TRUE(policy_entry);
    EXPECT_TRUE(policy_entry->external_data_fetcher);

    base::RunLoop run_loop;
    std::unique_ptr<std::string> fetched_external_data;
    base::FilePath file_path;
    policy_entry->external_data_fetcher->Fetch(base::BindOnce(
        [](base::OnceClosure quit_closure, std::string* external_data,
           std::unique_ptr<std::string> data, const base::FilePath& path) {
          *external_data = (data ? *data : "");
          std::move(quit_closure).Run();
        },
        run_loop.QuitClosure(), &external_data_));
    run_loop.Run();
  }

  const std::string& external_data() { return external_data_; }

 private:
  void PolicyChangedCallback(const base::Value* old_value,
                             const base::Value* new_value) {
    policy_change_waiting_run_loop_->Quit();
  }

  void WaitUntilPolicyChanged() {
    policy_change_waiting_run_loop_->Run();
    policy_change_waiting_run_loop_ = std::make_unique<base::RunLoop>();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::RunLoop> policy_change_waiting_run_loop_;
  PolicyService* policy_service_ = nullptr;  // owned by BrowserPolicyConnector.
  std::unique_ptr<PolicyChangeRegistrar> policy_change_registrar_;
  std::string external_data_;
};

// Test that nothing crashes and WilcoDtcConfiguration is successfully passed
// to WilcoDtcSupportdManager if the feature is enabled.
IN_PROC_BROWSER_TEST_P(DeviceWilcoDtcConfigurationExternalPolicyHandlerTest,
                       FetchExternalData) {
  EXPECT_EQ(IsWilcoDtcFeatureEnabled(),
            ash::WilcoDtcSupportdManager::Get() != nullptr);
  SetDeviceWilcoDtcConfigurationExternalData(test::ConstructExternalDataPolicy(
      *embedded_test_server(), "policy/wilco_dtc_configuration.json"));
  FetchExternalData();
  if (IsWilcoDtcFeatureEnabled()) {
    EXPECT_EQ(
        external_data(),
        ash::WilcoDtcSupportdManager::Get()->GetConfigurationDataForTesting());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         DeviceWilcoDtcConfigurationExternalPolicyHandlerTest,
                         testing::Bool());
}  // namespace policy
