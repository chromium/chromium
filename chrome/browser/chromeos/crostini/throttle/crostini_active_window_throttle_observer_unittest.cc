// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/throttle/crostini_active_window_throttle_observer.h"

#include "ash/public/cpp/app_types.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"

namespace crostini {

class CrostiniActiveWindowThrottleObserverTest : public testing::Test {
 public:
  using testing::Test::Test;

 protected:
  CrostiniActiveWindowThrottleObserver* observer() { return &observer_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  CrostiniActiveWindowThrottleObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniActiveWindowThrottleObserverTest);
};

TEST_F(CrostiniActiveWindowThrottleObserverTest, TestConstructDestruct) {}

TEST_F(CrostiniActiveWindowThrottleObserverTest, TestOnWindowActivated) {
  aura::test::TestWindowDelegate dummy_delegate;
  aura::Window* crostini_window = aura::test::CreateTestWindowWithDelegate(
      &dummy_delegate, 1, gfx::Rect(), nullptr);
  aura::Window* chrome_window = aura::test::CreateTestWindowWithDelegate(
      &dummy_delegate, 2, gfx::Rect(), nullptr);
  crostini_window->SetProperty(aura::client::kAppType,
                               static_cast<int>(ash::AppType::CROSTINI_APP));
  chrome_window->SetProperty(aura::client::kAppType,
                             static_cast<int>(ash::AppType::BROWSER));

  EXPECT_FALSE(observer()->active());

  // Test observer is active for crostini window.
  observer()->OnWindowActivated(
      CrostiniActiveWindowThrottleObserver::ActivationReason::INPUT_EVENT,
      crostini_window, chrome_window);
  EXPECT_TRUE(observer()->active());

  // Test observer is inactive for non-crostini window.
  observer()->OnWindowActivated(
      CrostiniActiveWindowThrottleObserver::ActivationReason::INPUT_EVENT,
      chrome_window, crostini_window);
  EXPECT_FALSE(observer()->active());

  // Test observer is inactive for null gained_active window.
  observer()->OnWindowActivated(
      CrostiniActiveWindowThrottleObserver::ActivationReason::INPUT_EVENT,
      nullptr, crostini_window);
  EXPECT_FALSE(observer()->active());
}

}  // namespace crostini
