// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/device_bound_session_prewarmer.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/mock_device_bound_session_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::net::device_bound_sessions::RefreshResult;
using ::testing::_;

class DeviceBoundSessionPrewarmerTest : public testing::Test {
 public:
  network::MockDeviceBoundSessionManager& mock_session_manager() {
    return mock_device_bound_session_manager_;
  }
  DeviceBoundSessionPrewarmer::SessionManagerProvider GetManagerProvider() {
    return base::BindLambdaForTesting(
        [this]() -> network::mojom::DeviceBoundSessionManager* {
          return &mock_session_manager();
        });
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  GURL target_url_{"https://google.com"};

 private:
  network::MockDeviceBoundSessionManager mock_device_bound_session_manager_;
};

TEST_F(DeviceBoundSessionPrewarmerTest, LogsStartupUmaTrue) {
  base::HistogramTester histogram_tester;
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        std::vector<RefreshResult> results = {RefreshResult::kRefreshed};
        std::move(callback).Run(results, base::Time::Now() + base::Seconds(90));
      });

  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);

  // First callback runs immediately or on first timer task.
  task_environment_.FastForwardBy(base::Seconds(1));
  histogram_tester.ExpectUniqueSample(
      "Net.DeviceBoundSessions.PrewarmResult.Startup",
      RefreshResult::kRefreshed, 1);
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled", 0);

  // Second callback runs after 90 seconds.
  task_environment_.FastForwardBy(base::Seconds(90));
  histogram_tester.ExpectUniqueSample(
      "Net.DeviceBoundSessions.PrewarmResult.Startup",
      RefreshResult::kRefreshed, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled",
      RefreshResult::kRefreshed, 1);

  prewarmer.Stop();
}

TEST_F(DeviceBoundSessionPrewarmerTest, LogsStartupUmaFalse) {
  base::HistogramTester histogram_tester;
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        std::vector<RefreshResult> results = {RefreshResult::kRefreshed};
        std::move(callback).Run(results, base::Time::Now() + base::Seconds(90));
      });

  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/false);

  // First callback runs immediately or on first timer task.
  task_environment_.FastForwardBy(base::Seconds(1));
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Startup", 0);
  histogram_tester.ExpectUniqueSample(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled",
      RefreshResult::kRefreshed, 1);

  // Second callback runs after 90 seconds.
  task_environment_.FastForwardBy(base::Seconds(90));
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Startup", 0);
  histogram_tester.ExpectUniqueSample(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled",
      RefreshResult::kRefreshed, 2);

  prewarmer.Stop();
}
TEST_F(DeviceBoundSessionPrewarmerTest, InvokesMojoOnTimerTick) {
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(3)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        std::move(callback).Run({}, base::Time::Now() + base::Seconds(90));
      });

  // Starts recurring timer dynamically. First call happens immediately (or next
  // tick).
  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);

  task_environment_.FastForwardBy(base::Seconds(180));

  prewarmer.Stop();
}

TEST_F(DeviceBoundSessionPrewarmerTest,
       SchedulesAfterMinIntervalIfTimeTooSoon) {
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        // Provide a time that is too soon (e.g. 10 seconds).
        std::move(callback).Run({}, base::Time::Now() + base::Seconds(10));
      });

  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/false);

  // Prewarm is invoked at t=0.
  // Fast forward by 10s: Prewarmer should NOT execute yet because it is capped
  // by the hard limit minimum interval (60s).
  task_environment_.FastForwardBy(base::Seconds(10));

  // Fast forward by another 50s (total 60s from start): Prewarmer executes.
  task_environment_.FastForwardBy(base::Seconds(50));
}

TEST_F(DeviceBoundSessionPrewarmerTest, StopsInvokingWhenStopped) {
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());

  // Only called once before Stop()
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce([&](const GURL& url,
                    network::mojom::DeviceBoundSessionManager::
                        PrewarmSessionsForUrlCallback callback) {
        // Next is in 60s.
        std::move(callback).Run({}, base::Time::Now() + base::Seconds(60));
      });

  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);

  // Fast forward by 30 seconds. One invocation happens at t=0.
  task_environment_.FastForwardBy(base::Seconds(30));

  prewarmer.Stop();

  // Further time skips should not trigger Mojo since it is stopped.
  task_environment_.FastForwardBy(base::Minutes(10));
}

TEST_F(DeviceBoundSessionPrewarmerTest,
       SchedulesAfterDefaultIntervalIfTimeInPast) {
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(3)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        // Provide a time in the past.
        std::move(callback).Run({}, base::Time::Now() - base::Seconds(1));
      });

  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);

  // 120 seconds, expecting t=0, t=60, t=120 (since time is in the past,
  // prewarmer should execute every 60 seconds).
  task_environment_.FastForwardBy(base::Seconds(120));
}

TEST_F(DeviceBoundSessionPrewarmerTest, GracefulOnMissingSessionManager) {
  DeviceBoundSessionPrewarmer::SessionManagerProvider
      missing_session_manager_provider = base::BindLambdaForTesting(
          []() -> network::mojom::DeviceBoundSessionManager* {
            return nullptr;
          });

  DeviceBoundSessionPrewarmer prewarmer(missing_session_manager_provider);

  // Prewarmer shouldn't crash.
  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);

  // Skip time; this should safely early-return.
  task_environment_.FastForwardBy(base::Seconds(60));
}

TEST_F(DeviceBoundSessionPrewarmerTest, CallbackNotInvokedAfterStop) {
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());

  network::mojom::DeviceBoundSessionManager::PrewarmSessionsForUrlCallback
      saved_callback;

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce([&](const GURL& url,
                    network::mojom::DeviceBoundSessionManager::
                        PrewarmSessionsForUrlCallback callback) {
        // Save the callback to invoke *after* Stop().
        saved_callback = std::move(callback);
      });

  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(saved_callback);
  prewarmer.Stop();

  // Run the callback setting the next refresh time in 60 seconds.
  std::move(saved_callback).Run({}, base::Time::Now() + base::Seconds(60));

  // Fast forward 120 seconds and ensure no further calls are made.
  task_environment_.FastForwardBy(base::Seconds(120));
}

TEST_F(DeviceBoundSessionPrewarmerTest, HandlesNetworkServiceDisconnect) {
  auto mock_manager =
      std::make_unique<network::MockDeviceBoundSessionManager>();
  DeviceBoundSessionPrewarmer::SessionManagerProvider provider =
      base::BindLambdaForTesting(
          [&]() -> network::mojom::DeviceBoundSessionManager* {
            return mock_manager.get();
          });
  DeviceBoundSessionPrewarmer prewarmer(std::move(provider));

  EXPECT_CALL(*mock_manager, PrewarmSessionsForUrl(target_url_, _))
      .WillOnce([&](const GURL& url,
                    network::mojom::DeviceBoundSessionManager::
                        PrewarmSessionsForUrlCallback callback) {
        // We must provide a next refresh time or a transient error, otherwise
        // the prewarmer stops and the subsequent fast forwards do nothing.
        std::move(callback).Run({}, base::Time::Now() + base::Seconds(90));
      });

  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);
  task_environment_.RunUntilIdle();

  // Simulate network service disconnect by destroying the mock manager.
  mock_manager.reset();

  // Since the provided session manager evaluates to null when the network
  // service disconnects, this will not invoke the mojo method nor crash.
  // Instead, it should schedule a retry after the default interval.
  task_environment_.FastForwardBy(base::Seconds(90));

  // Simulate the network service restarting.
  mock_manager = std::make_unique<network::MockDeviceBoundSessionManager>();

  EXPECT_CALL(*mock_manager, PrewarmSessionsForUrl(target_url_, _))
      .WillOnce([&](const GURL& url,
                    network::mojom::DeviceBoundSessionManager::
                        PrewarmSessionsForUrlCallback callback) {
        std::move(callback).Run({}, base::Time::Now() + base::Seconds(90));
      });

  // Fast forward by the default retry interval (60 seconds) to trigger the
  // next prewarm attempt, which should succeed now that the manager is back.
  task_environment_.FastForwardBy(base::Seconds(60));
}

TEST_F(DeviceBoundSessionPrewarmerTest,
       RescheduleIfRefreshTimeProvidedAndNoTransientErrors) {
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        std::move(callback).Run({RefreshResult::kFatalError},
                                base::Time::Now() + base::Seconds(60));
      });

  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);

  // Provided time but also no errors in result.
  task_environment_.FastForwardBy(base::Seconds(60));
}

TEST_F(
    DeviceBoundSessionPrewarmerTest,
    ReschedulesUsingDefaultIntervalIfNoRefreshTimeProvidedAndTransientError) {
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        std::move(callback).Run({RefreshResult::kSigningQuotaExceeded},
                                std::nullopt);
      });

  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);

  // Since a transient error was provided, even without a next refresh time,
  // we should call the prewarmer again after the default interval (60s).
  task_environment_.FastForwardBy(base::Seconds(60));
}

TEST_F(DeviceBoundSessionPrewarmerTest,
       ReschedulesUsingDefaultIntervalOnTransientErrors) {
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        std::move(callback).Run({RefreshResult::kServerError}, std::nullopt);
      });

  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);

  // Since a transient error was provided, even without a next refresh time,
  // we should call the prewarmer again after the default interval (60s).
  task_environment_.FastForwardBy(base::Seconds(60));
}

TEST_F(DeviceBoundSessionPrewarmerTest,
       DoesNotRescheduleIfNoRefreshTimeAndNoTransientErrors) {
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce([&](const GURL& url,
                    network::mojom::DeviceBoundSessionManager::
                        PrewarmSessionsForUrlCallback callback) {
        std::move(callback).Run({RefreshResult::kFatalError}, std::nullopt);
      });

  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);

  // No earliest_next_refresh_time provided and no transient errors in result.
  // Prewarmer should not be rescheduled.
  task_environment_.FastForwardBy(base::Seconds(120));
}

TEST_F(DeviceBoundSessionPrewarmerTest, StartTwice) {
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());

  network::mojom::DeviceBoundSessionManager::PrewarmSessionsForUrlCallback
      saved_callback_1;
  network::mojom::DeviceBoundSessionManager::PrewarmSessionsForUrlCallback
      saved_callback_2;

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(3)
      .WillOnce([&](const GURL& url,
                    network::mojom::DeviceBoundSessionManager::
                        PrewarmSessionsForUrlCallback callback) {
        saved_callback_1 = std::move(callback);
      })
      .WillOnce([&](const GURL& url,
                    network::mojom::DeviceBoundSessionManager::
                        PrewarmSessionsForUrlCallback callback) {
        saved_callback_2 = std::move(callback);
      })
      .WillOnce([&](const GURL& url,
                    network::mojom::DeviceBoundSessionManager::
                        PrewarmSessionsForUrlCallback callback) {
        // Do nothing in the final callback.
      });

  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);
  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);

  task_environment_.RunUntilIdle();

  ASSERT_TRUE(saved_callback_1);
  ASSERT_TRUE(saved_callback_2);

  // Run the first callback. It should be invalidated because Start() was called
  // a second time.
  std::move(saved_callback_1).Run({}, base::Time::Now() + base::Seconds(90));

  // Fast forwarding by 90 seconds should NOT trigger the prewarmer if the first
  // callback was ignored.
  task_environment_.FastForwardBy(base::Seconds(90));

  // Run the second callback. It should schedule a timer for 90 seconds.
  std::move(saved_callback_2).Run({}, base::Time::Now() + base::Seconds(90));

  // Fast forward by 90 seconds. Third call happens here.
  task_environment_.FastForwardBy(base::Seconds(90));
}

TEST_F(DeviceBoundSessionPrewarmerTest, LogsUmaMetricsOnPrewarmComplete) {
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());
  base::HistogramTester histogram_tester;

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        std::move(callback).Run({RefreshResult::kRefreshed},
                                base::Time::Now() + base::Seconds(90));
      });

  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);

  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "Net.DeviceBoundSessions.PrewarmResult.Startup",
      RefreshResult::kRefreshed, 1);
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled", 0);

  task_environment_.FastForwardBy(base::Seconds(90));
  histogram_tester.ExpectUniqueSample(
      "Net.DeviceBoundSessions.PrewarmResult.Startup",
      RefreshResult::kRefreshed, 1);
  histogram_tester.ExpectUniqueSample(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled",
      RefreshResult::kRefreshed, 1);
}

TEST_F(DeviceBoundSessionPrewarmerTest,
       DoesNotRetainStartupModeWhenResultsAreEmpty) {
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());
  base::HistogramTester histogram_tester;

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillOnce([&](const GURL& url,
                    network::mojom::DeviceBoundSessionManager::
                        PrewarmSessionsForUrlCallback callback) {
        // Return 0 results; flag should unconditionally drop.
        std::move(callback).Run({}, base::Time::Now() + base::Seconds(90));
      })
      .WillOnce([&](const GURL& url,
                    network::mojom::DeviceBoundSessionManager::
                        PrewarmSessionsForUrlCallback callback) {
        // Return 1 result which should be Scheduled.
        std::move(callback).Run({RefreshResult::kRefreshed},
                                base::Time::Now() + base::Seconds(90));
      });

  // First call (empty results).
  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Startup", 0);

  // Second call (1 result, expected to be Scheduled).
  task_environment_.FastForwardBy(base::Seconds(90));
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Startup", 0);
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled", 1);
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled",
      RefreshResult::kRefreshed, 1);
}

TEST_F(DeviceBoundSessionPrewarmerTest, LogsMultipleResultsCorrectly) {
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());
  base::HistogramTester histogram_tester;

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        // Return 2 results which should both be Startup on the first call and
        // Scheduled on the second call.
        std::move(callback).Run(
            {RefreshResult::kRefreshed, RefreshResult::kFatalError},
            base::Time::Now() + base::Seconds(90));
      });

  // First call (2 results, both Startup).
  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Startup", 2);
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.PrewarmResult.Startup",
      RefreshResult::kRefreshed, 1);
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.PrewarmResult.Startup",
      RefreshResult::kFatalError, 1);
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled", 0);

  // Second call (2 results, expected to be Scheduled).
  task_environment_.FastForwardBy(base::Seconds(90));
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Startup", 2);
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled", 2);
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled",
      RefreshResult::kRefreshed, 1);
  histogram_tester.ExpectBucketCount(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled",
      RefreshResult::kFatalError, 1);
}

TEST_F(DeviceBoundSessionPrewarmerTest, ResetStartupModeOnRestart) {
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());
  base::HistogramTester histogram_tester;

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(3)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        std::move(callback).Run({RefreshResult::kRefreshed},
                                base::Time::Now() + base::Seconds(90));
      });

  // First Start().
  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Startup", 1);
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled", 0);

  // Advance time for scheduled run.
  task_environment_.FastForwardBy(base::Seconds(90));
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Startup", 1);
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled", 1);

  // Restart via Start() again, should reset mode to Startup.
  prewarmer.Start(base::BindLambdaForTesting([&]() { return target_url_; }),
                  /*is_startup_prewarm=*/true);
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Startup", 2);
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled", 1);
}

class DeviceBoundSessionPrewarmerWithInvalidUrlTest
    : public DeviceBoundSessionPrewarmerTest,
      public ::testing::WithParamInterface<GURL> {};

INSTANTIATE_TEST_SUITE_P(DeviceBoundSessionPrewarmerWithInvalidUrlInstantiation,
                         DeviceBoundSessionPrewarmerWithInvalidUrlTest,
                         ::testing::Values(GURL(""),
                                           GURL("invalidurl"),
                                           GURL("http://google.com")));

TEST_P(DeviceBoundSessionPrewarmerWithInvalidUrlTest,
       RetriesMaxTimesUsingLongIntervalOnInvalidUrl) {
  DeviceBoundSessionPrewarmer prewarmer(GetManagerProvider());

  // Should not invoke mojo on empty/invalid URL.
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl).Times(0);

  int url_provider_calls = 0;
  prewarmer.Start(base::BindLambdaForTesting([&]() {
                    url_provider_calls++;
                    return GetParam();
                  }),
                  /*is_startup_prewarm=*/true);

  // Initial call happens immediately.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(url_provider_calls, 1);

  // Fast forward by 120 minutes should trigger the prewarmer at most
  // kMaxInvalidUrlRetries + 1 times.
  task_environment_.FastForwardBy(base::Minutes(120));

  EXPECT_EQ(url_provider_calls, 6);
}
