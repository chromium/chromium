// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/device_printers_external_data_handler.h"

#include <memory>
#include <string>

#include "base/test/task_environment.h"
#include "chrome/browser/ash/printing/enterprise/bulk_printers_calculator.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// The number of printers in kDevicePrintersContentsJson.
const size_t kNumPrinters = 2;

// An example device printers configuration file.
const char kDevicePrintersContentsJson[] = R"json(
[
  {
    "guid": "First",
    "display_name": "LexaPrint",
    "description": "Laser on the test shelf",
    "uri": "ipp://192.168.1.5",
    "ppd_resource": {
      "effective_model": "MS610de"
    }
  }, {
    "guid": "Second",
    "display_name": "Color Laser",
    "description": "The printer next to the water cooler.",
    "uri":"ipps://print-server.intranet.example.com:443/ipp/cl2k4",
    "ppd_resource":{
      "effective_model": "ColorLaser2k4"
    }
  }
])json";

}  // namespace

class DevicePrintersExternalDataHandlerTest : public testing::Test {
 protected:
  DevicePrintersExternalDataHandlerTest() = default;

  // testing::Test
  void SetUp() override {
    testing::Test::SetUp();
    EXPECT_CALL(policy_service_, AddObserver(POLICY_DOMAIN_CHROME, testing::_))
        .Times(1);
    EXPECT_CALL(policy_service_,
                RemoveObserver(POLICY_DOMAIN_CHROME, testing::_))
        .Times(1);
    external_printers_ = ash::BulkPrintersCalculator::Create();
    device_printers_external_data_handler_ =
        std::make_unique<DevicePrintersExternalDataHandler>(
            &policy_service_, external_printers_->AsWeakPtr());
    external_printers_->SetAccessMode(ash::BulkPrintersCalculator::ALL_ACCESS);
  }

  void TearDown() override {
    device_printers_external_data_handler_->Shutdown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  MockPolicyService policy_service_;
  std::unique_ptr<DevicePrintersExternalDataHandler>
      device_printers_external_data_handler_;
  std::unique_ptr<ash::BulkPrintersCalculator> external_printers_;
};

TEST_F(DevicePrintersExternalDataHandlerTest, OnDataFetched) {
  EXPECT_TRUE(external_printers_->GetPrinters().empty());

  device_printers_external_data_handler_->OnDeviceExternalDataSet(
      key::kDevicePrinters);
  device_printers_external_data_handler_->OnDeviceExternalDataFetched(
      key::kDevicePrinters,
      std::make_unique<std::string>(kDevicePrintersContentsJson),
      base::FilePath());
  task_environment_.RunUntilIdle();

  const auto& printers = external_printers_->GetPrinters();

  // Check that policy was pushed to printers settings.
  EXPECT_EQ(kNumPrinters, printers.size());
  EXPECT_EQ("LexaPrint", printers.at("First").display_name());
  EXPECT_EQ("Color Laser", printers.at("Second").display_name());
}

TEST_F(DevicePrintersExternalDataHandlerTest, OnDataCleared) {
  EXPECT_TRUE(external_printers_->GetPrinters().empty());

  device_printers_external_data_handler_->OnDeviceExternalDataSet(
      key::kDevicePrinters);
  device_printers_external_data_handler_->OnDeviceExternalDataFetched(
      key::kDevicePrinters,
      std::make_unique<std::string>(kDevicePrintersContentsJson),
      base::FilePath());
  device_printers_external_data_handler_->OnDeviceExternalDataCleared(
      key::kDevicePrinters);
  task_environment_.RunUntilIdle();

  // Check that policy was cleared.
  EXPECT_TRUE(external_printers_->GetPrinters().empty());
}

}  // namespace policy
