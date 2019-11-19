// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/external_data_handlers/device_native_printers_external_data_handler.h"

#include <memory>
#include <string>

#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator_factory.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// The number of printers in kDeviceNativePrintersContentsJson.
const size_t kNumPrinters = 2;

// An example device native printers configuration file.
const char kDeviceNativePrintersContentsJson[] = R"json(
[
  {
    "id": "First",
    "display_name": "LexaPrint",
    "description": "Laser on the test shelf",
    "manufacturer": "LexaPrint, Inc.",
    "model": "MS610de",
    "uri": "ipp://192.168.1.5",
    "ppd_resource": {
      "effective_model": "MS610de"
    }
  }, {
    "id": "Second",
    "display_name": "Color Laser",
    "description": "The printer next to the water cooler.",
    "manufacturer": "Printer Manufacturer",
    "model":"Color Laser 2004",
    "uri":"ipps://print-server.intranet.example.com:443/ipp/cl2k4",
    "uuid":"1c395fdb-5d93-4904-b246-b2c046e79d12",
    "ppd_resource":{
      "effective_manufacturer": "MakesPrinters",
      "effective_model": "ColorLaser2k4"
    }
  }
])json";

}  // namespace

class DeviceNativePrintersExternalDataHandlerTest : public testing::Test {
 protected:
  DeviceNativePrintersExternalDataHandlerTest() {}

  // testing::Test
  void SetUp() override {
    testing::Test::SetUp();
    EXPECT_CALL(policy_service_,
                AddObserver(policy::POLICY_DOMAIN_CHROME, testing::_))
        .Times(1);
    EXPECT_CALL(policy_service_,
                RemoveObserver(policy::POLICY_DOMAIN_CHROME, testing::_))
        .Times(1);
    device_native_printers_external_data_handler_ =
        std::make_unique<DeviceNativePrintersExternalDataHandler>(
            &policy_service_);
    external_printers_ =
        chromeos::BulkPrintersCalculatorFactory::Get()->GetForDevice();
    external_printers_->SetAccessMode(
        chromeos::BulkPrintersCalculator::ALL_ACCESS);
  }

  void TearDown() override {
    device_native_printers_external_data_handler_->Shutdown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  MockPolicyService policy_service_;
  std::unique_ptr<DeviceNativePrintersExternalDataHandler>
      device_native_printers_external_data_handler_;
  base::WeakPtr<chromeos::BulkPrintersCalculator> external_printers_;
};

TEST_F(DeviceNativePrintersExternalDataHandlerTest, OnDataFetched) {
  EXPECT_TRUE(external_printers_->GetPrinters().empty());

  device_native_printers_external_data_handler_->OnDeviceExternalDataSet(
      key::kDeviceNativePrinters);
  device_native_printers_external_data_handler_->OnDeviceExternalDataFetched(
      key::kDeviceNativePrinters,
      std::make_unique<std::string>(kDeviceNativePrintersContentsJson),
      base::FilePath());
  task_environment_.RunUntilIdle();

  const auto& printers = external_printers_->GetPrinters();

  // Check that policy was pushed to printers settings.
  EXPECT_EQ(kNumPrinters, printers.size());
  EXPECT_EQ("LexaPrint", printers.at("First").display_name());
  EXPECT_EQ("Color Laser", printers.at("Second").display_name());
}

TEST_F(DeviceNativePrintersExternalDataHandlerTest, OnDataCleared) {
  EXPECT_TRUE(external_printers_->GetPrinters().empty());

  device_native_printers_external_data_handler_->OnDeviceExternalDataSet(
      key::kDeviceNativePrinters);
  device_native_printers_external_data_handler_->OnDeviceExternalDataFetched(
      key::kDeviceNativePrinters,
      std::make_unique<std::string>(kDeviceNativePrintersContentsJson),
      base::FilePath());
  device_native_printers_external_data_handler_->OnDeviceExternalDataCleared(
      key::kDeviceNativePrinters);
  task_environment_.RunUntilIdle();

  // Check that policy was cleared.
  EXPECT_TRUE(external_printers_->GetPrinters().empty());
}

}  // namespace policy
