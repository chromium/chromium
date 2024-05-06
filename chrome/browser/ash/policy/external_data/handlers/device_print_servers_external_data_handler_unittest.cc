// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/device_print_servers_external_data_handler.h"

#include <memory>
#include <string>

#include "chrome/browser/ash/printing/enterprise/print_servers_provider.h"
#include "chrome/browser/ash/printing/enterprise/print_servers_provider_factory.h"
#include "chrome/browser/ash/printing/print_server.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

// An example device native printers configuration file.
constexpr char kDeviceExternalPrintServersContentsJson[] = R"json(
[
  {
    "id": "First",
    "display_name": "LexaPrint",
    "url": "ipp://192.168.1.5",
  }, {
    "id": "Second",
    "display_name": "Color Laser",
    "url":"ipps://print-server.intranet.example.com:443/ipp/cl2k4",
  }
])json";

constexpr char kAllowlistPrefName[] = "test";

class TestObserver : public ash::PrintServersProvider::Observer {
 public:
  ~TestObserver() override = default;

  // Callback from PrintServersProvider::Observer.
  void OnServersChanged(bool complete,
                        const std::vector<ash::PrintServer>& servers) override {
    print_servers_ = servers;
  }

  const std::vector<ash::PrintServer>* GetPrintServers() {
    return &print_servers_;
  }

 private:
  std::vector<ash::PrintServer> print_servers_;
};

}  // namespace

class DevicePrintServersExternalDataHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(policy_service_, AddObserver(POLICY_DOMAIN_CHROME, testing::_))
        .Times(1);
    EXPECT_CALL(policy_service_,
                RemoveObserver(POLICY_DOMAIN_CHROME, testing::_))
        .Times(1);

    print_servers_provider_ =
        ash::PrintServersProviderFactory::Get()->GetForDevice();
    pref_service_.registry()->RegisterListPref(kAllowlistPrefName);
    print_servers_provider_->SetAllowlistPref(&pref_service_,
                                              kAllowlistPrefName);
    device_print_servers_external_data_handler_ =
        std::make_unique<DevicePrintServersExternalDataHandler>(
            &policy_service_);
  }

  void TearDown() override {
    ash::PrintServersProviderFactory::Get()->Shutdown();
    device_print_servers_external_data_handler_->Shutdown();
  }

  // Everything must be called on Chrome_UIThread.
  content::BrowserTaskEnvironment task_environment_;

  sync_preferences::TestingPrefServiceSyncable pref_service_;

  MockPolicyService policy_service_;
  std::unique_ptr<DevicePrintServersExternalDataHandler>
      device_print_servers_external_data_handler_;
  base::WeakPtr<ash::PrintServersProvider> print_servers_provider_;
};

TEST_F(DevicePrintServersExternalDataHandlerTest, OnDataFetched) {
  TestObserver obs;
  print_servers_provider_->AddObserver(&obs);
  EXPECT_TRUE(obs.GetPrintServers()->empty());

  device_print_servers_external_data_handler_->OnDeviceExternalDataSet(
      key::kDeviceExternalPrintServers);
  device_print_servers_external_data_handler_->OnDeviceExternalDataFetched(
      key::kDeviceExternalPrintServers,
      std::make_unique<std::string>(kDeviceExternalPrintServersContentsJson),
      base::FilePath());
  task_environment_.RunUntilIdle();

  ash::PrintServer first("First", GURL("http://192.168.1.5:631"), "LexaPrint");
  ash::PrintServer second(
      "Second", GURL("https://print-server.intranet.example.com:443/ipp/cl2k4"),
      "Color Laser");

  EXPECT_THAT(obs.GetPrintServers(),
              Pointee(UnorderedElementsAre(first, second)));
}

TEST_F(DevicePrintServersExternalDataHandlerTest, OnDataCleared) {
  TestObserver obs;
  print_servers_provider_->AddObserver(&obs);
  EXPECT_TRUE(obs.GetPrintServers()->empty());

  device_print_servers_external_data_handler_->OnDeviceExternalDataSet(
      key::kDeviceExternalPrintServers);
  device_print_servers_external_data_handler_->OnDeviceExternalDataFetched(
      key::kDeviceExternalPrintServers,
      std::make_unique<std::string>(kDeviceExternalPrintServersContentsJson),
      base::FilePath());
  device_print_servers_external_data_handler_->OnDeviceExternalDataCleared(
      key::kDeviceExternalPrintServers);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(obs.GetPrintServers()->empty());
}

}  // namespace policy
