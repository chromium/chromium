// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/on_device_controls/on_device_utils.h"

#include <string>

#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::on_device_controls {

class OnDeviceUtilsTest : public testing::Test {
 public:
  OnDeviceUtilsTest() = default;

  OnDeviceUtilsTest(const OnDeviceUtilsTest&) = delete;
  OnDeviceUtilsTest& operator=(const OnDeviceUtilsTest&) = delete;

  ~OnDeviceUtilsTest() override = default;

 protected:
  // Sets device region in VPD.
  void SetDeviceRegion(const std::string& region) {
    statistics_provider_.SetMachineStatistic(ash::system::kRegionKey, region);
  }

  void ClearDeviceRegion() {
    statistics_provider_.ClearMachineStatistic(ash::system::kRegionKey);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
};

TEST_F(OnDeviceUtilsTest, DeviceRegionSimple) {
  SetDeviceRegion("us");
  EXPECT_EQ("US", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("ca");
  EXPECT_EQ("CA", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("es");
  EXPECT_EQ("ES", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();
}

TEST_F(OnDeviceUtilsTest, DeviceRegionCaseSensitivity) {
  SetDeviceRegion("US");
  EXPECT_EQ("US", GetDeviceRegionCode());
  ClearDeviceRegion();

  SetDeviceRegion("uS");
  EXPECT_EQ("US", GetDeviceRegionCode());
  ClearDeviceRegion();

  SetDeviceRegion("Us");
  EXPECT_EQ("US", GetDeviceRegionCode());
  ClearDeviceRegion();

  SetDeviceRegion("us.INTL");
  EXPECT_EQ("US", GetDeviceRegionCode());
  ClearDeviceRegion();
}

TEST_F(OnDeviceUtilsTest, DeviceRegionWithVariant) {
  SetDeviceRegion("ca.fr");
  EXPECT_EQ("CA", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("ca.ansi");
  EXPECT_EQ("CA", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("ca.hybridansi");
  EXPECT_EQ("CA", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("ca.hybrid");
  EXPECT_EQ("CA", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("ca.multix");
  EXPECT_EQ("CA", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();
}

TEST_F(OnDeviceUtilsTest, DeviceRegionVirtual) {
  SetDeviceRegion("latam-es-419");
  EXPECT_EQ("", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("nordic");
  EXPECT_EQ("", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("gcc");
  EXPECT_EQ("", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();
}

TEST_F(OnDeviceUtilsTest, DeviceRegionInvalid) {
  SetDeviceRegion("");
  EXPECT_EQ("", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("x");
  EXPECT_EQ("", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion("x"));
  ClearDeviceRegion();

  SetDeviceRegion("x.y");
  EXPECT_EQ("", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion("x.y"));
  ClearDeviceRegion();

  SetDeviceRegion("xyz");
  EXPECT_EQ("", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion("xyz"));
  ClearDeviceRegion();

  SetDeviceRegion("123");
  EXPECT_EQ("", GetDeviceRegionCode());
  EXPECT_FALSE(IsOnDeviceControlsRegion("123"));
  ClearDeviceRegion();
}

TEST_F(OnDeviceUtilsTest, DeviceRegionWithOnDeviceControls) {
  SetDeviceRegion("fr");
  EXPECT_EQ("FR", GetDeviceRegionCode());
  EXPECT_TRUE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("gf");
  EXPECT_EQ("GF", GetDeviceRegionCode());
  EXPECT_TRUE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("gp");
  EXPECT_EQ("GP", GetDeviceRegionCode());
  EXPECT_TRUE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("mf");
  EXPECT_EQ("MF", GetDeviceRegionCode());
  EXPECT_TRUE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("mq");
  EXPECT_EQ("MQ", GetDeviceRegionCode());
  EXPECT_TRUE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("re");
  EXPECT_EQ("RE", GetDeviceRegionCode());
  EXPECT_TRUE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("yt");
  EXPECT_EQ("YT", GetDeviceRegionCode());
  EXPECT_TRUE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("bl");
  EXPECT_EQ("BL", GetDeviceRegionCode());
  EXPECT_TRUE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("nc");
  EXPECT_EQ("NC", GetDeviceRegionCode());
  EXPECT_TRUE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("pf");
  EXPECT_EQ("PF", GetDeviceRegionCode());
  EXPECT_TRUE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("pm");
  EXPECT_EQ("PM", GetDeviceRegionCode());
  EXPECT_TRUE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("tf");
  EXPECT_EQ("TF", GetDeviceRegionCode());
  EXPECT_TRUE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();

  SetDeviceRegion("wf");
  EXPECT_EQ("WF", GetDeviceRegionCode());
  EXPECT_TRUE(IsOnDeviceControlsRegion(GetDeviceRegionCode()));
  ClearDeviceRegion();
}

}  // namespace ash::on_device_controls
