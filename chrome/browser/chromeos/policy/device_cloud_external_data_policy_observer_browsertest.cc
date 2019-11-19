// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_external_data_policy_observer.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace policy {

namespace {

const char kExternalDataPath[] = "policy/printers_configuration.json";
const char kExternalDataPathUpdated[] =
    "policy/printers_configuration_updated.json";

// The name of an External Data Policy in Device Policy.
const char* const kPolicyName = key::kDeviceNativePrinters;

class MockDeviceCloudExternalDataPolicyObserverDelegate
    : public DeviceCloudExternalDataPolicyObserver::Delegate {
 public:
  MockDeviceCloudExternalDataPolicyObserverDelegate() {}

  void OnDeviceExternalDataFetched(const std::string& policy,
                                   std::unique_ptr<std::string> data,
                                   const base::FilePath& file_path) override {
    OnDeviceExternalDataFetchedProxy(policy, data.get(), file_path);
  }

  MOCK_METHOD1(OnDeviceExternalDataSet, void(const std::string&));
  MOCK_METHOD1(OnDeviceExternalDataCleared, void(const std::string&));
  MOCK_METHOD3(OnDeviceExternalDataFetchedProxy,
               void(const std::string&, std::string*, const base::FilePath&));
};

}  // namespace

class DeviceCloudExternalDataPolicyObserverTest
    : public DevicePolicyCrosBrowserTest {
 public:
  DeviceCloudExternalDataPolicyObserverTest() {}

 protected:
  void SetUpOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    DevicePolicyCrosBrowserTest::SetUpOnMainThread();

    BrowserPolicyConnectorChromeOS* policy_connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    ASSERT_TRUE(policy_connector);
    PolicyService* policy_service = policy_connector->GetPolicyService();
    ASSERT_TRUE(policy_service->IsInitializationComplete(POLICY_DOMAIN_CHROME));

    observer_ = std::make_unique<DeviceCloudExternalDataPolicyObserver>(
        policy_service, kPolicyName, &mock_delegate_);

    policy_change_registrar_ = std::make_unique<PolicyChangeRegistrar>(
        policy_service, PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
    policy_change_registrar_->Observe(
        kPolicyName,
        base::BindRepeating(
            &DeviceCloudExternalDataPolicyObserverTest::PolicyChangedCallback,
            base::Unretained(this)));

    policy_change_waiting_run_loop_ = std::make_unique<base::RunLoop>();
  }

  void TearDownOnMainThread() override {
    observer_.reset();
    policy_change_registrar_.reset();
    DevicePolicyCrosBrowserTest::TearDownOnMainThread();
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

  MockDeviceCloudExternalDataPolicyObserverDelegate mock_delegate_;

 private:
  void PolicyChangedCallback(const base::Value* old_value,
                             const base::Value* new_value) {
    policy_change_waiting_run_loop_->Quit();
  }

  void WaitUntilPolicyChanged() {
    policy_change_waiting_run_loop_->Run();
    policy_change_waiting_run_loop_.reset(new base::RunLoop());
  }

  std::unique_ptr<DeviceCloudExternalDataPolicyObserver> observer_;
  std::unique_ptr<PolicyChangeRegistrar> policy_change_registrar_;
  std::unique_ptr<base::RunLoop> policy_change_waiting_run_loop_;
};

IN_PROC_BROWSER_TEST_F(DeviceCloudExternalDataPolicyObserverTest,
                       DataSetAndDataClearedCalled) {
  EXPECT_CALL(mock_delegate_, OnDeviceExternalDataSet(kPolicyName));
  EXPECT_CALL(mock_delegate_, OnDeviceExternalDataCleared(kPolicyName));

  SetDeviceNativePrintersExternalData(test::ConstructExternalDataPolicy(
      *embedded_test_server(), kExternalDataPath));
  ClearDeviceNativePrintersExternalData();
}

IN_PROC_BROWSER_TEST_F(DeviceCloudExternalDataPolicyObserverTest, PolicyIsSet) {
  EXPECT_CALL(mock_delegate_, OnDeviceExternalDataSet(kPolicyName));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, OnDeviceExternalDataFetchedProxy(
                                  kPolicyName,
                                  testing::Pointee(testing::StrEq(
                                      ReadExternalDataFile(kExternalDataPath))),
                                  _))
      .WillOnce(testing::Invoke(
          [&run_loop](const std::string&, std::string*, const base::FilePath&) {
            run_loop.Quit();
          }));

  SetDeviceNativePrintersExternalData(test::ConstructExternalDataPolicy(
      *embedded_test_server(), kExternalDataPath));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(DeviceCloudExternalDataPolicyObserverTest,
                       PolicyIsUpdated) {
  EXPECT_CALL(mock_delegate_, OnDeviceExternalDataSet(kPolicyName));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_delegate_, OnDeviceExternalDataFetchedProxy(
                                  kPolicyName,
                                  testing::Pointee(testing::StrEq(
                                      ReadExternalDataFile(kExternalDataPath))),
                                  _))
      .WillOnce(testing::Invoke(
          [&run_loop](const std::string&, std::string*, const base::FilePath&) {
            run_loop.Quit();
          }));

  SetDeviceNativePrintersExternalData(test::ConstructExternalDataPolicy(
      *embedded_test_server(), kExternalDataPath));
  run_loop.Run();

  EXPECT_CALL(mock_delegate_, OnDeviceExternalDataSet(kPolicyName));

  base::RunLoop run_loop_updated;
  EXPECT_CALL(mock_delegate_,
              OnDeviceExternalDataFetchedProxy(
                  kPolicyName,
                  testing::Pointee(testing::StrEq(
                      ReadExternalDataFile(kExternalDataPathUpdated))),
                  _))
      .WillOnce(
          testing::Invoke([&run_loop_updated](const std::string&, std::string*,
                                              const base::FilePath&) {
            run_loop_updated.Quit();
          }));

  SetDeviceNativePrintersExternalData(test::ConstructExternalDataPolicy(
      *embedded_test_server(), kExternalDataPathUpdated));
  run_loop_updated.Run();
}

}  // namespace policy
