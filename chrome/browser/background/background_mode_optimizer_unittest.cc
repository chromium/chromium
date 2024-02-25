// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/background_mode_optimizer.h"

#include <memory>

#include "chrome/browser/lifetime/browser_shutdown.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Dummy optimizer that skips doing the restart.
// friend with BackgroundModeOptimizer, can't be in the anonymous namespace.
class DummyBackgroundModeOptimizer : public BackgroundModeOptimizer {
 public:
  DummyBackgroundModeOptimizer() = default;

  MOCK_METHOD0(DoRestart, void());
};

TEST(BackgroundModeOptimizerTest, OnKeepAliveRestartStateChanged) {
  // Strict mock, will fail the test if a non expected call is made.
  testing::StrictMock<DummyBackgroundModeOptimizer> optimizer;

  // No restart until we have at least one browser that got opened
  optimizer.OnKeepAliveRestartStateChanged(true);

  optimizer.OnBrowserAdded(nullptr);
  EXPECT_CALL(optimizer, DoRestart()).RetiresOnSaturation();
  optimizer.OnKeepAliveRestartStateChanged(true);

  // Restart should not be called when we are trying to quit
  browser_shutdown::SetTryingToQuit(true);
  optimizer.OnKeepAliveRestartStateChanged(true);

  // Restart should check that restart changed to be allowed.
  browser_shutdown::SetTryingToQuit(false);
  optimizer.OnKeepAliveRestartStateChanged(false);

  EXPECT_CALL(optimizer, DoRestart()).RetiresOnSaturation();
  optimizer.OnKeepAliveRestartStateChanged(true);
}
