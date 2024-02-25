// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/app/arc_app_launch_notifier.h"

#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class TestObserver : public ArcAppLaunchNotifier::Observer {
 public:
  TestObserver(base::RepeatingCallback<void(std::string_view identifier)>
                   app_launch_callback,
               ArcAppLaunchNotifier* notifier)
      : app_launch_callback_(std::move(app_launch_callback)) {
    observation_.Observe(notifier);
  }
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override { observation_.Reset(); }

  // ArcAppLaunchNotifier::Observer overrides:
  void OnArcAppLaunchRequested(std::string_view identifier) override {
    ASSERT_FALSE(app_launch_callback_.is_null());
    app_launch_callback_.Run(identifier);
  }

 private:
  base::RepeatingCallback<void(std::string_view identifier)>
      app_launch_callback_;

  base::ScopedObservation<ArcAppLaunchNotifier, ArcAppLaunchNotifier::Observer>
      observation_{this};
};

}  // namespace

class ArcAppLaunchNotifierTest : public testing::Test {
 public:
  ArcAppLaunchNotifierTest() = default;
  ArcAppLaunchNotifierTest(const ArcAppLaunchNotifierTest&) = delete;
  ArcAppLaunchNotifierTest& operator=(const ArcAppLaunchNotifierTest&) = delete;

  ~ArcAppLaunchNotifierTest() override = default;

  void SetUp() override {
    notifier_ = ArcAppLaunchNotifier::GetForBrowserContextForTesting(&context_);
    ASSERT_NE(nullptr, notifier());
  }
  ArcAppLaunchNotifier* notifier() { return notifier_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  user_prefs::TestBrowserContextWithPrefs context_;
  raw_ptr<ArcAppLaunchNotifier> notifier_ = nullptr;
};

TEST_F(ArcAppLaunchNotifierTest, Notify) {
  int count = 0;
  const std::string expected_identifier = "app_identifier";
  auto test_observer = TestObserver(
      base::BindLambdaForTesting(
          [&count, &expected_identifier](std::string_view identifier) {
            if (identifier == expected_identifier) {
              count++;
            }
          }),
      notifier());

  EXPECT_EQ(0, count);
  notifier()->NotifyArcAppLaunchRequest(expected_identifier);
  EXPECT_EQ(1, count);
}
}  // namespace arc
