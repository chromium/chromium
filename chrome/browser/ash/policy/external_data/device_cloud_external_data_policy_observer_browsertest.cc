// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/device_cloud_external_data_policy_observer.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/external_data/cloud_external_data_manager_base_test_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
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
const char* const kPolicyName = key::kDevicePrinters;

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

    BrowserPolicyConnectorAsh* policy_connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    ASSERT_TRUE(policy_connector);
    PolicyService* policy_service = policy_connector->GetPolicyService();
    ASSERT_TRUE(policy_service->IsInitializationComplete(POLICY_DOMAIN_CHROME));

    observer_ = std::make_unique<DeviceCloudExternalDataPolicyObserver>(
        policy_service, kPolicyName, &mock_delegate_);

    policy_change_registrar_ = std::make_unique<PolicyChangeRegistrar>(
        policy_service, PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
    policy_change_registrar_->Observe(
        kPolicyName, policy_changed_repeating_future_.GetRepeatingCallback());
  }

  void TearDownOnMainThread() override {
    observer_.reset();
    policy_change_registrar_.reset();
    DevicePolicyCrosBrowserTest::TearDownOnMainThread();
  }

  void SetDevicePrintersExternalData(const base::Value::Dict& policy_dict) {
    std::string policy;
    EXPECT_TRUE(base::JSONWriter::Write(policy_dict, &policy));
    device_policy()->payload().mutable_device_printers()->set_external_policy(
        policy);
    RefreshDevicePolicy();

    std::tuple<const base::Value*, const base::Value*> prev_curr_policy =
        policy_changed_repeating_future_.Take();
    ASSERT_TRUE(std::get<1>(prev_curr_policy));
    EXPECT_EQ(policy_dict, *std::get<1>(prev_curr_policy));
  }

  void ClearDevicePrintersExternalData() {
    device_policy()->payload().clear_device_printers();
    RefreshDevicePolicy();

    std::tuple<const base::Value*, const base::Value*> prev_curr_policy =
        policy_changed_repeating_future_.Take();
    ASSERT_FALSE(std::get<1>(prev_curr_policy));
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
  std::unique_ptr<DeviceCloudExternalDataPolicyObserver> observer_;
  std::unique_ptr<PolicyChangeRegistrar> policy_change_registrar_;
  base::test::TestFuture<const base::Value*, const base::Value*>
      policy_changed_repeating_future_;
};

IN_PROC_BROWSER_TEST_F(DeviceCloudExternalDataPolicyObserverTest,
                       DataSetAndDataClearedCalled) {
  EXPECT_CALL(mock_delegate_, OnDeviceExternalDataSet(kPolicyName));
  EXPECT_CALL(mock_delegate_, OnDeviceExternalDataCleared(kPolicyName));

  SetDevicePrintersExternalData(test::ConstructExternalDataPolicy(
      *embedded_test_server(), kExternalDataPath));
  ClearDevicePrintersExternalData();
}

IN_PROC_BROWSER_TEST_F(DeviceCloudExternalDataPolicyObserverTest, PolicyIsSet) {
  EXPECT_CALL(mock_delegate_, OnDeviceExternalDataSet(kPolicyName));

  base::test::TestFuture<std::string, std::string> on_data_fetched_future;
  std::string expected_data_file = ReadExternalDataFile(kExternalDataPath);
  EXPECT_CALL(
      mock_delegate_,
      OnDeviceExternalDataFetchedProxy(
          kPolicyName, testing::Pointee(testing::StrEq(expected_data_file)), _))
      .WillOnce(
          testing::Invoke([&on_data_fetched_future](
                              const std::string& policy, std::string* data,
                              const base::FilePath& file_path) {
            ASSERT_TRUE(data);
            on_data_fetched_future.SetValue(policy, std::string(*data));
          }));

  SetDevicePrintersExternalData(test::ConstructExternalDataPolicy(
      *embedded_test_server(), kExternalDataPath));
  EXPECT_EQ(kPolicyName, on_data_fetched_future.Get<0>());
  EXPECT_EQ(expected_data_file, on_data_fetched_future.Get<1>());
}

IN_PROC_BROWSER_TEST_F(DeviceCloudExternalDataPolicyObserverTest,
                       PolicyIsUpdated) {
  EXPECT_CALL(mock_delegate_, OnDeviceExternalDataSet(kPolicyName));

  {
    base::test::TestFuture<std::string, std::string> on_data_fetched_future;
    std::string expected_data_file = ReadExternalDataFile(kExternalDataPath);
    EXPECT_CALL(mock_delegate_,
                OnDeviceExternalDataFetchedProxy(
                    kPolicyName,
                    testing::Pointee(testing::StrEq(expected_data_file)), _))
        .WillOnce(
            testing::Invoke([&on_data_fetched_future](const std::string& policy,
                                                      std::string* data,
                                                      const base::FilePath&) {
              ASSERT_TRUE(data);
              on_data_fetched_future.SetValue(policy, *data);
            }));

    SetDevicePrintersExternalData(test::ConstructExternalDataPolicy(
        *embedded_test_server(), kExternalDataPath));
    EXPECT_EQ(kPolicyName, on_data_fetched_future.Get<0>());
    EXPECT_EQ(expected_data_file, on_data_fetched_future.Get<1>());
  }

  EXPECT_CALL(mock_delegate_, OnDeviceExternalDataSet(kPolicyName));

  {
    base::test::TestFuture<std::string, std::string> on_data_fetched_future;
    std::string expected_data_file =
        ReadExternalDataFile(kExternalDataPathUpdated);
    EXPECT_CALL(mock_delegate_,
                OnDeviceExternalDataFetchedProxy(
                    kPolicyName,
                    testing::Pointee(testing::StrEq(expected_data_file)), _))
        .WillOnce(
            testing::Invoke([&on_data_fetched_future](const std::string& policy,
                                                      std::string* data,
                                                      const base::FilePath&) {
              ASSERT_TRUE(data);
              on_data_fetched_future.SetValue(policy, *data);
            }));

    SetDevicePrintersExternalData(test::ConstructExternalDataPolicy(
        *embedded_test_server(), kExternalDataPathUpdated));
    EXPECT_EQ(kPolicyName, on_data_fetched_future.Get<0>());
    EXPECT_EQ(expected_data_file, on_data_fetched_future.Get<1>());
  }
}

}  // namespace policy
