// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/game_mode/testing/game_mode_controller_test_base.h"

namespace game_mode {

void GameModeControllerTestBase::SetUp() {
  ChromeAshTestBase::SetUp();
  fake_resourced_client_ = new ash::FakeResourcedClient();
  profile_ = std::make_unique<TestingProfile>();
  game_mode_controller_ = std::make_unique<GameModeController>();
  histogram_tester_ = std::make_unique<base::HistogramTester>();
}

void GameModeControllerTestBase::TearDown() {
  game_mode_controller_.reset();
  histogram_tester_.reset();
  ash::ResourcedClient::Shutdown();
  ChromeAshTestBase::TearDown();
}

}  // namespace game_mode
