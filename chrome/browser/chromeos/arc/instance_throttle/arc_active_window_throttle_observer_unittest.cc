// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/instance_throttle/arc_active_window_throttle_observer.h"

#include "ash/public/cpp/app_types.h"
#include "base/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/app_list/search/arc/arc_app_reinstall_search_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"

namespace arc {

class ArcActiveWindowThrottleObserverTest : public testing::Test {
 public:
  ArcActiveWindowThrottleObserverTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

 protected:
  ArcActiveWindowThrottleObserver* window_observer() {
    return &window_observer_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ArcActiveWindowThrottleObserver window_observer_;

  DISALLOW_COPY_AND_ASSIGN(ArcActiveWindowThrottleObserverTest);
};

TEST_F(ArcActiveWindowThrottleObserverTest, TestConstructDestruct) {}

TEST_F(ArcActiveWindowThrottleObserverTest, TestOnWindowActivated) {
  aura::test::TestWindowDelegate dummy_delegate;
  aura::Window* arc_window = aura::test::CreateTestWindowWithDelegate(
      &dummy_delegate, 1, gfx::Rect(), nullptr);
  aura::Window* chrome_window = aura::test::CreateTestWindowWithDelegate(
      &dummy_delegate, 2, gfx::Rect(), nullptr);
  arc_window->SetProperty(aura::client::kAppType,
                          static_cast<int>(ash::AppType::ARC_APP));
  chrome_window->SetProperty(aura::client::kAppType,
                             static_cast<int>(ash::AppType::BROWSER));

  EXPECT_FALSE(window_observer()->active());

  window_observer()->OnWindowActivated(
      ArcActiveWindowThrottleObserver::ActivationReason::INPUT_EVENT,
      arc_window, chrome_window);
  EXPECT_TRUE(window_observer()->active());

  window_observer()->OnWindowActivated(
      ArcActiveWindowThrottleObserver::ActivationReason::INPUT_EVENT,
      chrome_window, arc_window);
  EXPECT_FALSE(window_observer()->active());
}

}  // namespace arc
