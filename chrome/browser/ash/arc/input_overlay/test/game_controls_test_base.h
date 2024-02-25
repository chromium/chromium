// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_GAME_CONTROLS_TEST_BASE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_GAME_CONTROLS_TEST_BASE_H_

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"

class TestingProfile;

namespace arc::input_overlay {

class ArcInputOverlayManager;
class DisplayOverlayController;
class TouchInjector;

// UI test base for beta+ version.
class GameControlsTestBase : public ash::AshTestBase {
 public:
  GameControlsTestBase();
  ~GameControlsTestBase() override;

 protected:
  TouchInjector* GetTouchInjector(aura::Window* window);
  DisplayOverlayController* GetDisplayOverlayController();
  void EnableDisplayMode(DisplayMode mode);

  // ash::AshTestBase:
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<ArcInputOverlayManager> arc_test_input_overlay_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<TouchInjector, DanglingUntriaged> touch_injector_;
  raw_ptr<DisplayOverlayController, DanglingUntriaged> controller_;

 private:
  ArcAppTest arc_app_test_;
  std::unique_ptr<TestingProfile> profile_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_GAME_CONTROLS_TEST_BASE_H_
