// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/volume_mounter/arc_volume_mounter_bridge.h"

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/test_browser_context.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/mock_disk_mount_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcVolumeMounterBridgeTest : public testing::Test {
 protected:
  ArcVolumeMounterBridgeTest() = default;
  ArcVolumeMounterBridgeTest(const ArcVolumeMounterBridgeTest&) = delete;
  ArcVolumeMounterBridgeTest& operator=(const ArcVolumeMounterBridgeTest&) =
      delete;
  ~ArcVolumeMounterBridgeTest() override = default;

  void SetUp() override {
    ash::disks::DiskMountManager::InitializeForTesting(
        new ash::disks::MockDiskMountManager);
    context_ = std::make_unique<TestBrowserContext>();
    bridge_ =
        ArcVolumeMounterBridge::GetForBrowserContextForTesting(context_.get());
  }

  void TearDown() override {
    context_.reset();
    ash::disks::DiskMountManager::Shutdown();
  }

  ArcVolumeMounterBridge* bridge() { return bridge_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  std::unique_ptr<TestBrowserContext> context_;
  ArcVolumeMounterBridge* bridge_ = nullptr;
};

TEST_F(ArcVolumeMounterBridgeTest, ConstructDestruct) {
  EXPECT_NE(nullptr, bridge());
}

}  // namespace
}  // namespace arc
