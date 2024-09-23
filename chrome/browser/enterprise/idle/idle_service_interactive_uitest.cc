// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/idle/dialog_manager.h"
#include "chrome/browser/enterprise/idle/idle_service.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/idle_bubble.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/profiles/profile_ui_test_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/idle/idle_pref_names.h"
#include "components/enterprise/idle/metrics.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/idle/idle_polling_service.h"
#include "ui/base/idle/idle_time_provider.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/test/idle_test_utils.h"

using base::TestMockTimeTaskRunner;
using testing::_;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Not;
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
  explicit BrowserCloseWaiter(
      std::set<raw_ptr<Browser, SetExperimental>> browsers) {
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
  std::set<raw_ptr<Browser, SetExperimental>> waiting_browsers_;
};

}  // namespace

namespace enterprise_idle {

class IdleServiceTest : public InProcessBrowserTest {
 public:
  IdleServiceTest() = default;
  ~IdleServiceTest() override = default;

  void SetUp() override {
    // Prevent user education from polling idle state.
    UserEducationServiceFactory::GetInstance()
        ->disable_idle_polling_for_testing();
    InProcessBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    polling_service().SetTaskRunnerForTest(task_runner_);
    polling_service().SetPollIntervalForTest(base::Seconds(1));

    auto time_provider =
        std::make_unique<testing::NiceMock<MockIdleTimeProvider>>();
    idle_time_provider_ = time_provider.get();
    scoped_idle_provider_ =
        std::make_unique<ui::test::ScopedIdleProviderForTest>(
            std::move(time_provider));
    ON_CALL(idle_time_provider(), CheckIdleStateIsLocked())
        .WillByDefault(Return(false));

    for (auto& provider : policy_providers_) {
      policy::PushProfilePolicyConnectorProviderForTesting(&provider);
    }

    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::BROWSER, KeepAliveRestartOption::DISABLED);

    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownOnMainThread() override {
    policy::PolicyMap policies;
    for (auto& provider : policy_providers_) {
      provider.UpdateChromePolicy(policies);
    }
    ASSERT_FALSE(polling_service().IsPollingForTest());
    polling_service().SetTaskRunnerForTest(
        base::SingleThreadTaskRunner::GetCurrentDefault());

    // If there are no active browsers, BrowserProcessImpl::Unpin() runs too
    // early and interrupts test teardown. `keep_alive_` solves this problem.
    if (chrome::GetTotalBrowserCount() > 0) {
      // It's safe to release this keepalive here, because browser windows are
      // already doing the same thing.
      keep_alive_.reset();
    }

    if (ProfilePicker::IsOpen()) {
      // `ProfilePicker` prevents browsers from being closed. We need to destroy
      // `ProfilePicker` first, and then tear down browsers.
      ProfilePicker::Hide();
    }

    InProcessBrowserTest::TearDownOnMainThread();
  }

  void TearDownInProcessBrowserTestFixture() override {
    keep_alive_.reset();
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void SetIdleTimeoutPolicies(
      policy::MockConfigurationPolicyProvider& policy_provider,
      int idle_timeout,
      const std::vector<std::string>& idle_timeout_actions = {
          "close_browsers", "show_profile_picker"}) {
    base::Value::List actions_list;
    for (const std::string& action : idle_timeout_actions) {
      actions_list.Append(action);
    }

    policy::PolicyMap policies;
    policies.Set(policy::key::kIdleTimeout, policy::POLICY_LEVEL_MANDATORY,
                 policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
                 base::Value(idle_timeout), nullptr);
    policies.Set(policy::key::kIdleTimeoutActions,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_PLATFORM,
                 base::Value(std::move(actions_list)), nullptr);
    policy_provider.UpdateChromePolicy(policies);
  }

  policy::MockConfigurationPolicyProvider& policy_provider(size_t index) {
    return policy_providers_[index];
  }
  MockIdleTimeProvider& idle_time_provider() { return *idle_time_provider_; }
  ui::IdlePollingService& polling_service() {
    return *ui::IdlePollingService::GetInstance();
  }
  base::TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

  int GetBrowserCount(Profile* profile) {
    int count = 0;
    for (Browser* browser : *BrowserList::GetInstance()) {
      if (browser->profile() == profile) {
        count++;
      }
    }
    return count;
  }

  bool IsDialogOpen() const {
    return enterprise_idle::DialogManager::GetInstance()
        ->IsDialogOpenForTesting();
  }

  void ActivateBrowser(Browser* browser) {
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_WAYLAND)
    // TODO(nicolaso): BrowserActivationWaiter times out on Wayland. Figure out
    // why.
#else
    ActivateBrowserImpl(browser);
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_WAYLAND)
  }

  void ActivateBrowserImpl(Browser* browser) {
    if (GetIdleBubble(browser)) {
      return;
    }
    CHECK(browser);
    ui_test_utils::BrowserActivationWaiter waiter(browser);
    browser->window()->Activate();
    waiter.WaitForActivation();
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider>
      policy_providers_[2];
  raw_ptr<MockIdleTimeProvider, AcrossTasksDanglingUntriaged>
      idle_time_provider_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  std::unique_ptr<ui::test::ScopedIdleProviderForTest> scoped_idle_provider_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
};

IN_PROC_BROWSER_TEST_F(IdleServiceTest, Basic) {
  // Set the IdleTimeout policy to 0 minute, which should round up
  // to 1 minute (the minimum).
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(58)));
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  Profile* profile = browser()->profile();
  SetIdleTimeoutPolicies(policy_provider(0), /*idle_timeout=*/0);

  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 59s, does nothing.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(59)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 60s, threshold is reached. This should show the dialog.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_TRUE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillRepeatedly(Return(base::Seconds(15)));
  BrowserCloseWaiter waiter({browser()});
  task_runner()->FastForwardBy(base::Seconds(30));
  waiter.Wait();
  EXPECT_EQ(0, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_TRUE(ProfilePicker::IsOpen());

  // Check that the idle dialog events are recorded in the histogram.
  EXPECT_THAT(
      histogram_tester->GetAllSamples(
          "Enterprise.IdleTimeoutPolicies.IdleTimeoutDialogEvent"),
      ElementsAre(
          base::Bucket(metrics::IdleTimeoutDialogEvent::kDialogShown, 1),
          base::Bucket(metrics::IdleTimeoutDialogEvent::kDialogExpired, 1)));
  // Check that the success of idle timeout actions is recorded.
  histogram_tester->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ShowProfilePicker", true, 1);
  histogram_tester->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.CloseBrowsers", true, 1);
  histogram_tester->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.AllActions", true, 1);
}

IN_PROC_BROWSER_TEST_F(IdleServiceTest, DidNotClose) {
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(59)));
  Profile* profile = browser()->profile();
  SetIdleTimeoutPolicies(policy_provider(0), /*idle_timeout=*/1);

  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 60s, threshold is reached. The user dismisses the dialog though, so we do
  // nothing.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_TRUE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillRepeatedly(Return(base::Seconds(61)));
  DialogManager::GetInstance()->DismissDialogForTesting();
  task_runner()->FastForwardBy(base::Seconds(30));
  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());
}

IN_PROC_BROWSER_TEST_F(IdleServiceTest, TenMinutes) {
  // Set the IdleTimeout policy to 10 minutes.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(599)));
  Profile* profile = browser()->profile();
  SetIdleTimeoutPolicies(policy_provider(0), /*idle_timeout=*/10);

  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 599s, does nothing.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(599)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 600s, threshold is reached. Close browsers, then show the Profile Picker.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(600)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_TRUE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillRepeatedly(Return(base::Seconds(615)));
  BrowserCloseWaiter waiter({browser()});
  task_runner()->FastForwardBy(base::Seconds(30));
  waiter.Wait();
  EXPECT_EQ(0, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

// TODO(crbug.com/40853067): Test flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MultiProfile DISABLED_MultiProfile
#else
#define MAYBE_MultiProfile MultiProfile
#endif
IN_PROC_BROWSER_TEST_F(IdleServiceTest, MAYBE_MultiProfile) {
  // `profile` has the IdleTimeout policy set to 5 minutes.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(299)));
  Profile* profile = browser()->profile();
  Browser* browser2 = CreateBrowser(profile);
  SetIdleTimeoutPolicies(policy_provider(0), /*idle_timeout=*/5);

  // `profile2` has the policy set to 5 minutes, so it will close at the same
  // time as `profile`.
  Profile* profile2;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile2 = g_browser_process->profile_manager()->GetProfile(
        g_browser_process->profile_manager()->user_data_dir().AppendASCII(
            "Profile 2"));
    SetIdleTimeoutPolicies(policy_provider(1), /*idle_timeout=*/5);
  }
  Browser* browser3 = CreateBrowser(profile2);

  // `profile3` doesn't have the IdleTimeout policy set, so it will
  // never close.
  Profile* profile3;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile3 = g_browser_process->profile_manager()->GetProfile(
        g_browser_process->profile_manager()->user_data_dir().AppendASCII(
            "Profile 3"));
  }
  std::ignore = CreateBrowser(profile3);
  ActivateBrowser(browser());

  EXPECT_EQ(2, GetBrowserCount(profile));
  EXPECT_EQ(1, GetBrowserCount(profile2));
  EXPECT_EQ(1, GetBrowserCount(profile3));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 299s, does nothing.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(299)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(2, GetBrowserCount(profile));
  EXPECT_EQ(1, GetBrowserCount(profile2));
  EXPECT_EQ(1, GetBrowserCount(profile3));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 300s, threshold is reached. Close browsers, then show the Profile Picker.
  {
    BrowserCloseWaiter waiter({browser(), browser2, browser3});
    EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
        .WillOnce(Return(base::Seconds(300)));
    task_runner()->FastForwardBy(base::Seconds(1));

    EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
        .WillRepeatedly(Return(base::Seconds(315)));
    task_runner()->FastForwardBy(base::Seconds(30));
    waiter.Wait();
  }
  EXPECT_EQ(0, GetBrowserCount(profile));
  EXPECT_EQ(0, GetBrowserCount(profile2));
  EXPECT_EQ(1, GetBrowserCount(profile3));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

// TODO(crbug.com/40064501): Flaky on MacOS
#if BUILDFLAG(IS_MAC)
#define MAYBE_MultiProfileWithDifferentThresholds \
  DISABLED_MultiProfileWithDifferentThresholds
#else
#define MAYBE_MultiProfileWithDifferentThresholds \
  MultiProfileWithDifferentThresholds
#endif
IN_PROC_BROWSER_TEST_F(IdleServiceTest,
                       MAYBE_MultiProfileWithDifferentThresholds) {
  // `profile` has the IdleTimeout policy set to 5 minutes.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(299)));
  Profile* profile = browser()->profile();
  Browser* browser2 = CreateBrowser(profile);
  SetIdleTimeoutPolicies(policy_provider(0), /*idle_timeout=*/5);

  // `profile2` has the policy set to 6 minutes, so it will close one minute
  // *after* `profile`.
  Profile* profile2;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    profile2 = g_browser_process->profile_manager()->GetProfile(
        g_browser_process->profile_manager()->user_data_dir().AppendASCII(
            "Profile 2"));
    SetIdleTimeoutPolicies(policy_provider(1), /*idle_timeout=*/6);
  }
  Browser* browser3 = CreateBrowser(profile2);

  EXPECT_EQ(2, GetBrowserCount(profile));
  EXPECT_EQ(1, GetBrowserCount(profile2));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 299s, does nothing.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(299)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(2, GetBrowserCount(profile));
  EXPECT_EQ(1, GetBrowserCount(profile2));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 300s, threshold is reached for `profile`. Close its browsers, then show the
  // Profile Picker.
  {
    BrowserCloseWaiter waiter({browser(), browser2});
    EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
        .WillOnce(Return(base::Seconds(300)));
    task_runner()->FastForwardBy(base::Seconds(1));
    EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
        .WillRepeatedly(Return(base::Seconds(315)));
    task_runner()->FastForwardBy(base::Seconds(30));
    waiter.Wait();
  }
  EXPECT_EQ(0, GetBrowserCount(profile));
  EXPECT_EQ(1, GetBrowserCount(profile2));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_TRUE(ProfilePicker::IsOpen());

  // 360s, threshold is reached for `profile2`. Close its browsers.
  {
    BrowserCloseWaiter waiter({browser3});
    EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
        .WillOnce(Return(base::Seconds(360)));
    task_runner()->FastForwardBy(base::Seconds(1));
    EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
        .WillRepeatedly(Return(base::Seconds(375)));
    task_runner()->FastForwardBy(base::Seconds(30));
    waiter.Wait();
  }
  EXPECT_EQ(0, GetBrowserCount(profile));
  EXPECT_EQ(0, GetBrowserCount(profile2));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_TRUE(ProfilePicker::IsOpen());
}

IN_PROC_BROWSER_TEST_F(IdleServiceTest, DialogDismissedByUser) {
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(58)));
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  Profile* profile = browser()->profile();
  SetIdleTimeoutPolicies(policy_provider(0), /*idle_timeout=*/1);

  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 59s, does nothing.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(59)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 60s, threshold is reached. Close browsers, then show the Profile
  // Picker.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));

  DialogManager::GetInstance()->DismissDialogForTesting();

  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillRepeatedly(Return(base::Seconds(75)));
  task_runner()->FastForwardBy(base::Seconds(30));
  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());
  // Check that the idle dialog events are recorded in the histogram.
  EXPECT_THAT(
      histogram_tester->GetAllSamples(
          "Enterprise.IdleTimeoutPolicies.IdleTimeoutDialogEvent"),
      ElementsAre(
          base::Bucket(metrics::IdleTimeoutDialogEvent::kDialogShown, 1),
          base::Bucket(metrics::IdleTimeoutDialogEvent::kDialogDismissedByUser,
                       1)));
}

IN_PROC_BROWSER_TEST_F(IdleServiceTest, NoActions) {
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(58)));
  Profile* profile = browser()->profile();
  SetIdleTimeoutPolicies(policy_provider(0), /*idle_timeout=*/1,
                         /*idle_timeout_actions=*/{});

  base::Value::List actions;
  profile->GetPrefs()->SetList(prefs::kIdleTimeoutActions, std::move(actions));

  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 60s, threshold is reached. This should not show the dialog, because there
  // are no actions.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillRepeatedly(Return(base::Seconds(15)));
  task_runner()->FastForwardBy(base::Seconds(30));

  // Nothing happened: no browsers closed, no Profile Picker.
  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());
}

IN_PROC_BROWSER_TEST_F(IdleServiceTest, JustCloseBrowsers) {
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(58)));
  Profile* profile = browser()->profile();
  SetIdleTimeoutPolicies(policy_provider(0), /*idle_timeout=*/1,
                         /*idle_timeout_actions=*/{"close_browsers"});

  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kCloseBrowsers));
  profile->GetPrefs()->SetList(prefs::kIdleTimeoutActions, std::move(actions));

  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 60s, threshold is reached. This should show the dialog.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_TRUE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillRepeatedly(Return(base::Seconds(15)));
  BrowserCloseWaiter waiter({browser()});
  task_runner()->FastForwardBy(base::Seconds(30));
  waiter.Wait();
  EXPECT_EQ(0, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());

  // Profile Picker didn't show.
  EXPECT_FALSE(ProfilePicker::IsOpen());
}

IN_PROC_BROWSER_TEST_F(IdleServiceTest, JustShowProfilePicker) {
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(58)));
  Profile* profile = browser()->profile();
  SetIdleTimeoutPolicies(policy_provider(0), /*idle_timeout=*/1,
                         /*idle_timeout_actions=*/{"show_profile_picker"});

  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kShowProfilePicker));
  profile->GetPrefs()->SetList(prefs::kIdleTimeoutActions, std::move(actions));

  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 60s, threshold is reached. This should show NOT show the dialog, which is
  // tied to the "close_browsers" action.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  EXPECT_EQ(1, GetBrowserCount(profile));
}

IN_PROC_BROWSER_TEST_F(IdleServiceTest, ReloadPages) {
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(58)));
  Profile* profile = browser()->profile();
  SetIdleTimeoutPolicies(policy_provider(0), /*idle_timeout=*/1,
                         /*idle_timeout_actions=*/{"reload_pages"});

  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_NE(nullptr, web_contents);

  // This callback should run after a navigation happens.
  base::MockCallback<base::RepeatingCallback<void(content::NavigationHandle*)>>
      cb;
  base::RunLoop run_loop;
  EXPECT_CALL(cb, Run(_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  content::DidFinishNavigationObserver observer(web_contents, cb.Get());

  // 60s, threshold is reached. This should show NOT show the dialog, which is
  // tied to the "close_browsers" action.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillRepeatedly(Return(base::Seconds(15)));
  task_runner()->FastForwardBy(base::Seconds(30));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(ProfilePicker::IsOpen());
  run_loop.Run();

  // Browsers are still open, bubble is visible.
  EXPECT_EQ(1, GetBrowserCount(profile));
}

IN_PROC_BROWSER_TEST_F(IdleServiceTest, ShowBubbleImmediately) {
  browser()->window()->Activate();
  BrowserList::SetLastActive(browser());

  // Use "reload_pages" as our action, because:
  // - It runs synchronously (succeeds immediately).
  // - It doesn't close browsers.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(58)));
  Profile* profile = browser()->profile();
  SetIdleTimeoutPolicies(policy_provider(0), /*idle_timeout=*/1,
                         /*idle_timeout_actions=*/{"reload_pages"});

  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 60s, threshold is reached. This should show NOT show the dialog, which is
  // tied to the "close_browsers" action.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillRepeatedly(Return(base::Seconds(15)));
  task_runner()->FastForwardBy(base::Seconds(30));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(ProfilePicker::IsOpen());
  ASSERT_EQ(1, GetBrowserCount(profile));

  // Bring the browser back into focus.
  ActivateBrowser(browser());
  // Bubble should be visible on that browser.
  EXPECT_TRUE(GetIdleBubble(browser()));
}

// It should be possible to focus the bubble with the "focus dialog" hotkey
// combination (Alt+Shift+A).
// TODO(crbug.com/40856612): Probably flaky on macOS.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CanFocusBubbleWithFocusDialogHotkey \
  DISABLED_CanFocusBubbleWithFocusDialogHotkey
#else
#define MAYBE_CanFocusBubbleWithFocusDialogHotkey \
  CanFocusBubbleWithFocusDialogHotkey
#endif
IN_PROC_BROWSER_TEST_F(IdleServiceTest,
                       MAYBE_CanFocusBubbleWithFocusDialogHotkey) {
  browser()->window()->Activate();
  BrowserList::SetLastActive(browser());

  // Use "reload_pages" as our action, because:
  // - It runs synchronously (succeeds immediately).
  // - It doesn't close browsers.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(58)));
  Profile* profile = browser()->profile();
  SetIdleTimeoutPolicies(policy_provider(0), /*idle_timeout=*/1,
                         /*idle_timeout_actions=*/{"reload_pages"});

  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 60s, threshold is reached. This should show NOT show the dialog, which is
  // tied to the "close_browsers" action.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillRepeatedly(Return(base::Seconds(15)));
  task_runner()->FastForwardBy(base::Seconds(30));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(ProfilePicker::IsOpen());
  ASSERT_EQ(1, GetBrowserCount(profile));

  // Bring the browser back into focus.
  ActivateBrowser(browser());

  // Bubble should be visible on that browser, and initially inactive.
  views::View* bubble = GetIdleBubble(browser());
  EXPECT_TRUE(bubble);
  EXPECT_FALSE(bubble->GetWidget()->IsActive());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  browser_view->FocusInactivePopupForAccessibility();
  EXPECT_TRUE(bubble->GetWidget()->IsActive());
}

// It should be possible to focus the bubble with the "rotate pane focus" (F6)
// hotkey.
// TODO(crbug.com/40852599): Probably flaky on macOS.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CanFocusBubbleWithRotatePaneFocusHotkey \
  DISABLED_CanFocusBubbleWithRotatePaneFocusHotkey
#else
#define MAYBE_CanFocusBubbleWithRotatePaneFocusHotkey \
  CanFocusBubbleWithRotatePaneFocusHotkey
#endif
IN_PROC_BROWSER_TEST_F(IdleServiceTest,
                       MAYBE_CanFocusBubbleWithRotatePaneFocusHotkey) {
  browser()->window()->Activate();
  BrowserList::SetLastActive(browser());

  // Use "reload_pages" as our action, because:
  // - It runs synchronously (succeeds immediately).
  // - It doesn't close browsers.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(58)));
  Profile* profile = browser()->profile();
  SetIdleTimeoutPolicies(policy_provider(0), /*idle_timeout=*/1,
                         /*idle_timeout_actions=*/{"reload_pages"});

  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));
  EXPECT_FALSE(ProfilePicker::IsOpen());

  // 60s, threshold is reached. This should show NOT show the dialog, which is
  // tied to the "close_browsers" action.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  task_runner()->FastForwardBy(base::Seconds(1));
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillRepeatedly(Return(base::Seconds(15)));
  task_runner()->FastForwardBy(base::Seconds(30));
  EXPECT_FALSE(IsDialogOpen());
  EXPECT_FALSE(ProfilePicker::IsOpen());
  ASSERT_EQ(1, GetBrowserCount(profile));

  // Bring the browser back into focus.
  ActivateBrowser(browser());

  // Bubble should be visible on that browser, and initially inactive.
  views::View* bubble = GetIdleBubble(browser());
  EXPECT_TRUE(bubble);
  EXPECT_FALSE(bubble->GetWidget()->IsActive());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  browser_view->RotatePaneFocus(true);
  // Rotate pane focus is expected to keep the bubble focused until the user
  // deals with it, so a second call should have no effect.
  browser_view->RotatePaneFocus(true);
  EXPECT_TRUE(bubble->GetWidget()->IsActive());
}

IN_PROC_BROWSER_TEST_F(IdleServiceTest, PRE_ShowBubbleOnStartup) {
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(58)));
  Profile* profile = browser()->profile();
  SetIdleTimeoutPolicies(policy_provider(0), /*idle_timeout=*/1);

  EXPECT_EQ(1, GetBrowserCount(profile));
  EXPECT_FALSE(GetIdleBubble(browser()));

  // 60s, threshold is reached. This should show the dialog.
  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillOnce(Return(base::Seconds(60)));
  task_runner()->FastForwardBy(base::Seconds(1));
  ASSERT_EQ(1, GetBrowserCount(profile));
  ASSERT_TRUE(IsDialogOpen());
  EXPECT_FALSE(GetIdleBubble(browser()));

  EXPECT_CALL(idle_time_provider(), CalculateIdleTime())
      .WillRepeatedly(Return(base::Seconds(15)));
  BrowserCloseWaiter waiter({browser()});
  task_runner()->FastForwardBy(base::Seconds(30));
  waiter.Wait();
  ASSERT_EQ(0, GetBrowserCount(profile));

  // No bubble visible, since there are no browser open. It will appear on next
  // startup.
}

IN_PROC_BROWSER_TEST_F(IdleServiceTest, ShowBubbleOnStartup) {
  // The bubble is visible, from the last browsing session.
  SetIdleTimeoutPolicies(policy_provider(0), /*idle_timeout=*/1);
  ActivateBrowser(browser());
  EXPECT_TRUE(GetIdleBubble(browser()));
}

}  // namespace enterprise_idle
