// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/screen_projection_change_monitor.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test/ash_test_base.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/ash/cast_config/cast_config_controller_media_router.h"
#include "ui/display/manager/display_manager.h"

namespace ash::input_method {

namespace {

using base::test::TestFuture;
using ScreenProjectionChangeMonitorTest = AshTestBase;

TEST_F(ScreenProjectionChangeMonitorTest, MirroringChangeTriggersCallback) {
  CastConfigControllerMediaRouter cast_config;
  TestFuture<bool> monitor_future;
  ScreenProjectionChangeMonitor monitor(monitor_future.GetRepeatingCallback());

  // Turn on mirroring.
  UpdateDisplay("600x500,600x500");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display_manager->SetMultiDisplayMode(display::DisplayManager::MIRRORING);
  display_manager->UpdateDisplays();
  EXPECT_TRUE(monitor_future.Take());

  // Turn off mirroring.
  UpdateDisplay("600x500");
  EXPECT_FALSE(monitor_future.Take());
}

TEST_F(ScreenProjectionChangeMonitorTest, ScreenSharingChangeTriggersCallback) {
  CastConfigControllerMediaRouter cast_config;
  TestFuture<bool> monitor_future;
  ScreenProjectionChangeMonitor monitor(monitor_future.GetRepeatingCallback());

  Shell::Get()->system_tray_notifier()->NotifyScreenAccessStart(
      base::DoNothing(), base::DoNothing(), u"");
  EXPECT_TRUE(monitor_future.Take());

  Shell::Get()->system_tray_notifier()->NotifyScreenAccessStop();
  EXPECT_FALSE(monitor_future.Take());
}

TEST_F(ScreenProjectionChangeMonitorTest, NoChangeDoesNotTriggerCallback) {
  CastConfigControllerMediaRouter cast_config;
  TestFuture<bool> monitor_future;
  ScreenProjectionChangeMonitor monitor(monitor_future.GetRepeatingCallback());

  // Turn on mirroring.
  UpdateDisplay("600x500,600x500");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display_manager->SetMultiDisplayMode(display::DisplayManager::MIRRORING);
  display_manager->UpdateDisplays();
  EXPECT_TRUE(monitor_future.Take());

  // Turning on screen capture will not change the screen projection state
  // because we are already mirroring.
  Shell::Get()->system_tray_notifier()->NotifyScreenAccessStart(
      base::DoNothing(), base::DoNothing(), u"");
  // This should not invoke our monitor.
  EXPECT_FALSE(monitor_future.IsReady());
}

}  // namespace

}  // namespace ash::input_method
