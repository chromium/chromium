// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "chrome/browser/chromeos/net/wake_on_wifi_connection_observer.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/network/mock_network_device_handler.h"
#include "components/gcm_driver/fake_gcm_profile_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::StrictMock;

namespace chromeos {
namespace {

std::unique_ptr<KeyedService> BuildFakeGCMProfileService(
    content::BrowserContext* context) {
  return gcm::FakeGCMProfileService::Build(static_cast<Profile*>(context));
}

class WakeOnWifiObserverTest : public ::testing::Test {
 public:
  WakeOnWifiObserverTest() {
    gcm::GCMProfileServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildFakeGCMProfileService));
  }
  ~WakeOnWifiObserverTest() override {}

 private:
  // Must outlive |profile_|.
  content::BrowserTaskEnvironment task_environment_;

 protected:
  StrictMock<MockNetworkDeviceHandler> mock_network_device_handler_;
  TestingProfile profile_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WakeOnWifiObserverTest);
};

}  // namespace

TEST_F(WakeOnWifiObserverTest, TestWakeOnWifiPacketAdd) {
  WakeOnWifiConnectionObserver observer(
      &profile_, false, WakeOnWifiManager::WAKE_ON_WIFI_PACKET,
      &mock_network_device_handler_);

  EXPECT_CALL(mock_network_device_handler_,
              AddWifiWakeOnPacketConnection(_, _, _))
      .Times(1);

  observer.AddWakeOnPacketConnection();
}

TEST_F(WakeOnWifiObserverTest, TestWakeOnWifiPacketRemove) {
  WakeOnWifiConnectionObserver observer(
      &profile_, false, WakeOnWifiManager::WAKE_ON_WIFI_PACKET,
      &mock_network_device_handler_);

  EXPECT_CALL(mock_network_device_handler_,
              RemoveWifiWakeOnPacketConnection(_, _, _))
      .Times(1);

  observer.RemoveWakeOnPacketConnection();
}

TEST_F(WakeOnWifiObserverTest, TestWakeOnWifiNoneAdd) {
  WakeOnWifiConnectionObserver observer(
      &profile_, false, WakeOnWifiManager::WAKE_ON_WIFI_NONE,
      &mock_network_device_handler_);

  EXPECT_CALL(mock_network_device_handler_,
              AddWifiWakeOnPacketConnection(_, _, _))
      .Times(0);

  observer.AddWakeOnPacketConnection();
}

TEST_F(WakeOnWifiObserverTest, TestWakeOnWifiNoneRemove) {
  WakeOnWifiConnectionObserver observer(
      &profile_, false, WakeOnWifiManager::WAKE_ON_WIFI_NONE,
      &mock_network_device_handler_);

  EXPECT_CALL(mock_network_device_handler_,
              RemoveWifiWakeOnPacketConnection(_, _, _))
      .Times(0);

  observer.RemoveWakeOnPacketConnection();
}

}  // namespace chromeos
