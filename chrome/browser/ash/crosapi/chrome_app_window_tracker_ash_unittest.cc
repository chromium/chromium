// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/chrome_app_window_tracker_ash.h"
#include "base/memory/raw_ptr.h"

#include "base/test/task_environment.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "components/exo/window_properties.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/window.h"

namespace crosapi {
namespace {

const char kAppId[] = "app_id";
const char kWindowSuffix[] = "window_id";

// A fake window tracker that does not update the shelf.
class ChromeAppWindowTrackerAshFake : public ChromeAppWindowTrackerAsh {
 public:
  void UpdateShelf(const std::string& app_id, aura::Window* window) override {
    last_updated_app_id_ = app_id;
    last_updated_window_ = window;
  }

  std::string last_updated_app_id_;
  raw_ptr<aura::Window, DanglingUntriaged | ExperimentalAsh>
      last_updated_window_ = nullptr;
};

class ChromeAppWindowTrackerAsh : public testing::Test {
 public:
  ChromeAppWindowTrackerAsh()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}
  ChromeAppWindowTrackerAsh(const ChromeAppWindowTrackerAsh&) = delete;
  ChromeAppWindowTrackerAsh& operator=(const ChromeAppWindowTrackerAsh&) =
      delete;
  ~ChromeAppWindowTrackerAsh() override = default;

  void SetUp() override {
    aura_test_helper_ = std::make_unique<aura::test::AuraTestHelper>();
    aura_test_helper_->SetUp();
    window_tracker_ = std::make_unique<ChromeAppWindowTrackerAshFake>();
    window_id_ = std::string(crosapi::kLacrosAppIdPrefix) + kWindowSuffix;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<aura::test::AuraTestHelper> aura_test_helper_;
  std::unique_ptr<ChromeAppWindowTrackerAshFake> window_tracker_;
  std::string window_id_;
};

// Checks that the shelf is updated appropriately when the app_id is received
// first, then the window.
TEST_F(ChromeAppWindowTrackerAsh, FirstAppThenWindow) {
  window_tracker_->OnAppWindowAdded(kAppId, window_id_);
  auto window = std::make_unique<aura::Window>(nullptr);
  window->SetProperty(exo::kApplicationIdKey, window_id_);
  EXPECT_EQ(window_tracker_->last_updated_window_, nullptr);

  window->Init(ui::LAYER_NOT_DRAWN);
  EXPECT_EQ(window_tracker_->last_updated_window_, window.get());
  EXPECT_EQ(window_tracker_->last_updated_app_id_, kAppId);
}

// Checks that the shelf is updated appropriately when the window is received
// first, then the app_id.
TEST_F(ChromeAppWindowTrackerAsh, FirstWindowThenApp) {
  auto window = std::make_unique<aura::Window>(nullptr);
  window->SetProperty(exo::kApplicationIdKey, window_id_);
  window->Init(ui::LAYER_NOT_DRAWN);
  EXPECT_EQ(window_tracker_->last_updated_window_, nullptr);

  window_tracker_->OnAppWindowAdded(kAppId, window_id_);
  EXPECT_EQ(window_tracker_->last_updated_window_, window.get());
  EXPECT_EQ(window_tracker_->last_updated_app_id_, kAppId);
}

// Checks that the shelf is not updated if the window is removed before the app
// is updated.
TEST_F(ChromeAppWindowTrackerAsh, WindowRemovedBeforeApp) {
  auto window = std::make_unique<aura::Window>(nullptr);
  window->SetProperty(exo::kApplicationIdKey, window_id_);
  window->Init(ui::LAYER_NOT_DRAWN);
  window.reset();
  EXPECT_EQ(window_tracker_->last_updated_window_, nullptr);

  window_tracker_->OnAppWindowAdded(kAppId, window_id_);
  EXPECT_EQ(window_tracker_->last_updated_window_, nullptr);
}

// Checks that the shelf is not updated if the app is removed before the window
// is updated.
TEST_F(ChromeAppWindowTrackerAsh, AppRemovedBeforeWindow) {
  window_tracker_->OnAppWindowAdded(kAppId, window_id_);
  window_tracker_->OnAppWindowRemoved(kAppId, window_id_);
  EXPECT_EQ(window_tracker_->last_updated_window_, nullptr);

  auto window = std::make_unique<aura::Window>(nullptr);
  window->SetProperty(exo::kApplicationIdKey, window_id_);
  window->Init(ui::LAYER_NOT_DRAWN);
  EXPECT_EQ(window_tracker_->last_updated_window_, nullptr);
}

}  // namespace
}  // namespace crosapi
