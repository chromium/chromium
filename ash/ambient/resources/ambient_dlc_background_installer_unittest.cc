// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/resources/ambient_dlc_background_installer.h"

#include <string>

#include "ash/constants/ambient_time_of_day_constants.h"
#include "ash/public/cpp/personalization_app/time_of_day_test_utils.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace {

class AmbientDlcBackgroundInstallerTest : public ::testing::Test {
 protected:
  AmbientDlcBackgroundInstallerTest() {
    feature_list_.InitWithFeatures(
        personalization_app::GetTimeOfDayEnabledFeatures(), {});
    wifi_device_path_ =
        cros_network_config_helper_.network_state_helper().ConfigureWiFi(
            shill::kStateIdle);
  }

  size_t GetNumTimeOfDayInstallRequests() {
    base::test::TestFuture<std::string_view, const dlcservice::DlcsWithContent&>
        future;
    dlc_service_client_.GetExistingDlcs(future.GetCallback());
    size_t num_installs = 0;
    for (const auto& dlc_info : future.Get<1>().dlc_infos()) {
      if (dlc_info.id() == kTimeOfDayDlcId) {
        ++num_installs;
      }
    }
    return num_installs;
  }

  bool IsTimeOfDayDlcInstalled() {
    return GetNumTimeOfDayInstallRequests() >= 1u;
  }

  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_helper_;
  FakeDlcserviceClient dlc_service_client_;
  std::string wifi_device_path_;
};

TEST_F(AmbientDlcBackgroundInstallerTest, InstallsWhenNetworkUp) {
  AmbientBackgroundDlcInstaller installer;
  ASSERT_FALSE(IsTimeOfDayDlcInstalled());

  cros_network_config_helper_.network_state_helper().SetServiceProperty(
      wifi_device_path_, shill::kStateProperty,
      base::Value(shill::kStateOnline));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(IsTimeOfDayDlcInstalled());
}

TEST_F(AmbientDlcBackgroundInstallerTest, InstallsWhenNetworkAlreadyUp) {
  cros_network_config_helper_.network_state_helper().SetServiceProperty(
      wifi_device_path_, shill::kStateProperty,
      base::Value(shill::kStateOnline));

  AmbientBackgroundDlcInstaller installer;
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(IsTimeOfDayDlcInstalled());
}

TEST_F(AmbientDlcBackgroundInstallerTest, InstallsOnlyOnce) {
  AmbientBackgroundDlcInstaller installer;
  ASSERT_FALSE(IsTimeOfDayDlcInstalled());

  cros_network_config_helper_.network_state_helper().SetServiceProperty(
      wifi_device_path_, shill::kStateProperty,
      base::Value(shill::kStateOnline));
  task_environment_.RunUntilIdle();

  cros_network_config_helper_.network_state_helper().SetServiceProperty(
      wifi_device_path_, shill::kStateProperty, base::Value(shill::kStateIdle));
  task_environment_.RunUntilIdle();

  cros_network_config_helper_.network_state_helper().SetServiceProperty(
      wifi_device_path_, shill::kStateProperty,
      base::Value(shill::kStateOnline));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(GetNumTimeOfDayInstallRequests(), 1u);
}

}  // namespace
}  // namespace ash
