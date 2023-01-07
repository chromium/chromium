// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/idle_service.h"

#include <memory>

#include "base/scoped_observation.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/profile_ui_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/idle/idle_polling_service.h"
#include "ui/base/idle/idle_time_provider.h"
#include "ui/base/test/idle_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

using base::TestMockTimeTaskRunner;
using testing::Return;

namespace {

class MockIdleTimeProvider : public ui::IdleTimeProvider {
 public:
  MockIdleTimeProvider() = default;

  MockIdleTimeProvider(const MockIdleTimeProvider&) = delete;
  MockIdleTimeProvider& operator=(const MockIdleTimeProvider&) = delete;

  ~MockIdleTimeProvider() override = default;

  MOCK_METHOD0(CalculateIdleTime, base::TimeDelta());
  MOCK_METHOD0(CheckIdleStateIsLocked, bool());
};

class BrowserCloseWaiter : public BrowserListObserver {
 public:
  explicit BrowserCloseWaiter(std::set<Browser*> browsers) {
    BrowserList::AddObserver(this);
    waiting_browsers_ = std::move(browsers);
  }

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override {
    waiting_browsers_.erase(browser);
    if (waiting_browsers_.size() == 0) {
      BrowserList::RemoveObserver(this);
      run_loop_.QuitWhenIdle();
    }
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
  std::set<Browser*> waiting_browsers_;
};

}  // namespace

namespace enterprise_idle {

class IdleServiceTest : public InProcessBrowserTest {
 public:
  IdleServiceTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    polling_service().SetTaskRunnerForTest(task_runner_);
    auto time_provider = std::make_unique<MockIdleTimeProvider>();
    time_provider_ = time_provider.get();
    scoped_idle_provider_ =
        std::make_unique<ui::test::ScopedIdleProviderForTest>(
            std::move(time_provider));
  }

  void TearDownOnMainThread() override {
    std::vector<Profile*> profiles =
        g_browser_process->profile_manager()->GetLoadedProfiles();
    for (Profile* profile : profiles) {
      profile->GetPrefs()->ClearPref("idle_profile_close_timeout");
    }
    ASSERT_FALSE(polling_service().IsPollingForTest());
    polling_service().SetTaskRunnerForTest(base::ThreadTaskRunnerHandle::Get());
  }

  MockIdleTimeProvider& provider() { return *time_provider_; }
  ui::IdlePollingService& polling_service() {
    return *ui::IdlePollingService::GetInstance();
  }
  base::TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

  int GetBrowserCount(Profile* profile) {
    int count = 0;
    for (auto* browser : *BrowserList::GetInstance()) {
      if (browser->profile() == profile)
        count++;
    }
    return count;
  }

 private:
  MockIdleTimeProvider* time_provider_ = nullptr;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  std::unique_ptr<ui::test::ScopedIdleProviderForTest> scoped_idle_provider_;
};

IN_PROC_BROWSER_TEST_F(IdleServiceTest, Basic) {
  ON_CALL(provider(), CheckIdleStateIsLocked()).WillByDefault(Return(false));

  // Set the IdleProfileCloseTimeout policy to 1 minute, which should round up
  // to 5 minutes (the minimum).
  EXPECT_CALL(provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(298)));
  Profile* profile = browser()->profile();
  profile->GetPrefs()->SetInteger("idle_profile_close_timeout", 1);

  EXPECT_EQ(1, GetBrowserCount(profile));

  // 299s, does nothing.
  EXPECT_CALL(provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(299)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, GetBrowserCount(profile));

  // 300s, threshold is reached. Close browsers, then show the Profile Picker.
  EXPECT_CALL(provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(300)));
  BrowserCloseWaiter waiter({browser()});
  task_runner()->FastForwardBy(base::Seconds(1));
  waiter.Wait();
  EXPECT_EQ(0, GetBrowserCount(profile));
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

IN_PROC_BROWSER_TEST_F(IdleServiceTest, TenMinutes) {
  ON_CALL(provider(), CheckIdleStateIsLocked()).WillByDefault(Return(false));

  // Set the IdleProfileCloseTimeout policy to 10 minutes.
  EXPECT_CALL(provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(599)));
  Profile* profile = browser()->profile();
  profile->GetPrefs()->SetInteger("idle_profile_close_timeout", 10);

  EXPECT_EQ(1, GetBrowserCount(profile));

  // 599s, does nothing.
  EXPECT_CALL(provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(599)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, GetBrowserCount(profile));

  // 600s, threshold is reached. Close browsers, then show the Profile Picker.
  EXPECT_CALL(provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(600)));
  BrowserCloseWaiter waiter({browser()});
  task_runner()->FastForwardBy(base::Seconds(1));
  waiter.Wait();
  EXPECT_EQ(0, GetBrowserCount(profile));
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

// TODO(crbug.com/1344609): Test flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MultiProfile DISABLED_MultiProfile
#else
#define MAYBE_MultiProfile MultiProfile
#endif
IN_PROC_BROWSER_TEST_F(IdleServiceTest, MAYBE_MultiProfile) {
  ON_CALL(provider(), CheckIdleStateIsLocked()).WillByDefault(Return(false));

  // `profile` has the IdleProfileCloseTimeout policy set to 5 minutes.
  EXPECT_CALL(provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(299)));
  Profile* profile = browser()->profile();
  Browser* browser2 = CreateBrowser(profile);
  profile->GetPrefs()->SetInteger("idle_profile_close_timeout", 5);

  // `profile2` has the policy set to 5 minutes, so it will close at the same
  // time as `profile`.
  Profile* profile2;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile2 = g_browser_process->profile_manager()->GetProfile(
        g_browser_process->profile_manager()->user_data_dir().AppendASCII(
            "Profile 2"));
  }
  Browser* browser3 = CreateBrowser(profile2);
  profile2->GetPrefs()->SetInteger("idle_profile_close_timeout", 5);

  // `profile3` doesn't have the IdleProfileCloseTimeout policy set, so it will
  // never close.
  Profile* profile3;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile3 = g_browser_process->profile_manager()->GetProfile(
        g_browser_process->profile_manager()->user_data_dir().AppendASCII(
            "Profile 3"));
  }
  std::ignore = CreateBrowser(profile3);

  EXPECT_EQ(2, GetBrowserCount(profile));
  EXPECT_EQ(1, GetBrowserCount(profile2));
  EXPECT_EQ(1, GetBrowserCount(profile3));

  // 299s, does nothing.
  EXPECT_CALL(provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(299)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(2, GetBrowserCount(profile));
  EXPECT_EQ(1, GetBrowserCount(profile2));
  EXPECT_EQ(1, GetBrowserCount(profile3));

  // 300s, threshold is reached. Close browsers, then show the Profile Picker.
  EXPECT_CALL(provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(300)));
  BrowserCloseWaiter waiter({browser(), browser2, browser3});
  task_runner()->FastForwardBy(base::Seconds(1));
  waiter.Wait();
  EXPECT_EQ(0, GetBrowserCount(profile));
  EXPECT_EQ(0, GetBrowserCount(profile2));
  EXPECT_EQ(1, GetBrowserCount(profile3));
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

IN_PROC_BROWSER_TEST_F(IdleServiceTest, MultiProfileWithDifferentThresholds) {
  ON_CALL(provider(), CheckIdleStateIsLocked()).WillByDefault(Return(false));

  // `profile` has the IdleProfileCloseTimeout policy set to 5 minutes.
  EXPECT_CALL(provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(299)));
  Profile* profile = browser()->profile();
  Browser* browser2 = CreateBrowser(profile);
  profile->GetPrefs()->SetInteger("idle_profile_close_timeout", 1);

  // `profile2` has the policy set to 6 minutes, so it will close one minute
  // *after* `profile`.
  Profile* profile2;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile2 = g_browser_process->profile_manager()->GetProfile(
        g_browser_process->profile_manager()->user_data_dir().AppendASCII(
            "Profile 2"));
  }
  Browser* browser3 = CreateBrowser(profile2);
  profile2->GetPrefs()->SetInteger("idle_profile_close_timeout", 6);

  EXPECT_EQ(2, GetBrowserCount(profile));
  EXPECT_EQ(1, GetBrowserCount(profile2));

  // 299s, does nothing.
  EXPECT_CALL(provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(299)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(2, GetBrowserCount(profile));
  EXPECT_EQ(1, GetBrowserCount(profile2));

  // 300s, threshold is reached for `profile`. Close its browsers, then show the
  // Profile Picker.
  EXPECT_CALL(provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(300)));
  {
    BrowserCloseWaiter waiter({browser(), browser2});
    task_runner()->FastForwardBy(base::Seconds(1));
    waiter.Wait();
  }
  EXPECT_EQ(0, GetBrowserCount(profile));
  EXPECT_EQ(1, GetBrowserCount(profile2));
  EXPECT_TRUE(ProfilePicker::IsOpen());

  // 360s, threshold is reached for `profile2`. Close its browsers.
  EXPECT_CALL(provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(360)));
  {
    BrowserCloseWaiter waiter({browser3});
    task_runner()->FastForwardBy(base::Seconds(1));
    waiter.Wait();
  }
  EXPECT_EQ(0, GetBrowserCount(profile));
  EXPECT_EQ(0, GetBrowserCount(profile2));
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

}  // namespace enterprise_idle
