// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_service.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/finds/core/finds_features.h"
#include "chrome/browser/finds/core/finds_pref_names.h"
#include "chrome/browser/finds/core/finds_utils.h"
#include "chrome/browser/notifications/scheduler/public/notification_entry.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_constant.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_schedule_service.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/proto/features/finds.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using SuggestionTheme =
    optimization_guide::proto::FindsSuggestionResponse::SuggestionTheme;

namespace history {
class MockHistoryService : public HistoryService {
 public:
  MockHistoryService() = default;
  ~MockHistoryService() override = default;

  MOCK_METHOD(base::CancelableTaskTracker::TaskId,
              QueryHistory,
              (const std::u16string& text_query,
               const QueryOptions& options,
               QueryHistoryCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
};
}  // namespace history

namespace finds {

class MockFindsServiceObserver : public FindsService::Observer {
 public:
  MOCK_METHOD(void, OnOptInCriteriaFulfilled, (), (override));
};

class FindsServiceTest : public testing::Test {
 public:
  FindsServiceTest() = default;
  ~FindsServiceTest() override = default;

  void SetUp() override {
    optimization_guide::model_execution::prefs::RegisterProfilePrefs(
        prefs_.registry());
    FindsService::RegisterProfilePrefs(prefs_.registry());
    prefs_.registry()->RegisterBooleanPref(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        false);
    prefs_.SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
    opt_guide_service_ = std::make_unique<
        testing::NiceMock<MockOptimizationGuideKeyedService>>();
    history_service_ =
        std::make_unique<testing::NiceMock<history::MockHistoryService>>();
    notification_schedule_service_ = std::make_unique<testing::NiceMock<
        notifications::test::MockNotificationScheduleService>>();
    service_ = std::make_unique<FindsService>(
        opt_guide_service_.get(), history_service_.get(), &prefs_,
        notification_schedule_service_.get(), &sync_service_);
  }

 protected:
  TestingPrefServiceSimple prefs_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<MockOptimizationGuideKeyedService> opt_guide_service_;
  std::unique_ptr<history::MockHistoryService> history_service_;
  std::unique_ptr<notifications::test::MockNotificationScheduleService>
      notification_schedule_service_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<FindsService> service_;
  base::HistogramTester histogram_tester_;

  const base::flat_map<optimization_guide::proto::FindsMetadata::ThemeType,
                       int>&
  theme_url_visit_count() const {
    return service_->theme_url_visit_count_;
  }
};

TEST_F(FindsServiceTest, VerifyNotificationCooldownPref) {
  EXPECT_EQ(0, prefs_.GetInt64(prefs::kFindsModelExecutionLastTimestamp));
  finds::MarkModelExecutionLastTimestamp(&prefs_);
  EXPECT_NE(0, prefs_.GetInt64(prefs::kFindsModelExecutionLastTimestamp));
}

TEST_F(FindsServiceTest, VerifyThemeNotInterestedCooldownPref) {
  auto shopping_theme = optimization_guide::proto::FindsSuggestionResponse::
      SuggestionTheme::SHOPPING;
  EXPECT_TRUE(
      prefs_.GetDict(prefs::kFindsNotInterestedThemesLastTimestamp).empty());
  finds::MarkThemeAsNotInterested(&prefs_, shopping_theme);
  EXPECT_TRUE(prefs_.GetDict(prefs::kFindsNotInterestedThemesLastTimestamp)
                  .Find("Shopping"));
}

TEST_F(FindsServiceTest, DeleteNotificationsOnMSBBToggle) {
  EXPECT_CALL(
      *notification_schedule_service_,
      DeleteNotifications(notifications::SchedulerClientType::kChromeFinds))
      .Times(1);

  prefs_.SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
}

TEST_F(FindsServiceTest, DeleteNotificationsOnHistorySyncToggle) {
  EXPECT_CALL(
      *notification_schedule_service_,
      DeleteNotifications(notifications::SchedulerClientType::kChromeFinds))
      .Times(1);

  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, false);
  // TestSyncService requires explicit firing of observer events.
  sync_service_.FireStateChanged();
}

TEST_F(FindsServiceTest, HistoryServiceUnavailable) {
  auto service = std::make_unique<FindsService>(
      opt_guide_service_.get(), nullptr, &prefs_,
      notification_schedule_service_.get(), &sync_service_);

  bool callback_called = false;
  service->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kHistoryServiceUnavailable,
                  result.status);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
  histogram_tester_.ExpectUniqueSample(
      "Finds.Result", FindsService::Result::Status::kHistoryServiceUnavailable,
      1);
}

TEST_F(FindsServiceTest, OptimizationGuideUnavailable) {
  auto service = std::make_unique<FindsService>(
      nullptr, history_service_.get(), &prefs_,
      notification_schedule_service_.get(), &sync_service_);

  bool callback_called = false;
  service->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kOptimizationGuideUnavailable,
                  result.status);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
  histogram_tester_.ExpectUniqueSample(
      "Finds.Result",
      FindsService::Result::Status::kOptimizationGuideUnavailable, 1);
}

TEST_F(FindsServiceTest, HistorySyncDisabled) {
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, false);
  sync_service_.FireStateChanged();

  bool callback_called = false;
  service_->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kDisabledByHistorySyncOrMsbb,
                  result.status);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
  histogram_tester_.ExpectUniqueSample(
      "Finds.Result",
      FindsService::Result::Status::kDisabledByHistorySyncOrMsbb, 1);
}

TEST_F(FindsServiceTest, MSBBDisabled) {
  prefs_.SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);

  bool callback_called = false;
  service_->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kDisabledByHistorySyncOrMsbb,
                  result.status);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
  histogram_tester_.ExpectUniqueSample(
      "Finds.Result",
      FindsService::Result::Status::kDisabledByHistorySyncOrMsbb, 1);
}

TEST_F(FindsServiceTest, EmptyHistory) {
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        std::move(callback).Run(history::QueryResults());
        return base::CancelableTaskTracker::kBadTaskId;
      });

  bool callback_called = false;
  service_->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kEmptyHistory, result.status);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
  histogram_tester_.ExpectUniqueSample(
      "Finds.Result", FindsService::Result::Status::kEmptyHistory, 1);
}

TEST_F(FindsServiceTest, ModelExecutionFailed) {
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryResults results;
        std::vector<history::URLResult> urls;
        urls.emplace_back(GURL("https://example.com"), base::Time::Now());
        results.SetURLResults(std::move(urls));
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  EXPECT_CALL(*opt_guide_service_, ExecuteModel(_, _, _, _))
      .WillOnce([](optimization_guide::ModelBasedCapabilityKey feature,
                   const google::protobuf::MessageLite& request_metadata,
                   const optimization_guide::ModelExecutionOptions&
                       execution_options,
                   optimization_guide::
                       OptimizationGuideModelExecutionResultCallback callback) {
        optimization_guide::OptimizationGuideModelExecutionResult result;
        result.response = base::unexpected(
            optimization_guide::OptimizationGuideModelExecutionError::
                FromModelExecutionError(
                    optimization_guide::OptimizationGuideModelExecutionError::
                        ModelExecutionError::kGenericFailure));
        std::move(callback).Run(std::move(result), nullptr);
      });

  bool callback_called = false;
  service_->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kModelExecutionFailed,
                  result.status);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
  histogram_tester_.ExpectUniqueSample(
      "Finds.Result", FindsService::Result::Status::kModelExecutionFailed, 1);
}

TEST_F(FindsServiceTest, ParsingFailed) {
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryResults results;
        std::vector<history::URLResult> urls;
        urls.emplace_back(GURL("https://example.com"), base::Time::Now());
        results.SetURLResults(std::move(urls));
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  EXPECT_CALL(*opt_guide_service_, ExecuteModel(_, _, _, _))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& execution_options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::OptimizationGuideModelExecutionResult result;
            optimization_guide::proto::Any any;
            any.set_type_url("type.googleapis.com/not.a.match");
            result.response = any;
            std::move(callback).Run(std::move(result), nullptr);
          });

  bool callback_called = false;
  service_->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kResponseParsingFailed,
                  result.status);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
  histogram_tester_.ExpectUniqueSample(
      "Finds.Result", FindsService::Result::Status::kResponseParsingFailed, 1);
}

TEST_F(FindsServiceTest, Success) {
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryResults results;
        std::vector<history::URLResult> urls;
        urls.emplace_back(GURL("https://example.com"), base::Time::Now());
        results.SetURLResults(std::move(urls));
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  EXPECT_CALL(*opt_guide_service_, ExecuteModel(_, _, _, _))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& execution_options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::OptimizationGuideModelExecutionResult result;
            optimization_guide::proto::FindsSuggestionResponse response;
            auto* suggestion_theme = response.add_suggested_themes();
            suggestion_theme->set_theme_title("Shopping");
            suggestion_theme->set_theme_type(SuggestionTheme::SHOPPING);
            suggestion_theme->add_theme_suggested_contents();
            optimization_guide::proto::Any any;
            any.set_type_url(
                "type.googleapis.com/"
                "optimization_guide.proto.FindsSuggestionResponse");
            response.SerializeToString(any.mutable_value());
            result.response = any;
            std::move(callback).Run(std::move(result), nullptr);
          });

  bool callback_called = false;
  service_->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kSuccess, result.status);
        EXPECT_EQ("Theme: Shopping (Type: 4, Score: 0)\n  - : ",
                  result.message);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
  EXPECT_NE(0, prefs_.GetInt64(prefs::kFindsModelExecutionLastTimestamp));
  histogram_tester_.ExpectUniqueSample(
      "Finds.Result", FindsService::Result::Status::kSuccess, 1);
}

TEST_F(FindsServiceTest, ExecutionCooldownNotPassed) {
  // Set the last execution timestamp to now.
  finds::MarkModelExecutionLastTimestamp(&prefs_);

  // Fast forward time just before the cooldown is set to expire.
  task_environment_.FastForwardBy(base::Days(
      finds::features::kModelExecutionCooldownDurationInDays.Get() - 1));

  // Check that the history service is not called as the model is on cooldown.
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _)).Times(0);

  base::HistogramTester histogram_tester_local;

  service_->ExecuteModelAndScheduleNotification(base::DoNothing());

  histogram_tester_local.ExpectUniqueSample(
      "Finds.Result", FindsService::Result::Status::kModelExecutionOnCooldown,
      1);
}

TEST_F(FindsServiceTest, ExecutionCooldownPassed) {
  // Set the last execution timestamp to now.
  finds::MarkModelExecutionLastTimestamp(&prefs_);

  // Fast forward enough to pass the cooldown.
  task_environment_.FastForwardBy(
      base::Days(finds::features::kModelExecutionCooldownDurationInDays.Get()));

  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryResults results;
        std::vector<history::URLResult> urls;
        urls.emplace_back(GURL("https://example.com"), base::Time::Now());
        results.SetURLResults(std::move(urls));
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  EXPECT_CALL(*opt_guide_service_, ExecuteModel(_, _, _, _))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& execution_options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::OptimizationGuideModelExecutionResult result;
            optimization_guide::proto::FindsSuggestionResponse response;
            auto* suggestion_theme = response.add_suggested_themes();
            suggestion_theme->set_theme_title("Shopping");
            suggestion_theme->set_theme_type(SuggestionTheme::SHOPPING);
            suggestion_theme->add_theme_suggested_contents();
            optimization_guide::proto::Any any;
            any.set_type_url(
                "type.googleapis.com/"
                "optimization_guide.proto.FindsSuggestionResponse");
            response.SerializeToString(any.mutable_value());
            result.response = any;
            std::move(callback).Run(std::move(result), nullptr);
          });

  base::HistogramTester histogram_tester_local;

  service_->ExecuteModelAndScheduleNotification(base::DoNothing());

  histogram_tester_local.ExpectUniqueSample(
      "Finds.Result", FindsService::Result::Status::kSuccess, 1);
}

TEST_F(FindsServiceTest, VerifyHistoryLookbackIntervalWithFinchParam) {
  // Override the feature param to something non-default.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      finds::features::kChromeFinds,
      {{"model_execution_cooldown_duration_in_days", "14"}});

  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        // Assert that lookback interval is 14 days.
        EXPECT_EQ(options.begin_time, base::Time::Now() - base::Days(14));
        history::QueryResults results;
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  service_->ExecuteModelAndScheduleNotification(base::DoNothing());
}

TEST_F(FindsServiceTest, VerifyMaxHistoryEntriesWithFinchParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      finds::features::kChromeFinds, {{"max_history_entries", "50"}});

  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        EXPECT_EQ(options.max_count, 50);
        history::QueryResults results;
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  service_->ExecuteModelAndScheduleNotification(base::DoNothing());
}

TEST_F(FindsServiceTest, VerifyMaxHistoryEntriesDefault) {
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        EXPECT_EQ(options.max_count, 0);  // Default should be 0 (no limit)
        history::QueryResults results;
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  service_->ExecuteModelAndScheduleNotification(base::DoNothing());
}

TEST_F(FindsServiceTest, EmptyNotificationService) {
  auto service = std::make_unique<FindsService>(opt_guide_service_.get(),
                                                history_service_.get(), &prefs_,
                                                nullptr, &sync_service_);

  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryResults results;
        std::vector<history::URLResult> urls;
        urls.emplace_back(GURL("https://example.com"), base::Time::Now());
        results.SetURLResults(std::move(urls));
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  EXPECT_CALL(*opt_guide_service_, ExecuteModel(_, _, _, _))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& execution_options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::OptimizationGuideModelExecutionResult result;
            optimization_guide::proto::FindsSuggestionResponse response;
            auto* suggestion_theme = response.add_suggested_themes();
            suggestion_theme->set_theme_title("Shopping");
            suggestion_theme->set_theme_type(SuggestionTheme::SHOPPING);
            suggestion_theme->add_theme_suggested_contents();
            optimization_guide::proto::Any any;
            any.set_type_url(
                "type.googleapis.com/"
                "optimization_guide.proto.FindsSuggestionResponse");
            response.SerializeToString(any.mutable_value());
            result.response = any;
            std::move(callback).Run(std::move(result), nullptr);
          });

  bool callback_called = false;
  service->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kFailedToScheduleNotification,
                  result.status);
        EXPECT_EQ("Could not schedule notification.", result.message);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("Finds.Result"),
      testing::ElementsAre(base::Bucket(
          FindsService::Result::Status::kFailedToScheduleNotification, 1)));
}

TEST_F(FindsServiceTest, NoThemesFound) {
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryResults results;
        std::vector<history::URLResult> urls;
        urls.emplace_back(GURL("https://example.com"), base::Time::Now());
        results.SetURLResults(std::move(urls));
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  EXPECT_CALL(*opt_guide_service_, ExecuteModel(_, _, _, _))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& execution_options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::OptimizationGuideModelExecutionResult result;
            optimization_guide::proto::FindsSuggestionResponse response;
            optimization_guide::proto::Any any;
            any.set_type_url(
                "type.googleapis.com/"
                "optimization_guide.proto.FindsSuggestionResponse");
            response.SerializeToString(any.mutable_value());
            result.response = any;
            std::move(callback).Run(std::move(result), nullptr);
          });

  bool callback_called = false;
  service_->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kNoThemesFound, result.status);
        EXPECT_EQ("No themes found.", result.message);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
}

TEST_F(FindsServiceTest, NoSuggestionsForTheme) {
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryResults results;
        std::vector<history::URLResult> urls;
        urls.emplace_back(GURL("https://example.com"), base::Time::Now());
        results.SetURLResults(std::move(urls));
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  EXPECT_CALL(*opt_guide_service_, ExecuteModel(_, _, _, _))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& execution_options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::OptimizationGuideModelExecutionResult result;
            optimization_guide::proto::FindsSuggestionResponse response;
            auto* suggestion_theme = response.add_suggested_themes();
            suggestion_theme->set_theme_title("Shopping");
            suggestion_theme->set_theme_type(SuggestionTheme::SHOPPING);
            optimization_guide::proto::Any any;
            any.set_type_url(
                "type.googleapis.com/"
                "optimization_guide.proto.FindsSuggestionResponse");
            response.SerializeToString(any.mutable_value());
            result.response = any;
            std::move(callback).Run(std::move(result), nullptr);
          });

  bool callback_called = false;
  service_->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kNoNonCooldownThemesFound,
                  result.status);
        EXPECT_EQ("No themes found that passed cooldown criteria.",
                  result.message);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
  EXPECT_THAT(histogram_tester_.GetAllSamples("Finds.Result"),
              testing::ElementsAre(base::Bucket(
                  FindsService::Result::Status::kNoNonCooldownThemesFound, 1)));
}

TEST_F(FindsServiceTest, SkipsEmptyThemes) {
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryResults results;
        std::vector<history::URLResult> urls;
        urls.emplace_back(GURL("https://example.com"), base::Time::Now());
        results.SetURLResults(std::move(urls));
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  EXPECT_CALL(*opt_guide_service_, ExecuteModel(_, _, _, _))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& execution_options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::OptimizationGuideModelExecutionResult result;
            optimization_guide::proto::FindsSuggestionResponse response;

            // Add a high-scoring theme that is empty.
            auto* shopping_theme = response.add_suggested_themes();
            shopping_theme->set_theme_title("Shopping");
            shopping_theme->set_theme_type(SuggestionTheme::SHOPPING);
            shopping_theme->set_theme_score(10);

            // Add a lower-scoring theme that has suggestions.
            auto* travel_theme = response.add_suggested_themes();
            travel_theme->set_theme_title("Travel");
            travel_theme->set_theme_type(SuggestionTheme::TRAVEL);
            travel_theme->set_theme_score(5);
            travel_theme->add_theme_suggested_contents()->set_content_title(
                "Top Destinations");

            optimization_guide::proto::Any any;
            any.set_type_url(
                "type.googleapis.com/"
                "optimization_guide.proto.FindsSuggestionResponse");
            response.SerializeToString(any.mutable_value());
            result.response = any;
            std::move(callback).Run(std::move(result), nullptr);
          });

  std::unique_ptr<notifications::NotificationParams> scheduled_params;
  EXPECT_CALL(*notification_schedule_service_, Schedule(_))
      .WillOnce([&](std::unique_ptr<notifications::NotificationParams> params) {
        scheduled_params = std::move(params);
      });

  bool callback_called = false;
  service_->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kSuccess, result.status);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);

  ASSERT_NE(nullptr, scheduled_params);
  EXPECT_EQ(u"Top Destinations", scheduled_params->notification_data.title);
}

TEST_F(FindsServiceTest, ReturnsHighestScore) {
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryResults results;
        std::vector<history::URLResult> urls;
        urls.emplace_back(GURL("https://example.com"), base::Time::Now());
        results.SetURLResults(std::move(urls));
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  EXPECT_CALL(*opt_guide_service_, ExecuteModel(_, _, _, _))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& execution_options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::OptimizationGuideModelExecutionResult result;
            optimization_guide::proto::FindsSuggestionResponse response;
            auto* entertainment_theme = response.add_suggested_themes();
            entertainment_theme->set_theme_title("Entertainment");
            entertainment_theme->set_theme_type(SuggestionTheme::ENTERTAINMENT);
            entertainment_theme->set_theme_score(7);
            entertainment_theme->add_theme_suggested_contents()
                ->set_content_title("Trending Movies");
            auto* shopping_theme = response.add_suggested_themes();
            shopping_theme->set_theme_title("Shopping");
            shopping_theme->set_theme_type(SuggestionTheme::SHOPPING);
            shopping_theme->set_theme_score(9);
            shopping_theme->add_theme_suggested_contents()->set_content_title(
                "Latest Deals");
            auto* travel_theme = response.add_suggested_themes();
            travel_theme->set_theme_title("Travel");
            travel_theme->set_theme_type(SuggestionTheme::TRAVEL);
            travel_theme->set_theme_score(8);
            travel_theme->add_theme_suggested_contents()->set_content_title(
                "Top Destinations");
            optimization_guide::proto::Any any;
            any.set_type_url(
                "type.googleapis.com/"
                "optimization_guide.proto.FindsSuggestionResponse");
            response.SerializeToString(any.mutable_value());
            result.response = any;
            std::move(callback).Run(std::move(result), nullptr);
          });

  std::unique_ptr<notifications::NotificationParams> scheduled_params;
  EXPECT_CALL(*notification_schedule_service_, Schedule(_))
      .WillOnce([&](std::unique_ptr<notifications::NotificationParams> params) {
        scheduled_params = std::move(params);
      });

  bool callback_called = false;
  service_->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kSuccess, result.status);
        EXPECT_EQ(
            "Theme: Entertainment (Type: 3, Score: 7)\n  - Trending Movies: "
            "\nTheme: Shopping (Type: 4, Score: 9)\n  - Latest Deals: \nTheme: "
            "Travel (Type: 5, Score: 8)\n  - Top Destinations: ",
            result.message);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);

  ASSERT_NE(nullptr, scheduled_params);
  EXPECT_EQ(u"Latest Deals", scheduled_params->notification_data.title);
}

TEST_F(FindsServiceTest, SkipsThemeOnCooldown) {
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryResults results;
        std::vector<history::URLResult> urls;
        urls.emplace_back(GURL("https://example.com"), base::Time::Now());
        results.SetURLResults(std::move(urls));
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  EXPECT_CALL(*opt_guide_service_, ExecuteModel(_, _, _, _))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& execution_options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::OptimizationGuideModelExecutionResult result;
            optimization_guide::proto::FindsSuggestionResponse response;
            auto* entertainment_theme = response.add_suggested_themes();
            entertainment_theme->set_theme_title("Entertainment");
            entertainment_theme->set_theme_type(SuggestionTheme::ENTERTAINMENT);
            entertainment_theme->set_theme_score(7);
            entertainment_theme->add_theme_suggested_contents()
                ->set_content_title("Trending Movies");
            auto* shopping_theme = response.add_suggested_themes();
            shopping_theme->set_theme_title("Shopping");
            shopping_theme->set_theme_type(SuggestionTheme::SHOPPING);
            shopping_theme->set_theme_score(9);
            shopping_theme->add_theme_suggested_contents()->set_content_title(
                "Latest Deals");
            auto* travel_theme = response.add_suggested_themes();
            travel_theme->set_theme_title("Travel");
            travel_theme->set_theme_type(SuggestionTheme::TRAVEL);
            travel_theme->set_theme_score(8);
            travel_theme->add_theme_suggested_contents()->set_content_title(
                "Top Destinations");
            optimization_guide::proto::Any any;
            any.set_type_url(
                "type.googleapis.com/"
                "optimization_guide.proto.FindsSuggestionResponse");
            response.SerializeToString(any.mutable_value());
            result.response = any;
            std::move(callback).Run(std::move(result), nullptr);
          });

  finds::MarkThemeAsNotInterested(&prefs_, SuggestionTheme::SHOPPING);

  std::unique_ptr<notifications::NotificationParams> scheduled_params;
  EXPECT_CALL(*notification_schedule_service_, Schedule(_))
      .WillOnce([&](std::unique_ptr<notifications::NotificationParams> params) {
        scheduled_params = std::move(params);
      });

  bool callback_called = false;
  service_->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kSuccess, result.status);
        EXPECT_EQ(
            "Theme: Entertainment (Type: 3, Score: 7)\n  - Trending Movies: "
            "\nTheme: Shopping (Type: 4, Score: 9)\n  - Latest Deals: \nTheme: "
            "Travel (Type: 5, Score: 8)\n  - Top Destinations: ",
            result.message);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);

  ASSERT_NE(nullptr, scheduled_params);
  EXPECT_EQ(u"Top Destinations", scheduled_params->notification_data.title);
}

TEST_F(FindsServiceTest, NoNotificationIfAllOnCooldown) {
  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _))
      .WillOnce([](const std::u16string& text_query,
                   const history::QueryOptions& options,
                   history::HistoryService::QueryHistoryCallback callback,
                   base::CancelableTaskTracker* tracker) {
        history::QueryResults results;
        std::vector<history::URLResult> urls;
        urls.emplace_back(GURL("https://example.com"), base::Time::Now());
        results.SetURLResults(std::move(urls));
        std::move(callback).Run(std::move(results));
        return base::CancelableTaskTracker::kBadTaskId;
      });

  EXPECT_CALL(*opt_guide_service_, ExecuteModel(_, _, _, _))
      .WillOnce(
          [](optimization_guide::ModelBasedCapabilityKey feature,
             const google::protobuf::MessageLite& request_metadata,
             const optimization_guide::ModelExecutionOptions& execution_options,
             optimization_guide::OptimizationGuideModelExecutionResultCallback
                 callback) {
            optimization_guide::OptimizationGuideModelExecutionResult result;
            optimization_guide::proto::FindsSuggestionResponse response;
            auto* entertainment_theme = response.add_suggested_themes();
            entertainment_theme->set_theme_title("Entertainment");
            entertainment_theme->set_theme_type(SuggestionTheme::ENTERTAINMENT);
            entertainment_theme->set_theme_score(7);
            entertainment_theme->add_theme_suggested_contents()
                ->set_content_title("Trending Movies");
            auto* shopping_theme = response.add_suggested_themes();
            shopping_theme->set_theme_title("Shopping");
            shopping_theme->set_theme_type(SuggestionTheme::SHOPPING);
            shopping_theme->set_theme_score(9);
            shopping_theme->add_theme_suggested_contents()->set_content_title(
                "Latest Deals");
            auto* travel_theme = response.add_suggested_themes();
            travel_theme->set_theme_title("Travel");
            travel_theme->set_theme_type(SuggestionTheme::TRAVEL);
            travel_theme->set_theme_score(8);
            travel_theme->add_theme_suggested_contents()->set_content_title(
                "Top Destinations");
            optimization_guide::proto::Any any;
            any.set_type_url(
                "type.googleapis.com/"
                "optimization_guide.proto.FindsSuggestionResponse");
            response.SerializeToString(any.mutable_value());
            result.response = any;
            std::move(callback).Run(std::move(result), nullptr);
          });

  finds::MarkThemeAsNotInterested(&prefs_, SuggestionTheme::SHOPPING);
  finds::MarkThemeAsNotInterested(&prefs_, SuggestionTheme::ENTERTAINMENT);
  finds::MarkThemeAsNotInterested(&prefs_, SuggestionTheme::TRAVEL);

  EXPECT_CALL(*notification_schedule_service_, Schedule(_)).Times(0);

  bool callback_called = false;
  service_->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kNoNonCooldownThemesFound,
                  result.status);
        EXPECT_EQ("No themes found that passed cooldown criteria.",
                  result.message);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);
}

TEST_F(FindsServiceTest, RecordThemeURLVisitedIncrementsCount) {
  EXPECT_TRUE(theme_url_visit_count().empty());

  service_->RecordThemeURLVisited(
      optimization_guide::proto::FindsMetadata::SHOPPING);

  auto it = theme_url_visit_count().find(
      optimization_guide::proto::FindsMetadata::SHOPPING);
  EXPECT_NE(it, theme_url_visit_count().end());
  EXPECT_EQ(it->second, 1);
}

TEST_F(FindsServiceTest, RecordThemeURLVisitedThresholdTriggersOptIn) {
  testing::NiceMock<MockFindsServiceObserver> observer;
  service_->AddObserver(&observer);

  EXPECT_CALL(observer, OnOptInCriteriaFulfilled()).Times(1);

  service_->RecordThemeURLVisited(
      optimization_guide::proto::FindsMetadata::SHOPPING);
  auto it = theme_url_visit_count().find(
      optimization_guide::proto::FindsMetadata::SHOPPING);
  EXPECT_NE(it, theme_url_visit_count().end());
  EXPECT_EQ(it->second, 1);

  service_->RecordThemeURLVisited(
      optimization_guide::proto::FindsMetadata::SHOPPING);
  it = theme_url_visit_count().find(
      optimization_guide::proto::FindsMetadata::SHOPPING);
  EXPECT_NE(it, theme_url_visit_count().end());
  EXPECT_EQ(it->second, 2);

  service_->RecordThemeURLVisited(
      optimization_guide::proto::FindsMetadata::SHOPPING);
  it = theme_url_visit_count().find(
      optimization_guide::proto::FindsMetadata::SHOPPING);
  EXPECT_NE(it, theme_url_visit_count().end());
  EXPECT_EQ(it->second, 0);

  histogram_tester_.ExpectUniqueSample(
      "Notifications.ChromeFinds.OptInCriteriaFulfilled.Reason",
      FindsOptInTriggerReason::kThemeUrlVisitCount, 1);

  service_->RemoveObserver(&observer);
}

TEST_F(FindsServiceTest, SRPBackNavigationTriggersOptIn) {
  testing::NiceMock<MockFindsServiceObserver> observer;
  service_->AddObserver(&observer);

  EXPECT_CALL(observer, OnOptInCriteriaFulfilled()).Times(1);

  service_->SRPBackNavigationCountForOptInReached();

  histogram_tester_.ExpectUniqueSample(
      "Notifications.ChromeFinds.OptInCriteriaFulfilled.Reason",
      FindsOptInTriggerReason::kSrpBackNavigationCount, 1);

  service_->RemoveObserver(&observer);
}

TEST_F(FindsServiceTest, ScheduleNotificationForInternalsPage) {
  std::unique_ptr<notifications::NotificationParams> scheduled_params;
  EXPECT_CALL(*notification_schedule_service_, Schedule(_))
      .WillOnce([&](std::unique_ptr<notifications::NotificationParams> params) {
        scheduled_params = std::move(params);
      });

  bool success = service_->ScheduleNotificationForInternalsPage();
  EXPECT_TRUE(success);

  ASSERT_NE(nullptr, scheduled_params);
  EXPECT_EQ(u"Test Notification", scheduled_params->notification_data.title);
  EXPECT_EQ(u"This is a test notification from the internals page.",
            scheduled_params->notification_data.message);
  EXPECT_EQ("https://www.google.com",
            scheduled_params->notification_data
                .custom_data[notifications::kChromeFindsNotificationsUrl]);
  EXPECT_NE(0, prefs_.GetInt64(prefs::kFindsModelExecutionLastTimestamp));
}

TEST_F(FindsServiceTest, ClearNotificationsBeforeScheduling) {
  testing::InSequence enforce_call_order;
  EXPECT_CALL(
      *notification_schedule_service_,
      DeleteNotifications(notifications::SchedulerClientType::kChromeFinds))
      .Times(1);
  EXPECT_CALL(*notification_schedule_service_, Schedule(_)).Times(1);

  bool success = service_->ScheduleNotificationForInternalsPage();
  EXPECT_TRUE(success);
}

TEST_F(FindsServiceTest, MaybeRescheduleNotifications_Empty_NoOp) {
  EXPECT_CALL(*notification_schedule_service_, GetClientOverview(_, _))
      .WillOnce(
          [](notifications::SchedulerClientType client_type,
             base::OnceCallback<void(notifications::ClientOverview)> callback) {
            std::move(callback).Run(notifications::ClientOverview());
          });

  EXPECT_CALL(*notification_schedule_service_, DeleteNotifications(_)).Times(0);
  EXPECT_CALL(*notification_schedule_service_, Schedule(_)).Times(0);

  service_->MaybeRescheduleNotifications();
}

TEST_F(FindsServiceTest, MaybeRescheduleNotifications_Reschedules) {
  notifications::NotificationEntry entry;
  notifications::ClientOverview overview;
  overview.scheduled_notifications.push_back(&entry);

  EXPECT_CALL(*notification_schedule_service_, GetClientOverview(_, _))
      .WillOnce(
          [&](notifications::SchedulerClientType client_type,
              base::OnceCallback<void(notifications::ClientOverview)>
                  callback) { std::move(callback).Run(std::move(overview)); });

  EXPECT_CALL(*notification_schedule_service_, DeleteNotifications(_)).Times(1);
  EXPECT_CALL(*notification_schedule_service_, Schedule(_)).Times(1);

  service_->MaybeRescheduleNotifications();
}

TEST_F(FindsServiceTest, TestExecuteModelEnterprisePolicyDisabled) {
  prefs_.SetInteger(
      optimization_guide::prefs::kFindsEnterprisePolicyAllowed,
      static_cast<int>(optimization_guide::model_execution::prefs::
                           ModelExecutionEnterprisePolicyValue::kDisable));

  base::RunLoop run_loop;
  service_->ExecuteModelAndScheduleNotification(base::BindOnce(
      [](base::OnceClosure quit_closure, FindsService::Result result) {
        EXPECT_EQ(result.status,
                  FindsService::Result::Status::kDisabledByEnterprisePolicy);
        EXPECT_EQ(result.message,
                  "Error: Feature disabled by enterprise policy.");
        std::move(quit_closure).Run();
      },
      run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(FindsServiceTest, TestRecordThemeURLVisitedEnterprisePolicyDisabled) {
  prefs_.SetInteger(
      optimization_guide::prefs::kFindsEnterprisePolicyAllowed,
      static_cast<int>(optimization_guide::model_execution::prefs::
                           ModelExecutionEnterprisePolicyValue::kDisable));

  testing::NiceMock<MockFindsServiceObserver> observer;
  service_->AddObserver(&observer);

  EXPECT_CALL(observer, OnOptInCriteriaFulfilled()).Times(0);

  // Threshold is 3.
  service_->RecordThemeURLVisited(
      optimization_guide::proto::FindsMetadata::SHOPPING);
  service_->RecordThemeURLVisited(
      optimization_guide::proto::FindsMetadata::SHOPPING);
  service_->RecordThemeURLVisited(
      optimization_guide::proto::FindsMetadata::SHOPPING);

  EXPECT_TRUE(theme_url_visit_count().empty());

  service_->RemoveObserver(&observer);
}

TEST_F(FindsServiceTest, TestSRPBackNavigationEnterprisePolicyDisabled) {
  prefs_.SetInteger(
      optimization_guide::prefs::kFindsEnterprisePolicyAllowed,
      static_cast<int>(optimization_guide::model_execution::prefs::
                           ModelExecutionEnterprisePolicyValue::kDisable));

  testing::NiceMock<MockFindsServiceObserver> observer;
  service_->AddObserver(&observer);

  EXPECT_CALL(observer, OnOptInCriteriaFulfilled()).Times(0);

  service_->SRPBackNavigationCountForOptInReached();

  service_->RemoveObserver(&observer);
}

TEST_F(FindsServiceTest, TestModelExecutionDisabledByParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      finds::features::kChromeFinds, {{"block_model_execution", "true"}});

  EXPECT_CALL(*history_service_, QueryHistory(_, _, _, _)).Times(0);
  EXPECT_CALL(*opt_guide_service_, ExecuteModel(_, _, _, _)).Times(0);

  bool callback_called = false;
  service_->ExecuteModelAndScheduleNotification(
      base::BindLambdaForTesting([&](FindsService::Result result) {
        EXPECT_EQ(FindsService::Result::Status::kModelExecutionDisabledByParam,
                  result.status);
        EXPECT_EQ("Error: Model execution disabled by feature parameter.",
                  result.message);
        callback_called = true;
      }));
  EXPECT_TRUE(callback_called);

  histogram_tester_.ExpectUniqueSample(
      "Finds.Result",
      FindsService::Result::Status::kModelExecutionDisabledByParam, 1);
}

}  // namespace finds
