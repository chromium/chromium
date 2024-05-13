// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/throttle/crostini_active_window_throttle_observer.h"

#include "ash/public/cpp/window_properties.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
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
  std::unique_ptr<aura::Window> terminal_window(
      aura::test::CreateTestWindowWithDelegate(&dummy_delegate, 3, gfx::Rect(),
                                               nullptr));
  std::unique_ptr<aura::Window> chrome_app_window(
      aura::test::CreateTestWindowWithDelegate(&dummy_delegate, 4, gfx::Rect(),
                                               nullptr));
  crostini_window->SetProperty(chromeos::kAppTypeKey,
                               chromeos::AppType::CROSTINI_APP);
  chrome_window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::BROWSER);
  terminal_window->SetProperty(chromeos::kAppTypeKey,
                               chromeos::AppType::CHROME_APP);
  terminal_window->SetProperty<std::string>(ash::kAppIDKey,
                                            guest_os::kTerminalSystemAppId);
  chrome_app_window->SetProperty(chromeos::kAppTypeKey,
                                 chromeos::AppType::CHROME_APP);
  chrome_app_window->SetProperty<std::string>(ash::kAppIDKey,
                                              "this_is_another_chrome_app");

  EXPECT_FALSE(observer()->active());

  // Test observer is active for crostini window.
  observer()->OnWindowActivated(
      CrostiniActiveWindowThrottleObserver::ActivationReason::INPUT_EVENT,
      crostini_window.get(), chrome_window.get());
  EXPECT_TRUE(observer()->active());

  // Test observer is active for terminal window.
  observer()->OnWindowActivated(
      CrostiniActiveWindowThrottleObserver::ActivationReason::INPUT_EVENT,
      terminal_window.get(), crostini_window.get());
  EXPECT_TRUE(observer()->active());

  // Test observer is inactive for non-crostini window.
  observer()->OnWindowActivated(
      CrostiniActiveWindowThrottleObserver::ActivationReason::INPUT_EVENT,
      chrome_window.get(), terminal_window.get());
  EXPECT_FALSE(observer()->active());

  observer()->OnWindowActivated(
      CrostiniActiveWindowThrottleObserver::ActivationReason::INPUT_EVENT,
      chrome_app_window.get(), chrome_window.get());
  EXPECT_FALSE(observer()->active());

  // Test observer is inactive for null gained_active window.
  observer()->OnWindowActivated(
      CrostiniActiveWindowThrottleObserver::ActivationReason::INPUT_EVENT,
      nullptr, crostini_window.get());
  EXPECT_FALSE(observer()->active());
}

}  // namespace crostini
