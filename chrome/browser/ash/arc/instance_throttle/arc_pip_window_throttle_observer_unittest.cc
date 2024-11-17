// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/instance_throttle/arc_pip_window_throttle_observer.h"

#include <map>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/wm_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"

namespace arc {

namespace {

class FakeWMHelper : public exo::WMHelper {
 public:
  FakeWMHelper() = default;
  ~FakeWMHelper() override = default;
  FakeWMHelper(const FakeWMHelper&) = delete;
  FakeWMHelper& operator=(const FakeWMHelper&) = delete;

  aura::Window* GetPrimaryDisplayContainer(int container_id) override {
    return map_[container_id];
  }

  void SetPrimaryDisplayContainer(int container_id, aura::Window* window) {
    map_[container_id] = window;
  }

 private:
  std::map<int, raw_ptr<aura::Window, CtnExperimental>> map_;
};

}  // namespace

class ArcPipWindowThrottleObserverTest : public testing::Test {
 public:
  ArcPipWindowThrottleObserverTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}
  ArcPipWindowThrottleObserverTest(const ArcPipWindowThrottleObserverTest&) =
      delete;
  ArcPipWindowThrottleObserverTest& operator=(
      const ArcPipWindowThrottleObserverTest&) = delete;

  void SetUp() override {
    // Set up PipContainer
    pip_container_.reset(aura::test::CreateTestWindowWithDelegate(
        &dummy_delegate_, ash::kShellWindowId_PipContainer, gfx::Rect(),
        nullptr));
    wm_helper_ = std::make_unique<FakeWMHelper>();
    wm_helper()->SetPrimaryDisplayContainer(ash::kShellWindowId_PipContainer,
                                            pip_container_.get());
    // Set up PIP windows
    arc_window_.reset(aura::test::CreateTestWindowWithDelegate(
        &dummy_delegate_, 1, gfx::Rect(), nullptr));
    chrome_window_.reset(aura::test::CreateTestWindowWithDelegate(
        &dummy_delegate_, 2, gfx::Rect(), nullptr));
    arc_window_->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
    chrome_window_->SetProperty(chromeos::kAppTypeKey,
                                chromeos::AppType::BROWSER);
  }

  void TearDown() override { wm_helper_.reset(); }

 protected:
  ArcPipWindowThrottleObserver* observer() { return &pip_observer_; }

  FakeWMHelper* wm_helper() { return wm_helper_.get(); }

  aura::Window* pip_container() { return pip_container_.get(); }

  aura::Window* arc_window() { return arc_window_.get(); }

  aura::Window* chrome_window() { return chrome_window_.get(); }

 public:
  content::BrowserTaskEnvironment task_environment_;
  ArcPipWindowThrottleObserver pip_observer_;
  std::unique_ptr<FakeWMHelper> wm_helper_;
  aura::test::TestWindowDelegate dummy_delegate_;
  std::unique_ptr<aura::Window> pip_container_;
  std::unique_ptr<aura::Window> arc_window_;
  std::unique_ptr<aura::Window> chrome_window_;
};

TEST_F(ArcPipWindowThrottleObserverTest, TestConstructDestruct) {}

TEST_F(ArcPipWindowThrottleObserverTest, TestActiveWhileArcPipVisible) {
  TestingProfile profile;
  observer()->StartObserving(
      &profile, ArcPipWindowThrottleObserver::ObserverStateChangedCallback());
  EXPECT_FALSE(observer()->active());

  // Test that observer does not activate for Chrome PIP, only for ARC PIP
  pip_container()->AddChild(chrome_window());
  EXPECT_FALSE(observer()->active());
  pip_container()->AddChild(arc_window());
  EXPECT_TRUE(observer()->active());

  pip_container()->RemoveChild(chrome_window());
  EXPECT_TRUE(observer()->active());
  pip_container()->RemoveChild(arc_window());
  EXPECT_FALSE(observer()->active());

  observer()->StopObserving();
}

}  // namespace arc
