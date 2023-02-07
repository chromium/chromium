// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/geolocation_service_ash.h"

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/shill/shill_clients.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/network/geolocation_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace crosapi {

class GeolocationServiceAshTest : public testing::Test {
 public:
  GeolocationServiceAshTest()
      : network_handler_test_helper_(
            std::make_unique<ash::NetworkHandlerTestHelper>()) {}

  void AddAccessPoints(int ssids, int aps_per_ssid) {
    for (int i = 0; i < ssids; ++i) {
      for (int j = 0; j < aps_per_ssid; ++j) {
        base::Value::Dict properties;
        std::string mac_address = base::StringPrintf(
            "%02X:%02X:%02X:%02X:%02X:%02X", i, j, 3, 4, 5, 6);
        std::string channel = base::NumberToString(i * 10 + j);
        std::string strength = base::NumberToString(i * 100 + j);
        properties.Set(shill::kGeoMacAddressProperty, base::Value(mac_address));
        properties.Set(shill::kGeoChannelProperty, base::Value(channel));
        properties.Set(shill::kGeoSignalStrengthProperty,
                       base::Value(strength));
        network_handler_test_helper_->manager_test()->AddGeoNetwork(
            shill::kGeoWifiAccessPointsProperty, std::move(properties));
      }
    }
    base::RunLoop().RunUntilIdle();
  }

  void DoWifiScanTask(
      GeolocationServiceAsh::GetWifiAccessPointsCallback callback) {
    download_controller_ash_.GetWifiAccessPoints(std::move(callback));
  }

  void reset_network_handler_test_helper() {
    network_handler_test_helper_.reset();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ash::NetworkHandlerTestHelper> network_handler_test_helper_;
  GeolocationServiceAsh download_controller_ash_;
};

TEST_F(GeolocationServiceAshTest, NotifiesWhenServiceDisabled) {
  reset_network_handler_test_helper();
  base::RunLoop().RunUntilIdle();

  // The service should notify clients when the NetworkHandler is disabled.
  const auto callback =
      [](bool service_initialized, bool data_available,
         base::TimeDelta time_since_last_updated,
         std::vector<crosapi::mojom::AccessPointDataPtr> access_points) {
        EXPECT_FALSE(service_initialized);
        EXPECT_FALSE(data_available);
        EXPECT_TRUE(access_points.empty());
      };
  DoWifiScanTask(base::BindOnce(callback));
}

TEST_F(GeolocationServiceAshTest, NotifiesWhenNoAccessPointsAvailable) {
  base::RunLoop().RunUntilIdle();

  // The service should notify clients that there are no access points
  // available.
  const auto callback =
      [](bool service_initialized, bool data_available,
         base::TimeDelta time_since_last_updated,
         std::vector<crosapi::mojom::AccessPointDataPtr> access_points) {
        EXPECT_TRUE(service_initialized);
        EXPECT_FALSE(data_available);
        EXPECT_TRUE(access_points.empty());
      };
  DoWifiScanTask(base::BindOnce(callback));
}

TEST_F(GeolocationServiceAshTest, CorrectlyReturnsSingleAccessPoint) {
  base::RunLoop().RunUntilIdle();

  // The service should notify clients that there are no access points
  // available.
  const auto initial_callback =
      [](bool service_initialized, bool data_available,
         base::TimeDelta time_since_last_updated,
         std::vector<crosapi::mojom::AccessPointDataPtr> access_points) {
        EXPECT_TRUE(service_initialized);
        EXPECT_FALSE(data_available);
        EXPECT_TRUE(access_points.empty());
      };
  DoWifiScanTask(base::BindOnce(initial_callback));

  // The service should notify clients when data is available, providing the
  // correct access point data.
  const auto one_ap_callback =
      [](bool service_initialized, bool data_available,
         base::TimeDelta time_since_last_updated,
         std::vector<crosapi::mojom::AccessPointDataPtr> access_points) {
        EXPECT_TRUE(service_initialized);
        EXPECT_TRUE(data_available);
        EXPECT_EQ(1u, access_points.size());
        EXPECT_EQ("00:00:03:04:05:06",
                  base::UTF16ToUTF8(access_points.front()->mac_address));
      };
  AddAccessPoints(1, 1);
  DoWifiScanTask(base::BindOnce(one_ap_callback));
}

TEST_F(GeolocationServiceAshTest, CorrectlyReturnsMultipleAccessPoints) {
  base::RunLoop().RunUntilIdle();

  // The service should notify clients that there are no access points
  // available.
  const auto initial_callback =
      [](bool service_initialized, bool data_available,
         base::TimeDelta time_since_last_updated,
         std::vector<crosapi::mojom::AccessPointDataPtr> access_points) {
        EXPECT_TRUE(service_initialized);
        EXPECT_FALSE(data_available);
        EXPECT_TRUE(access_points.empty());
      };
  DoWifiScanTask(base::BindOnce(initial_callback));

  // The service should notify clients when data is available, providing the
  // correct access point data.
  const auto multiple_ap_callback =
      [](bool service_initialized, bool data_available,
         base::TimeDelta time_since_last_updated,
         std::vector<crosapi::mojom::AccessPointDataPtr> access_points) {
        EXPECT_TRUE(service_initialized);
        EXPECT_TRUE(data_available);
        EXPECT_EQ(12u, access_points.size());
        EXPECT_EQ("00:00:03:04:05:06",
                  base::UTF16ToUTF8(access_points.front()->mac_address));
        EXPECT_EQ("02:03:03:04:05:06",
                  base::UTF16ToUTF8(access_points.back()->mac_address));
      };
  AddAccessPoints(3, 4);
  DoWifiScanTask(base::BindOnce(multiple_ap_callback));
}

}  // namespace crosapi
