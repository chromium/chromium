// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GAME_MODE_TESTING_GAME_MODE_CONTROLLER_TEST_BASE_H_
#define CHROME_BROWSER_ASH_GAME_MODE_TESTING_GAME_MODE_CONTROLLER_TEST_BASE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/resourced/fake_resourced_client.h"
#include "chromeos/ash/components/game_mode/game_mode_controller.h"
#include "content/public/test/browser_task_environment.h"

namespace game_mode {

// Test base for all game mode types (e.g. Borealis, ARC).
class GameModeControllerTestBase : public ChromeAshTestBase {
 public:
  GameModeControllerTestBase()
      : ChromeAshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                base::test::TaskEnvironment::TimeSource::MOCK_TIME))) {}

 protected:
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<GameModeController> game_mode_controller_;
  raw_ptr<ash::FakeResourcedClient, DanglingUntriaged> fake_resourced_client_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

}  // namespace game_mode

#endif  // CHROME_BROWSER_ASH_GAME_MODE_TESTING_GAME_MODE_CONTROLLER_TEST_BASE_H_
