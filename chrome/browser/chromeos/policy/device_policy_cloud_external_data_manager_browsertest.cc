// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cloud_external_data_manager.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_paths.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// The contents of these files are served as external data.
const char kExternalDataPath[] = "policy/printers_configuration.json";
const char kExternalDataPathUpdated[] =
    "policy/printers_configuration_updated.json";
const char kExternalDataPathOverSizeLimit[] =
    "policy/printers_configuration_over_size_limit.json";
// The name of an External Data Policy in Device Policy.
const char* const kPolicyName = policy::key::kDeviceNativePrinters;

const int64_t kTestCacheMaxSize = 64;

}  // namespace

class DevicePolicyCloudExternalDataManagerTest
    : public DevicePolicyCrosBrowserTest {
 public:
  DevicePolicyCloudExternalDataManagerTest() {
    DevicePolicyCloudExternalDataManager::SetCacheMaxSizeForTesting(
        kTestCacheMaxSize);
  }
  ~DevicePolicyCloudExternalDataManagerTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    DevicePolicyCrosBrowserTest::SetUpOnMainThread();

    BrowserPolicyConnectorChromeOS* policy_connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    ASSERT_TRUE(policy_connector);
    policy_service_ = policy_connector->GetPolicyService();
    ASSERT_TRUE(
        policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME));
    policy_change_registrar_ = std::make_unique<PolicyChangeRegistrar>(
        policy_service_, PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
    policy_change_registrar_->Observe(
        kPolicyName,
        base::BindRepeating(
            &DevicePolicyCloudExternalDataManagerTest::PolicyChangedCallback,
            base::Unretained(this)));

    policy_change_waiting_run_loop_ = std::make_unique<base::RunLoop>();
  }

  void TearDownOnMainThread() override {
    policy_change_registrar_.reset();
    DevicePolicyCrosBrowserTest::TearDownOnMainThread();
  }

  std::string GetExternalData() {
    const PolicyMap& policies = policy_service_->GetPolicies(
        PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
    const PolicyMap::Entry* policy_entry = policies.Get(kPolicyName);
    EXPECT_TRUE(policy_entry);
    EXPECT_TRUE(policy_entry->external_data_fetcher);

    base::RunLoop run_loop;
    std::unique_ptr<std::string> fetched_external_data;
    base::FilePath file_path;
    policy_entry->external_data_fetcher->Fetch(
        base::BindOnce(&test::ExternalDataFetchCallback, &fetched_external_data,
                       &file_path, run_loop.QuitClosure()));
    run_loop.Run();

    EXPECT_TRUE(fetched_external_data);
    return *fetched_external_data;
  }

  int64_t ComputeExternalDataCacheDirectorySize() {
    base::FilePath device_policy_external_data_path;
    CHECK(base::PathService::Get(chromeos::DIR_DEVICE_POLICY_EXTERNAL_DATA,
                                 &device_policy_external_data_path));
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::ComputeDirectorySize(device_policy_external_data_path);
  }

  void SetDeviceNativePrintersExternalData(const std::string& policy) {
    device_policy()
        ->payload()
        .mutable_native_device_printers()
        ->set_external_policy(policy);
    RefreshDevicePolicy();
    WaitUntilPolicyChanged();
  }

  void ClearDeviceNativePrintersExternalData() {
    device_policy()->payload().clear_native_device_printers();
    RefreshDevicePolicy();
    WaitUntilPolicyChanged();
  }

  std::string ReadExternalDataFile(const std::string& file_path) {
    base::FilePath test_data_dir;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    std::string external_data;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::ReadFileToString(test_data_dir.AppendASCII(file_path),
                                         &external_data));
    }
    return external_data;
  }

 private:
  void PolicyChangedCallback(const base::Value* old_value,
                             const base::Value* new_value) {
    policy_change_waiting_run_loop_->Quit();
  }

  void WaitUntilPolicyChanged() {
    policy_change_waiting_run_loop_->Run();
    policy_change_waiting_run_loop_.reset(new base::RunLoop());
  }

  PolicyService* policy_service_ = nullptr;
  std::unique_ptr<PolicyChangeRegistrar> policy_change_registrar_;
  std::unique_ptr<base::RunLoop> policy_change_waiting_run_loop_;
};

IN_PROC_BROWSER_TEST_F(DevicePolicyCloudExternalDataManagerTest,
                       FetchExternalData) {
  SetDeviceNativePrintersExternalData(test::ConstructExternalDataPolicy(
      *embedded_test_server(), kExternalDataPath));
  EXPECT_EQ(ReadExternalDataFile(kExternalDataPath), GetExternalData());
}

IN_PROC_BROWSER_TEST_F(DevicePolicyCloudExternalDataManagerTest,
                       FetchOverSizeLimitExternalData) {
  EXPECT_EQ(0, ComputeExternalDataCacheDirectorySize());

  std::string external_data =
      ReadExternalDataFile(kExternalDataPathOverSizeLimit);
  // Check that file size is greater than cache limit.
  ASSERT_GT(base::checked_cast<int64_t>(external_data.size()),
            kTestCacheMaxSize);
  SetDeviceNativePrintersExternalData(test::ConstructExternalDataPolicy(
      *embedded_test_server(), kExternalDataPathOverSizeLimit));
  EXPECT_EQ(external_data, GetExternalData());

  // Check that nothing is cached because file was too big.
  EXPECT_EQ(0, ComputeExternalDataCacheDirectorySize());
}

IN_PROC_BROWSER_TEST_F(DevicePolicyCloudExternalDataManagerTest,
                       CleanUpResourceCache) {
  EXPECT_EQ(0, ComputeExternalDataCacheDirectorySize());

  std::string external_data = ReadExternalDataFile(kExternalDataPath);
  SetDeviceNativePrintersExternalData(test::ConstructExternalDataPolicy(
      *embedded_test_server(), kExternalDataPath));
  EXPECT_EQ(external_data, GetExternalData());
  EXPECT_EQ(base::checked_cast<int64_t>(external_data.size()),
            ComputeExternalDataCacheDirectorySize());

  external_data = ReadExternalDataFile(kExternalDataPathUpdated);
  SetDeviceNativePrintersExternalData(test::ConstructExternalDataPolicy(
      *embedded_test_server(), kExternalDataPathUpdated));
  EXPECT_EQ(external_data, GetExternalData());
  // Check that previous policy data was cleared and replaced by new one.
  EXPECT_EQ(base::checked_cast<int64_t>(external_data.size()),
            ComputeExternalDataCacheDirectorySize());

  ClearDeviceNativePrintersExternalData();
  // We have to wait until
  // CloudExternalDataManagerBase::Backend::OnMetadataUpdated(), which is
  // responsible for removing outdated external policy files, is completed.
  content::RunAllTasksUntilIdle();
  // Check that policy data was cleared.
  EXPECT_EQ(0, ComputeExternalDataCacheDirectorySize());
}

}  // namespace policy
