// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/windowed_incognito_observer.h"

#include "base/test/bind.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {
// Allows access to some private methods for testing.
class TestWindowedIncognitoMonitor : public WindowedIncognitoMonitor {
 public:
  TestWindowedIncognitoMonitor() : WindowedIncognitoMonitor() {}

  TestWindowedIncognitoMonitor(const TestWindowedIncognitoMonitor&) = delete;
  TestWindowedIncognitoMonitor& operator=(const TestWindowedIncognitoMonitor&) =
      delete;

  int num_on_browser_added() { return num_on_browser_added_; }
  int num_on_browser_removed() { return num_on_browser_removed_; }

  using WindowedIncognitoMonitor::num_active_incognito_windows;
  using WindowedIncognitoMonitor::num_incognito_window_opened;

 private:
  // BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override {
    num_on_browser_added_++;
    WindowedIncognitoMonitor::OnBrowserAdded(browser);
  }

  void OnBrowserRemoved(Browser* browser) override {
    num_on_browser_removed_++;
    WindowedIncognitoMonitor::OnBrowserRemoved(browser);
  }

  int num_on_browser_added_ = 0;
  int num_on_browser_removed_ = 0;
};

}  // namespace

class WindowedIncognitoMonitorTest : public testing::Test {
 public:
  WindowedIncognitoMonitorTest() = default;

  WindowedIncognitoMonitorTest(const WindowedIncognitoMonitorTest&) = delete;
  WindowedIncognitoMonitorTest& operator=(const WindowedIncognitoMonitorTest&) =
      delete;

  void SetUp() override {
    // Instantiate a testing profile.
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
    incognito_monitor_ = std::make_unique<TestWindowedIncognitoMonitor>();
  }

  void TearDown() override {
    incognito_monitor_.reset();
    profile_.reset();
  }

  size_t OpenBrowserWindow(bool incognito) {
    auto browser_window = std::make_unique<TestBrowserWindow>();
    Profile* browser_profile =
        incognito ? profile_->GetPrimaryOTRProfile(/*create_if_needed=*/true)
                  : profile_.get();
    Browser::CreateParams params(browser_profile, true);
    params.type = Browser::TYPE_NORMAL;
    params.window = browser_window.get();
    auto browser = std::unique_ptr<Browser>(Browser::Create(params));

    size_t handle = next_browser_id++;
    open_browsers_[handle] =
        std::make_pair(std::move(browser_window), std::move(browser));
    return handle;
  }

  // Closes the browser window with the given handle.
  void CloseBrowserWindow(size_t handle) {
    auto it = open_browsers_.find(handle);
    ASSERT_FALSE(it == open_browsers_.end());
    open_browsers_.erase(it);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  // The associated testing browser profile.
  std::unique_ptr<TestingProfile> profile_;

  // Keep track of the open browsers and accompanying windows.
  std::unordered_map<
      size_t,
      std::pair<std::unique_ptr<TestBrowserWindow>, std::unique_ptr<Browser>>>
      open_browsers_;
  static size_t next_browser_id;

  std::unique_ptr<TestWindowedIncognitoMonitor> incognito_monitor_;
};

size_t WindowedIncognitoMonitorTest::next_browser_id = 1;

// Test that BrowserListObserver callbacks work as expected.
TEST_F(WindowedIncognitoMonitorTest, CheckSetup) {
  // Open a normal window.
  size_t window1 = OpenBrowserWindow(false);
  EXPECT_EQ(incognito_monitor_->num_on_browser_added(), 1);
  EXPECT_EQ(incognito_monitor_->num_on_browser_removed(), 0);

  // Close the normal window.
  CloseBrowserWindow(window1);
  EXPECT_EQ(incognito_monitor_->num_on_browser_added(), 1);
  EXPECT_EQ(incognito_monitor_->num_on_browser_removed(), 1);

  // Open an incognito window.
  size_t window2 = OpenBrowserWindow(true);
  EXPECT_EQ(incognito_monitor_->num_on_browser_added(), 2);
  EXPECT_EQ(incognito_monitor_->num_on_browser_removed(), 1);

  // Open a normal window.
  size_t window3 = OpenBrowserWindow(false);
  EXPECT_EQ(incognito_monitor_->num_on_browser_added(), 3);
  EXPECT_EQ(incognito_monitor_->num_on_browser_removed(), 1);

  // Close the normal window.
  CloseBrowserWindow(window3);
  EXPECT_EQ(incognito_monitor_->num_on_browser_added(), 3);
  EXPECT_EQ(incognito_monitor_->num_on_browser_removed(), 2);

  // Close the incognito window.
  CloseBrowserWindow(window2);
  EXPECT_EQ(incognito_monitor_->num_on_browser_added(), 3);
  EXPECT_EQ(incognito_monitor_->num_on_browser_removed(), 3);
}

// Test that incognito window state is recorded correctly.
TEST_F(WindowedIncognitoMonitorTest, NumIncognitoWindow) {
  // Open a normal window. This doesn't affect incognito state.
  size_t window1 = OpenBrowserWindow(false);
  EXPECT_EQ(incognito_monitor_->num_active_incognito_windows(), 0);
  EXPECT_EQ(incognito_monitor_->num_incognito_window_opened(), 0U);

  // Close the normal window.
  CloseBrowserWindow(window1);
  EXPECT_EQ(incognito_monitor_->num_active_incognito_windows(), 0);
  EXPECT_EQ(incognito_monitor_->num_incognito_window_opened(), 0U);

  // Open an incognito window.
  size_t window2 = OpenBrowserWindow(true);
  EXPECT_EQ(incognito_monitor_->num_active_incognito_windows(), 1);
  EXPECT_EQ(incognito_monitor_->num_incognito_window_opened(), 1U);

  // Open another incognito window.
  size_t window3 = OpenBrowserWindow(true);
  EXPECT_EQ(incognito_monitor_->num_active_incognito_windows(), 2);
  EXPECT_EQ(incognito_monitor_->num_incognito_window_opened(), 2U);

  // Close an incognito window.
  CloseBrowserWindow(window3);
  EXPECT_EQ(incognito_monitor_->num_active_incognito_windows(), 1);
  EXPECT_EQ(incognito_monitor_->num_incognito_window_opened(), 2U);

  // Close the final incognito window.
  CloseBrowserWindow(window2);
  EXPECT_EQ(incognito_monitor_->num_active_incognito_windows(), 0);
  EXPECT_EQ(incognito_monitor_->num_incognito_window_opened(), 2U);
}

// Test that WindowedIncognitoObserver returns the correct incognito state.
TEST_F(WindowedIncognitoMonitorTest, IncognitoObserver) {
  auto observer1 = incognito_monitor_->CreateObserver();
  EXPECT_FALSE(observer1->IncognitoActive());
  EXPECT_FALSE(observer1->IncognitoLaunched());

  // Open an incognito window.
  size_t window1 = OpenBrowserWindow(true);
  EXPECT_TRUE(observer1->IncognitoActive());
  EXPECT_TRUE(observer1->IncognitoLaunched());

  // |observer2| is created after incognito window is opened.
  auto observer2 = incognito_monitor_->CreateObserver();
  EXPECT_TRUE(observer2->IncognitoActive());
  EXPECT_FALSE(observer2->IncognitoLaunched());

  // Open another incognito window.
  size_t window2 = OpenBrowserWindow(true);
  EXPECT_TRUE(observer1->IncognitoActive());
  EXPECT_TRUE(observer1->IncognitoLaunched());
  EXPECT_TRUE(observer2->IncognitoActive());
  EXPECT_TRUE(observer2->IncognitoLaunched());

  // Close all incognito windows.
  CloseBrowserWindow(window1);
  CloseBrowserWindow(window2);

  EXPECT_FALSE(observer1->IncognitoActive());
  EXPECT_TRUE(observer1->IncognitoLaunched());
  EXPECT_FALSE(observer2->IncognitoActive());
  EXPECT_TRUE(observer2->IncognitoLaunched());

  // An observer created now should not see incognito active of launched.
  auto observer3 = incognito_monitor_->CreateObserver();
  EXPECT_FALSE(observer3->IncognitoActive());
  EXPECT_FALSE(observer3->IncognitoLaunched());

  size_t window3 = OpenBrowserWindow(true);
  EXPECT_TRUE(observer3->IncognitoActive());
  EXPECT_TRUE(observer3->IncognitoLaunched());

  CloseBrowserWindow(window3);
  EXPECT_FALSE(observer3->IncognitoActive());
  EXPECT_TRUE(observer3->IncognitoLaunched());
}

}  // namespace metrics
