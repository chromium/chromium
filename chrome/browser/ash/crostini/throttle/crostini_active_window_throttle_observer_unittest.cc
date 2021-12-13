// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/throttle/crostini_active_window_throttle_observer.h"

#include "ash/constants/app_types.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"

namespace crostini {

class CrostiniActiveWindowThrottleObserverTest : public testing::Test {
 public:
  using testing::Test::Test;

  CrostiniActiveWindowThrottleObserverTest(
      const CrostiniActiveWindowThrottleObserverTest&) = delete;
  CrostiniActiveWindowThrottleObserverTest& operator=(
      const CrostiniActiveWindowThrottleObserverTest&) = delete;

 protected:
  CrostiniActiveWindowThrottleObserver* observer() { return &observer_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  CrostiniActiveWindowThrottleObserver observer_;
};

TEST_F(CrostiniActiveWindowThrottleObserverTest, TestConstructDestruct) {}

TEST_F(CrostiniActiveWindowThrottleObserverTest, TestOnWindowActivated) {
  aura::test::TestWindowDelegate dummy_delegate;
  std::unique_ptr<aura::Window> crostini_window(
      aura::test::CreateTestWindowWithDelegate(&dummy_delegate, 1, gfx::Rect(),
                                               nullptr));
  std::unique_ptr<aura::Window> chrome_window(
      aura::test::CreateTestWindowWithDelegate(&dummy_delegate, 2, gfx::Rect(),
                                               nullptr));
  crostini_window->SetProperty(aura::client::kAppType,
                               static_cast<int>(ash::AppType::CROSTINI_APP));
  chrome_window->SetProperty(aura::client::kAppType,
                             static_cast<int>(ash::AppType::BROWSER));

  EXPECT_FALSE(observer()->active());

  // Test observer is active for crostini window.
  observer()->OnWindowActivated(
      CrostiniActiveWindowThrottleObserver::ActivationReason::INPUT_EVENT,
      crostini_window.get(), chrome_window.get());
  EXPECT_TRUE(observer()->active());

  // Test observer is inactive for non-crostini window.
  observer()->OnWindowActivated(
      CrostiniActiveWindowThrottleObserver::ActivationReason::INPUT_EVENT,
      chrome_window.get(), crostini_window.get());
  EXPECT_FALSE(observer()->active());

  // Test observer is inactive for null gained_active window.
  observer()->OnWindowActivated(
      CrostiniActiveWindowThrottleObserver::ActivationReason::INPUT_EVENT,
      nullptr, crostini_window.get());
  EXPECT_FALSE(observer()->active());
}

}  // namespace crostini
