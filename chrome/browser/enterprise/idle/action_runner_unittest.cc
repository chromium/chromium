// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/action_runner.h"

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/enterprise/idle/action.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/idle/idle_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/test/views/chrome_views_test_base.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace enterprise_idle {

using content::BrowsingDataRemover;

using testing::_;
using testing::IsEmpty;
using testing::Return;
using testing::UnorderedElementsAre;

namespace {

class FakeActionFactory : public ActionFactory {
 public:
  FakeActionFactory() = default;

  ActionQueue Build(Profile* profile,
                    const std::vector<ActionType>& action_types) override {
    ActionQueue actions;
    for (ActionType action_type : action_types) {
      auto it = associations_.find(action_type);
      if (it != associations_.end()) {
        actions.push(std::move(it->second));
        associations_.erase(it);
      }
    }
    return actions;
  }

  void Associate(ActionType action_type, std::unique_ptr<Action> action) {
    associations_[action_type] = std::move(action);
  }

 private:
  std::map<ActionType, std::unique_ptr<Action>> associations_;
};

class MockAction : public Action {
 public:
  explicit MockAction(ActionType action_type)
      : Action(static_cast<int>(action_type)) {}

  MOCK_METHOD2(Run, void(Profile*, Continuation));
  bool ShouldNotifyUserOfPendingDestructiveAction(Profile* profile) override {
    return false;
  }
};

// testing::InvokeArgument<N> does not work with base::OnceCallback, so we
// define our own gMock action to run the 2nd argument.
ACTION_P(RunContinuation, success) {
  std::move(const_cast<Action::Continuation&>(arg1)).Run(success);
}

}  // namespace

// TODO(crbug.com/40222234): Enable this when Android supports >1 Action.
#if !BUILDFLAG(IS_ANDROID)
// Tests that actions are run in sequence, in order of priority.
TEST(IdleActionRunnerTest, RunsActionsInSequence) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeActionFactory action_factory;
  ActionRunner runner(&profile, &action_factory);

  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kCloseBrowsers));
  actions.Append(static_cast<int>(ActionType::kShowProfilePicker));
  profile.GetPrefs()->SetList(prefs::kIdleTimeoutActions, std::move(actions));

  auto close_browsers =
      std::make_unique<MockAction>(ActionType::kCloseBrowsers);
  auto show_profile_picker =
      std::make_unique<MockAction>(ActionType::kShowProfilePicker);
  testing::InSequence in_sequence;
  EXPECT_CALL(*close_browsers, Run(&profile, _))
      .WillOnce(RunContinuation(true));
  EXPECT_CALL(*show_profile_picker, Run(&profile, _))
      .WillOnce(RunContinuation(true));

  action_factory.Associate(ActionType::kCloseBrowsers,
                           std::move(close_browsers));
  action_factory.Associate(ActionType::kShowProfilePicker,
                           std::move(show_profile_picker));
  runner.Run();
}

// Tests that the order of actions in the pref doesn't matter. They still run
// by order of priority.
TEST(IdleActionRunnerTest, PrefOrderDoesNotMatter) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeActionFactory action_factory;
  ActionRunner runner(&profile, &action_factory);

  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kCloseBrowsers));
  actions.Append(static_cast<int>(ActionType::kShowProfilePicker));
  profile.GetPrefs()->SetList(prefs::kIdleTimeoutActions, std::move(actions));

  auto close_browsers =
      std::make_unique<MockAction>(ActionType::kCloseBrowsers);
  auto show_profile_picker =
      std::make_unique<MockAction>(ActionType::kShowProfilePicker);
  testing::InSequence in_sequence;
  EXPECT_CALL(*close_browsers, Run(&profile, _))
      .WillOnce(RunContinuation(true));
  EXPECT_CALL(*show_profile_picker, Run(&profile, _))
      .WillOnce(RunContinuation(true));

  action_factory.Associate(ActionType::kCloseBrowsers,
                           std::move(close_browsers));
  action_factory.Associate(ActionType::kShowProfilePicker,
                           std::move(show_profile_picker));

  runner.Run();
}

// Tests that when a higher-priority action fails, the lower-priority actions
// don't run.
TEST(IdleActionRunnerTest, OtherActionsDontRunOnFailure) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeActionFactory action_factory;
  ActionRunner runner(&profile, &action_factory);

  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kCloseBrowsers));
  actions.Append(static_cast<int>(ActionType::kShowProfilePicker));
  profile.GetPrefs()->SetList(prefs::kIdleTimeoutActions, std::move(actions));

  auto close_browsers =
      std::make_unique<MockAction>(ActionType::kCloseBrowsers);
  auto show_profile_picker =
      std::make_unique<MockAction>(ActionType::kShowProfilePicker);

  // "show_profile_picker" shouldn't run, because "close_browsers" fails.
  testing::InSequence in_sequence;
  EXPECT_CALL(*close_browsers, Run(&profile, _))
      .WillOnce(RunContinuation(false));
  EXPECT_CALL(*show_profile_picker, Run(_, _)).Times(0);

  action_factory.Associate(ActionType::kCloseBrowsers,
                           std::move(close_browsers));
  action_factory.Associate(ActionType::kShowProfilePicker,
                           std::move(show_profile_picker));
  runner.Run();
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Tests that it does nothing when the "IdleTimeoutActions" pref is empty.
TEST(IdleActionRunnerTest, DoNothingWithEmptyPref) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeActionFactory action_factory;
  ActionRunner runner(&profile, &action_factory);

  // "IdleTimeoutActions" is deliberately unset.
  auto clear_browsing_history =
      std::make_unique<MockAction>(ActionType::kClearBrowsingHistory);
  auto clear_cookies_and_site_data =
      std::make_unique<MockAction>(ActionType::kClearCookiesAndOtherSiteData);

  EXPECT_CALL(*clear_browsing_history, Run(_, _)).Times(0);
  EXPECT_CALL(*clear_cookies_and_site_data, Run(_, _)).Times(0);

  action_factory.Associate(ActionType::kClearBrowsingHistory,
                           std::move(clear_browsing_history));
  action_factory.Associate(ActionType::kClearCookiesAndOtherSiteData,
                           std::move(clear_cookies_and_site_data));
  runner.Run();
}

// TODO(crbug.com/40222234): Enable this when Android supports >1 Action.
#if !BUILDFLAG(IS_ANDROID)
// Tests that ActionRunner only runs the actions configured via the
// "IdleTimeoutActions" pref.
TEST(IdleActionRunnerTest, JustCloseBrowsers) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeActionFactory action_factory;
  ActionRunner runner(&profile, &action_factory);

  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kCloseBrowsers));
  profile.GetPrefs()->SetList(prefs::kIdleTimeoutActions, std::move(actions));

  auto close_browsers =
      std::make_unique<MockAction>(ActionType::kCloseBrowsers);
  auto show_profile_picker =
      std::make_unique<MockAction>(ActionType::kShowProfilePicker);

  EXPECT_CALL(*close_browsers, Run(&profile, _))
      .WillOnce(RunContinuation(true));
  EXPECT_CALL(*show_profile_picker, Run(_, _)).Times(0);

  action_factory.Associate(ActionType::kCloseBrowsers,
                           std::move(close_browsers));
  action_factory.Associate(ActionType::kShowProfilePicker,
                           std::move(show_profile_picker));
  runner.Run();
}

// Tests that ActionRunner only runs the actions configured via the
// "IdleTimeoutActions" pref.
TEST(IdleActionRunnerTest, JustShowProfilePicker) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  FakeActionFactory action_factory;
  ActionRunner runner(&profile, &action_factory);

  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kShowProfilePicker));
  profile.GetPrefs()->SetList(prefs::kIdleTimeoutActions, std::move(actions));

  auto close_browsers =
      std::make_unique<MockAction>(ActionType::kCloseBrowsers);
  auto show_profile_picker =
      std::make_unique<MockAction>(ActionType::kShowProfilePicker);

  EXPECT_CALL(*close_browsers, Run(_, _)).Times(0);
  EXPECT_CALL(*show_profile_picker, Run(&profile, _))
      .WillOnce(RunContinuation(true));

  action_factory.Associate(ActionType::kCloseBrowsers,
                           std::move(close_browsers));
  action_factory.Associate(ActionType::kShowProfilePicker,
                           std::move(show_profile_picker));
  runner.Run();
}
#endif  // !BUILDFLAG(IS_ANDROID)

// A basic implementation of BrowsingDataRemover, that doesn't remove any data.
//
// Doesn't use gmock methods, since we could technically accept any of the 4
// Remove*() methods. Instead, it saves the arguments for BrowsingDataRemover's
class FakeBrowsingDataRemover : public BrowsingDataRemover {
 public:
  void SetEmbedderDelegate(
      content::BrowsingDataRemoverDelegate* embedder_delegate) override {
    NOTREACHED_IN_MIGRATION();
  }
  bool DoesOriginMatchMaskForTesting(
      uint64_t origin_type_mask,
      const url::Origin& origin,
      storage::SpecialStoragePolicy* special_storage_policy) override {
    NOTREACHED_IN_MIGRATION();
    return true;
  }
  void Remove(const base::Time& delete_begin,
              const base::Time& delete_end,
              uint64_t remove_mask,
              uint64_t origin_type_mask) override {
    DCHECK_EQ(nullptr, observer_);
    remove_mask_ = remove_mask;
    origin_type_mask_ = origin_type_mask;
  }
  void RemoveWithFilter(const base::Time& delete_begin,
                        const base::Time& delete_end,
                        uint64_t remove_mask,
                        uint64_t origin_type_mask,
                        std::unique_ptr<content::BrowsingDataFilterBuilder>
                            filter_builder) override {
    Remove(delete_begin, delete_end, remove_mask, origin_type_mask);
  }
  void RemoveAndReply(const base::Time& delete_begin,
                      const base::Time& delete_end,
                      uint64_t remove_mask,
                      uint64_t origin_type_mask,
                      Observer* observer) override {
    DCHECK_EQ(observer, observer_);
    remove_mask_ = remove_mask;
    origin_type_mask_ = origin_type_mask;
    observer->OnBrowsingDataRemoverDone(failed_data_types_);
  }
  void RemoveWithFilterAndReply(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      uint64_t remove_mask,
      uint64_t origin_type_mask,
      std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder,
      Observer* observer) override {
    RemoveAndReply(delete_begin, delete_end, remove_mask, origin_type_mask,
                   observer);
  }
  void AddObserver(Observer* observer) override {
    DCHECK_EQ(nullptr, observer_);
    observer_ = observer;
  }
  void RemoveObserver(Observer* observer) override {
    DCHECK_EQ(observer, observer_);
    observer_ = nullptr;
  }
  void SetWouldCompleteCallbackForTesting(
      const base::RepeatingCallback<
          void(base::OnceClosure continue_to_completion)>& callback) override {
    NOTREACHED_IN_MIGRATION();
  }
  const base::Time& GetLastUsedBeginTimeForTesting() override {
    NOTREACHED_IN_MIGRATION();
    return begin_time_;
  }
  uint64_t GetLastUsedRemovalMaskForTesting() override { return remove_mask_; }
  uint64_t GetLastUsedOriginTypeMaskForTesting() override {
    return origin_type_mask_;
  }
  std::optional<content::StoragePartitionConfig>
  GetLastUsedStoragePartitionConfigForTesting() override {
    return std::nullopt;
  }
  uint64_t GetPendingTaskCountForTesting() override { return 0; }

  void SetFailedDataTypesForTesting(uint64_t failed_data_types) {
    failed_data_types_ = failed_data_types;
  }

 private:
  base::Time begin_time_;
  uint64_t remove_mask_ = 0;
  uint64_t origin_type_mask_ = 0;
  uint64_t failed_data_types_ = 0;

  raw_ptr<Observer> observer_ = nullptr;
};

#if !BUILDFLAG(IS_ANDROID)
class IdleActionRunnerClearDataTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    set_native_widget_type(NativeWidgetType::kDesktop);
    ChromeViewsTestBase::SetUp();
  }

  void TearDown() override { ChromeViewsTestBase::TearDown(); }

  TestingProfile* profile() { return &profile_; }
  FakeBrowsingDataRemover* remover() { return &browsing_data_remover_; }

 private:
  TestingProfile profile_;
  FakeBrowsingDataRemover browsing_data_remover_;
};

TEST_F(IdleActionRunnerClearDataTest, ClearBrowsingHistory) {
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kClearBrowsingHistory));
  profile()->GetPrefs()->SetList(prefs::kIdleTimeoutActions,
                                 std::move(actions));

  ActionFactory::GetInstance()->SetBrowsingDataRemoverForTesting(remover());
  ActionRunner runner(profile(), ActionFactory::GetInstance());
  runner.Run();
  task_environment()->FastForwardBy(base::Seconds(30));
  EXPECT_EQ(chrome_browsing_data_remover::DATA_TYPE_HISTORY,
            remover()->GetLastUsedRemovalMaskForTesting());
  histogram_tester->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", true, 1);
}

TEST_F(IdleActionRunnerClearDataTest, ClearDownloadHistory) {
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kClearDownloadHistory));
  profile()->GetPrefs()->SetList(prefs::kIdleTimeoutActions,
                                 std::move(actions));

  ActionFactory::GetInstance()->SetBrowsingDataRemoverForTesting(remover());
  ActionRunner runner(profile(), ActionFactory::GetInstance());
  runner.Run();
  task_environment()->FastForwardBy(base::Seconds(30));

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
            remover()->GetLastUsedRemovalMaskForTesting());
  histogram_tester->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", true, 1);
}

TEST_F(IdleActionRunnerClearDataTest, ClearCookies) {
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kClearCookiesAndOtherSiteData));
  profile()->GetPrefs()->SetList(prefs::kIdleTimeoutActions,
                                 std::move(actions));

  ActionFactory::GetInstance()->SetBrowsingDataRemoverForTesting(remover());
  ActionRunner runner(profile(), ActionFactory::GetInstance());
  runner.Run();
  task_environment()->FastForwardBy(base::Seconds(30));

  EXPECT_EQ(chrome_browsing_data_remover::DATA_TYPE_SITE_DATA,
            remover()->GetLastUsedRemovalMaskForTesting());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
            remover()->GetLastUsedOriginTypeMaskForTesting());
  histogram_tester->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", true, 1);
}

TEST_F(IdleActionRunnerClearDataTest, ClearCache) {
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kClearCachedImagesAndFiles));
  profile()->GetPrefs()->SetList(prefs::kIdleTimeoutActions,
                                 std::move(actions));

  ActionFactory::GetInstance()->SetBrowsingDataRemoverForTesting(remover());
  ActionRunner runner(profile(), ActionFactory::GetInstance());
  runner.Run();
  task_environment()->FastForwardBy(base::Seconds(30));

  EXPECT_EQ(BrowsingDataRemover::DATA_TYPE_CACHE,
            remover()->GetLastUsedRemovalMaskForTesting());
  histogram_tester->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", true, 1);
}

TEST_F(IdleActionRunnerClearDataTest, ClearPasswordSignin) {
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kClearPasswordSignin));
  profile()->GetPrefs()->SetList(prefs::kIdleTimeoutActions,
                                 std::move(actions));

  ActionFactory::GetInstance()->SetBrowsingDataRemoverForTesting(remover());
  ActionRunner runner(profile(), ActionFactory::GetInstance());
  runner.Run();
  task_environment()->FastForwardBy(base::Seconds(30));

  EXPECT_EQ(chrome_browsing_data_remover::DATA_TYPE_PASSWORDS,
            remover()->GetLastUsedRemovalMaskForTesting());
  histogram_tester->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", true, 1);
}

TEST_F(IdleActionRunnerClearDataTest, ClearAutofill) {
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kClearAutofill));
  profile()->GetPrefs()->SetList(prefs::kIdleTimeoutActions,
                                 std::move(actions));

  ActionFactory::GetInstance()->SetBrowsingDataRemoverForTesting(remover());
  ActionRunner runner(profile(), ActionFactory::GetInstance());
  runner.Run();
  task_environment()->FastForwardBy(base::Seconds(30));

  EXPECT_EQ(chrome_browsing_data_remover::DATA_TYPE_FORM_DATA,
            remover()->GetLastUsedRemovalMaskForTesting());
  histogram_tester->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", true, 1);
}

TEST_F(IdleActionRunnerClearDataTest, ClearSiteSettings) {
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kClearSiteSettings));
  profile()->GetPrefs()->SetList(prefs::kIdleTimeoutActions,
                                 std::move(actions));

  ActionFactory::GetInstance()->SetBrowsingDataRemoverForTesting(remover());
  ActionRunner runner(profile(), ActionFactory::GetInstance());
  runner.Run();
  task_environment()->FastForwardBy(base::Seconds(30));

  EXPECT_EQ(chrome_browsing_data_remover::DATA_TYPE_CONTENT_SETTINGS,
            remover()->GetLastUsedRemovalMaskForTesting());
  histogram_tester->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", true, 1);
}

TEST_F(IdleActionRunnerClearDataTest, ClearHostedAppData) {
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kClearHostedAppData));
  profile()->GetPrefs()->SetList(prefs::kIdleTimeoutActions,
                                 std::move(actions));

  ActionFactory::GetInstance()->SetBrowsingDataRemoverForTesting(remover());
  ActionRunner runner(profile(), ActionFactory::GetInstance());
  runner.Run();
  task_environment()->FastForwardBy(base::Seconds(30));

  EXPECT_EQ(chrome_browsing_data_remover::DATA_TYPE_SITE_DATA,
            remover()->GetLastUsedRemovalMaskForTesting());
  EXPECT_EQ(BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
            remover()->GetLastUsedOriginTypeMaskForTesting());
  histogram_tester->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", true, 1);
}

TEST_F(IdleActionRunnerClearDataTest, MultipleTypes) {
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  base::Value::List actions;
  actions.Append(static_cast<int>(ActionType::kClearBrowsingHistory));
  actions.Append(static_cast<int>(ActionType::kClearDownloadHistory));
  actions.Append(static_cast<int>(ActionType::kClearAutofill));
  profile()->GetPrefs()->SetList(prefs::kIdleTimeoutActions,
                                 std::move(actions));

  ActionFactory::GetInstance()->SetBrowsingDataRemoverForTesting(remover());
  ActionRunner runner(profile(), ActionFactory::GetInstance());
  runner.Run();
  task_environment()->FastForwardBy(base::Seconds(30));

  EXPECT_EQ(chrome_browsing_data_remover::DATA_TYPE_HISTORY |
                BrowsingDataRemover::DATA_TYPE_DOWNLOADS |
                chrome_browsing_data_remover::DATA_TYPE_FORM_DATA,
            remover()->GetLastUsedRemovalMaskForTesting());
  histogram_tester->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", true, 1);
}

TEST_F(IdleActionRunnerClearDataTest, MultipleTypesAndFailure) {
  std::unique_ptr<base::HistogramTester> histogram_tester =
      std::make_unique<base::HistogramTester>();
  remover()->SetFailedDataTypesForTesting(
      chrome_browsing_data_remover::DATA_TYPE_HISTORY |
      BrowsingDataRemover::DATA_TYPE_DOWNLOADS |
      chrome_browsing_data_remover::DATA_TYPE_FORM_DATA);

  ActionFactory::GetInstance()->SetBrowsingDataRemoverForTesting(remover());
  ActionFactory::ActionQueue actions = ActionFactory::GetInstance()->Build(
      profile(),
      {ActionType::kClearBrowsingHistory, ActionType::kClearDownloadHistory,
       ActionType::kClearAutofill});
  ASSERT_EQ(1u, actions.size());
  EXPECT_EQ(static_cast<int>(ActionType::kClearBrowsingHistory),
            actions.top()->priority());

  // The callback should run with success=false.
  base::MockCallback<Action::Continuation> cb;
  base::RunLoop run_loop;
  EXPECT_CALL(cb, Run(/*success=*/false))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  actions.top()->Run(profile(), cb.Get());
  run_loop.Run();
  histogram_tester->ExpectUniqueSample(
      "Enterprise.IdleTimeoutPolicies.Success.ClearBrowsingData", false, 1);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace enterprise_idle
