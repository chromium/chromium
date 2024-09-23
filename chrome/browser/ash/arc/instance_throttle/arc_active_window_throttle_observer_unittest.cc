// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_active_window_throttle_observer.h"

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"

namespace arc {

class ArcActiveWindowThrottleObserverTest : public testing::Test {
 public:
  ArcActiveWindowThrottleObserverTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  ArcActiveWindowThrottleObserverTest(
      const ArcActiveWindowThrottleObserverTest&) = delete;
  ArcActiveWindowThrottleObserverTest& operator=(
      const ArcActiveWindowThrottleObserverTest&) = delete;

 protected:
  ArcActiveWindowThrottleObserver* window_observer() {
    return &window_observer_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ArcActiveWindowThrottleObserver window_observer_;
};

TEST_F(ArcActiveWindowThrottleObserverTest, TestConstructDestruct) {}

TEST_F(ArcActiveWindowThrottleObserverTest, TestOnWindowActivated) {
  aura::test::TestWindowDelegate dummy_delegate;
  std::unique_ptr<aura::Window> arc_window(
      aura::test::CreateTestWindowWithDelegate(&dummy_delegate, 1, gfx::Rect(),
                                               nullptr));
  std::unique_ptr<aura::Window> chrome_window(
      aura::test::CreateTestWindowWithDelegate(&dummy_delegate, 2, gfx::Rect(),
                                               nullptr));
  arc_window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  chrome_window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::BROWSER);

  EXPECT_FALSE(window_observer()->active());

  window_observer()->OnWindowActivated(
      ArcActiveWindowThrottleObserver::ActivationReason::INPUT_EVENT,
      arc_window.get(), chrome_window.get());
  EXPECT_TRUE(window_observer()->active());

  window_observer()->OnWindowActivated(
      ArcActiveWindowThrottleObserver::ActivationReason::INPUT_EVENT,
      chrome_window.get(), arc_window.get());
  EXPECT_FALSE(window_observer()->active());
}

}  // namespace arc
