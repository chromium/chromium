// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/device_bound_session_prewarmer.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/session_access.h"
#include "net/device_bound_sessions/session_key.h"
#include "services/network/test/mock_device_bound_session_manager.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::net::device_bound_sessions::RefreshResult;
using ::net::device_bound_sessions::SessionAccess;
using ::net::device_bound_sessions::SessionKey;
using ::testing::_;

namespace {

auto RunPrewarmCallback(
    std::optional<base::Time> earliest_next_refresh_time = std::nullopt,
    std::vector<RefreshResult> results = {}) {
  return [earliest_next_refresh_time, results = std::move(results)](
             const GURL&,
             network::mojom::DeviceBoundSessionManager::
                 PrewarmSessionsForUrlCallback callback) {
    std::move(callback).Run(results, earliest_next_refresh_time);
  };
}

auto RunPrewarmCallbackAndQuit(
    base::RunLoop& run_loop,
    std::optional<base::Time> earliest_next_refresh_time = std::nullopt,
    std::vector<RefreshResult> results = {}) {
  return [&run_loop, earliest_next_refresh_time,
          results = std::move(results)](
             const GURL&,
             network::mojom::DeviceBoundSessionManager::
                 PrewarmSessionsForUrlCallback callback) {
    std::move(callback).Run(results, earliest_next_refresh_time);
    run_loop.Quit();
  };
}

}  // namespace

class DeviceBoundSessionPrewarmerTest : public testing::Test {
 public:
  DeviceBoundSessionPrewarmerTest() {
    ON_CALL(mock_session_manager(), AddObserver)
        .WillByDefault(
            [this](const GURL& url,
                   mojo::PendingRemote<
                       network::mojom::DeviceBoundSessionAccessObserver>
                       observer) {
              observer_remote_.reset();
              observer_remote_.Bind(std::move(observer));
              if (observer_bound_quit_closure_) {
                std::move(observer_bound_quit_closure_).Run();
              }
            });
  }

  mojo::Remote<network::mojom::DeviceBoundSessionAccessObserver>&
  WaitForObserverRemote() {
    if (!observer_remote_.is_bound()) {
      base::RunLoop run_loop;
      observer_bound_quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    return observer_remote_;
  }

  network::MockDeviceBoundSessionManager& mock_session_manager() {
    return mock_device_bound_session_manager_;
  }
  DeviceBoundSessionPrewarmer::SessionManagerProvider GetManagerProvider() {
    return base::BindLambdaForTesting(
        [this]() -> network::mojom::DeviceBoundSessionManager* {
          return &mock_session_manager();
        });
  }

  // Per-test instance installed by `content::UnitTestTestSuite`.
  network::TestNetworkConnectionTracker* network_connection_tracker() {
    return network::TestNetworkConnectionTracker::GetInstance();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  GURL target_url_{"https://google.com"};

 private:
  network::MockDeviceBoundSessionManager mock_device_bound_session_manager_;
  mojo::Remote<network::mojom::DeviceBoundSessionAccessObserver>
      observer_remote_;
  base::OnceClosure observer_bound_quit_closure_;
};

TEST_F(DeviceBoundSessionPrewarmerTest, LogsStartupUmaTrue) {
  base::HistogramTester histogram_tester;
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        std::vector<RefreshResult> results = {RefreshResult::kRefreshed};
        std::move(callback).Run(results, base::Time::Now() + base::Seconds(90));
      });

  prewarmer.Start(/*is_startup_prewarm=*/true);

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
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        std::vector<RefreshResult> results = {RefreshResult::kRefreshed};
        std::move(callback).Run(results, base::Time::Now() + base::Seconds(90));
      });

  prewarmer.Start(/*is_startup_prewarm=*/false);

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
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(3)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        std::move(callback).Run({}, base::Time::Now() + base::Seconds(90));
      });

  // Starts recurring timer dynamically. First call happens immediately (or next
  // tick).
  prewarmer.Start(/*is_startup_prewarm=*/true);

  task_environment_.FastForwardBy(base::Seconds(180));

  prewarmer.Stop();
}

TEST_F(DeviceBoundSessionPrewarmerTest,
       SchedulesAfterMinIntervalIfTimeTooSoon) {
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        // Provide a time that is too soon (e.g. 10 seconds).
        std::move(callback).Run({}, base::Time::Now() + base::Seconds(10));
      });

  prewarmer.Start(/*is_startup_prewarm=*/false);

  // Prewarm is invoked at t=0.
  // Fast forward by 10s: Prewarmer should NOT execute yet because it is capped
  // by the hard limit minimum interval (60s).
  task_environment_.FastForwardBy(base::Seconds(10));

  // Fast forward by another 50s (total 60s from start): Prewarmer executes.
  task_environment_.FastForwardBy(base::Seconds(50));
}

TEST_F(DeviceBoundSessionPrewarmerTest, StopsInvokingWhenStopped) {
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());

  // Only called once before Stop()
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce([&](const GURL& url,
                    network::mojom::DeviceBoundSessionManager::
                        PrewarmSessionsForUrlCallback callback) {
        // Next is in 60s.
        std::move(callback).Run({}, base::Time::Now() + base::Seconds(60));
      });

  prewarmer.Start(/*is_startup_prewarm=*/true);

  // Fast forward by 30 seconds. One invocation happens at t=0.
  task_environment_.FastForwardBy(base::Seconds(30));

  prewarmer.Stop();

  // Further time skips should not trigger Mojo since it is stopped.
  task_environment_.FastForwardBy(base::Minutes(10));
}

TEST_F(DeviceBoundSessionPrewarmerTest,
       SchedulesAfterDefaultIntervalIfTimeInPast) {
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(3)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        // Provide a time in the past.
        std::move(callback).Run({}, base::Time::Now() - base::Seconds(1));
      });

  prewarmer.Start(/*is_startup_prewarm=*/true);

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

  DeviceBoundSessionPrewarmer prewarmer(target_url_,
                                        missing_session_manager_provider,
                                        network_connection_tracker());

  // Prewarmer shouldn't crash.
  prewarmer.Start(/*is_startup_prewarm=*/true);

  // Skip time; this should safely early-return.
  task_environment_.FastForwardBy(base::Seconds(60));
}

TEST_F(DeviceBoundSessionPrewarmerTest, CallbackNotInvokedAfterStop) {
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());

  network::mojom::DeviceBoundSessionManager::PrewarmSessionsForUrlCallback
      saved_callback;

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce([&](const GURL& url,
                    network::mojom::DeviceBoundSessionManager::
                        PrewarmSessionsForUrlCallback callback) {
        // Save the callback to invoke *after* Stop().
        saved_callback = std::move(callback);
      });

  prewarmer.Start(/*is_startup_prewarm=*/true);
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
  DeviceBoundSessionPrewarmer prewarmer(target_url_, std::move(provider),
                                        network_connection_tracker());

  EXPECT_CALL(*mock_manager, PrewarmSessionsForUrl(target_url_, _))
      .WillOnce([&](const GURL& url,
                    network::mojom::DeviceBoundSessionManager::
                        PrewarmSessionsForUrlCallback callback) {
        // We must provide a next refresh time or a transient error, otherwise
        // the prewarmer stops and the subsequent fast forwards do nothing.
        std::move(callback).Run({}, base::Time::Now() + base::Seconds(90));
      });

  prewarmer.Start(/*is_startup_prewarm=*/true);
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
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        std::move(callback).Run({RefreshResult::kFatalError},
                                base::Time::Now() + base::Seconds(60));
      });

  prewarmer.Start(/*is_startup_prewarm=*/true);

  // Provided time but also no errors in result.
  task_environment_.FastForwardBy(base::Seconds(60));
}

TEST_F(
    DeviceBoundSessionPrewarmerTest,
    ReschedulesUsingDefaultIntervalIfNoRefreshTimeProvidedAndTransientError) {
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        std::move(callback).Run({RefreshResult::kSigningQuotaExceeded},
                                std::nullopt);
      });

  prewarmer.Start(/*is_startup_prewarm=*/true);

  // Since a transient error was provided, even without a next refresh time,
  // we should call the prewarmer again after the default interval (60s).
  task_environment_.FastForwardBy(base::Seconds(60));
}

TEST_F(DeviceBoundSessionPrewarmerTest,
       ReschedulesUsingDefaultIntervalOnTransientErrors) {
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        std::move(callback).Run({RefreshResult::kServerError}, std::nullopt);
      });

  prewarmer.Start(/*is_startup_prewarm=*/true);

  // Since a transient error was provided, even without a next refresh time,
  // we should call the prewarmer again after the default interval (60s).
  task_environment_.FastForwardBy(base::Seconds(60));
}

TEST_F(DeviceBoundSessionPrewarmerTest,
       ReschedulesUsingDefaultIntervalOnTransientErrorEvenWithNextRefreshTime) {
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce(RunPrewarmCallback(
          base::Time::Now() + base::Hours(2),
          {RefreshResult::kRefreshed, RefreshResult::kServerError}));

  prewarmer.Start(/*is_startup_prewarm=*/true);

  // Advance 59s: verify retry has NOT fired yet.
  task_environment_.FastForwardBy(base::Seconds(59));
  // Assert that initial call occurred and no retry was triggered during 0..59s.
  testing::Mock::VerifyAndClearExpectations(&mock_session_manager());

  // Advance 1s (reaching 60s): verify the retry fires now.
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce(RunPrewarmCallback(std::nullopt, {RefreshResult::kRefreshed,
                                                  RefreshResult::kRefreshed}));
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_F(DeviceBoundSessionPrewarmerTest,
       DoesNotRescheduleIfNoRefreshTimeAndNoTransientErrors) {
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce([&](const GURL& url,
                    network::mojom::DeviceBoundSessionManager::
                        PrewarmSessionsForUrlCallback callback) {
        std::move(callback).Run({RefreshResult::kFatalError}, std::nullopt);
      });

  prewarmer.Start(/*is_startup_prewarm=*/true);

  // No earliest_next_refresh_time provided and no transient errors in result.
  // Prewarmer should not be rescheduled.
  task_environment_.FastForwardBy(base::Seconds(120));
}

TEST_F(DeviceBoundSessionPrewarmerTest, StartTwice) {
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());

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

  prewarmer.Start(/*is_startup_prewarm=*/true);
  prewarmer.Start(/*is_startup_prewarm=*/true);

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
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());
  base::HistogramTester histogram_tester;

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL& url,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        std::move(callback).Run({RefreshResult::kRefreshed},
                                base::Time::Now() + base::Seconds(90));
      });

  prewarmer.Start(/*is_startup_prewarm=*/true);

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
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());
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
  prewarmer.Start(/*is_startup_prewarm=*/true);
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
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());
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
  prewarmer.Start(/*is_startup_prewarm=*/true);
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
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());
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
  prewarmer.Start(/*is_startup_prewarm=*/true);
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
  prewarmer.Start(/*is_startup_prewarm=*/true);
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Startup", 2);
  histogram_tester.ExpectTotalCount(
      "Net.DeviceBoundSessions.PrewarmResult.Scheduled", 1);
}

TEST_F(DeviceBoundSessionPrewarmerTest, RegistersObserverForPrewarmUrl) {
  EXPECT_CALL(mock_session_manager(), AddObserver(target_url_, _));
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _));

  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());
  prewarmer.Start(/*is_startup_prewarm=*/true);
}

TEST_F(DeviceBoundSessionPrewarmerTest, NewSessionCreationTriggersPrewarm) {
  // Initial Prewarm on Start(), returning no next refresh time.
  base::RunLoop initial_prewarm_loop;
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce(RunPrewarmCallbackAndQuit(initial_prewarm_loop));

  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());
  prewarmer.Start(/*is_startup_prewarm=*/true);
  initial_prewarm_loop.Run();

  // A new session is created. This should trigger another
  // PrewarmSessionsForUrl call.
  base::RunLoop second_prewarm_loop;
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce(RunPrewarmCallbackAndQuit(
          second_prewarm_loop, base::Time::Now() + base::Seconds(90),
          {RefreshResult::kInScopeRefreshNotYetNeeded}));

  SessionKey session_key{net::SchemefulSite(target_url_),
                         SessionKey::Id("session_id")};
  SessionAccess access{SessionAccess::AccessType::kCreation, session_key};
  WaitForObserverRemote()->OnDeviceBoundSessionAccessed(access);
  second_prewarm_loop.Run();
}

TEST_F(DeviceBoundSessionPrewarmerTest,
       NewSessionCreationStopsPendingPrewarmTimer) {
  base::RunLoop initial_prewarm_loop;
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce(RunPrewarmCallbackAndQuit(
          initial_prewarm_loop, base::Time::Now() + base::Seconds(60)));

  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());
  prewarmer.Start(/*is_startup_prewarm=*/true);
  initial_prewarm_loop.Run();

  // Advance by 30 seconds (halfway through the 60s timer).
  task_environment_.FastForwardBy(base::Seconds(30));

  // A new session is created at t=30s. This should stop the pending 60s timer
  // and trigger PrewarmSessionsForUrl immediately.
  base::RunLoop second_prewarm_loop;
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce(RunPrewarmCallbackAndQuit(
          second_prewarm_loop, base::Time::Now() + base::Seconds(90)));

  SessionKey session_key{net::SchemefulSite(target_url_),
                         SessionKey::Id("session_id")};
  SessionAccess access{SessionAccess::AccessType::kCreation, session_key};
  WaitForObserverRemote()->OnDeviceBoundSessionAccessed(access);
  second_prewarm_loop.Run();

  // Fast forward by 30 seconds (reaching t=60s from start).
  // The original timer must NOT fire.
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl).Times(0);
  task_environment_.FastForwardBy(base::Seconds(30));

  // Fast forward by another 60 seconds (reaching t=120s from start, which is
  // 90s from the second prewarm). The second prewarm's timer should now fire.
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _));
  task_environment_.FastForwardBy(base::Seconds(60));
}

TEST_F(DeviceBoundSessionPrewarmerTest,
       NonCreationSessionAccessDoesNotTriggerPrewarm) {
  base::RunLoop initial_prewarm_loop;
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce(RunPrewarmCallbackAndQuit(initial_prewarm_loop));

  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());
  prewarmer.Start(/*is_startup_prewarm=*/true);
  initial_prewarm_loop.Run();

  // Access events of type kUpdate or kTermination should NOT trigger
  // PrewarmSessionsForUrl.
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl).Times(0);

  SessionKey session_key{net::SchemefulSite(target_url_),
                         SessionKey::Id("session_id")};
  SessionAccess update_access{SessionAccess::AccessType::kUpdate, session_key};
  WaitForObserverRemote()->OnDeviceBoundSessionAccessed(update_access);

  SessionAccess term_access{SessionAccess::AccessType::kTermination,
                            session_key};
  WaitForObserverRemote()->OnDeviceBoundSessionAccessed(term_access);

  // Flush the Mojo pipe to ensure messages have been processed by the
  // receiver.
  WaitForObserverRemote().FlushForTesting();
}

TEST_F(DeviceBoundSessionPrewarmerTest,
       ObserverDisconnectReschedulesPrewarmTimer) {
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce(RunPrewarmCallback(base::Time::Now() + base::Hours(1)));

  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());
  prewarmer.Start(/*is_startup_prewarm=*/true);

  // Disconnect the observer remote. This simulates the network service closing
  // the pipe and should schedule DoPrewarm() after kMinPrewarmInterval (60s).
  WaitForObserverRemote().reset();

  // Advancing by 59 seconds should NOT trigger the prewarm yet.
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl).Times(0);
  task_environment_.FastForwardBy(base::Seconds(59));
  testing::Mock::VerifyAndClearExpectations(&mock_session_manager());

  // Advancing by 1 more second (total 60s) triggers DoPrewarm(),
  // which rebinds the observer and triggers prewarm.
  EXPECT_CALL(mock_session_manager(), AddObserver(target_url_, _));
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce(RunPrewarmCallback(base::Time::Now() + base::Hours(1)));
  task_environment_.FastForwardBy(base::Seconds(1));
}

struct PrewarmMetricsTestCase {
  const char* name;
  std::vector<RefreshResult> results;
  // Offset from the completion time; `std::nullopt` returns no refresh time.
  std::optional<base::TimeDelta> next_refresh_offset;
  // `nullptr` if no `Duration` sample is expected.
  const char* expected_duration_suffix;
  // `std::nullopt` if no `NextRefreshDelay` sample is expected.
  std::optional<base::TimeDelta> expected_next_refresh_delay;
};

const PrewarmMetricsTestCase kPrewarmMetricsTestCases[] = {
    {"NoSessions", {}, std::nullopt, nullptr, std::nullopt},
    {"NotYetNeeded",
     {RefreshResult::kInScopeRefreshNotYetNeeded},
     base::Minutes(10),
     ".NotYetNeeded",
     base::Minutes(10)},
    // Below `kMinPrewarmInterval`: recorded before clamping.
    {"Success",
     {RefreshResult::kRefreshed},
     base::Seconds(10),
     ".Success",
     base::Seconds(10)},
    {"SuccessAlreadyDue",
     {RefreshResult::kRefreshed},
     base::Seconds(-5),
     ".Success",
     base::TimeDelta()},
    {"SuccessWithoutNextRefresh",
     {RefreshResult::kRefreshedAsWaiter},
     std::nullopt,
     ".Success",
     std::nullopt},
    // Not expected for pre-warms; folded into other transient errors.
    {"InitializedService",
     {RefreshResult::kInitializedService},
     std::nullopt,
     ".OtherTransientError",
     std::nullopt},
    {"FatalError",
     {RefreshResult::kFatalError},
     std::nullopt,
     ".FatalError",
     std::nullopt},
    // Transient errors ignore the returned refresh time.
    {"Unreachable",
     {RefreshResult::kUnreachable},
     base::Seconds(10),
     ".Unreachable",
     std::nullopt},
    {"ServerError",
     {RefreshResult::kServerError},
     base::Seconds(10),
     ".OtherTransientError",
     std::nullopt},
    {"SigningErrors",
     {RefreshResult::kSigningQuotaExceeded,
      RefreshResult::kTransientSigningError},
     std::nullopt,
     ".OtherTransientError",
     std::nullopt},
    // Mixed results are recorded under the least successful outcome.
    {"MixedSuccessAndNotYetNeeded",
     {RefreshResult::kInScopeRefreshNotYetNeeded, RefreshResult::kRefreshed},
     base::Minutes(10),
     ".Success",
     base::Minutes(10)},
    {"MixedSuccessAndFatalError",
     {RefreshResult::kRefreshed, RefreshResult::kFatalError},
     std::nullopt,
     ".FatalError",
     std::nullopt},
    {"MixedFatalErrorAndUnreachable",
     {RefreshResult::kFatalError, RefreshResult::kUnreachable},
     std::nullopt,
     ".Unreachable",
     std::nullopt},
    {"MixedUnreachableAndServerError",
     {RefreshResult::kUnreachable, RefreshResult::kServerError},
     std::nullopt,
     ".OtherTransientError",
     std::nullopt},
};

class DeviceBoundSessionPrewarmerMetricsTest
    : public DeviceBoundSessionPrewarmerTest,
      public testing::WithParamInterface<PrewarmMetricsTestCase> {};

TEST_P(DeviceBoundSessionPrewarmerMetricsTest, LogsMetrics) {
  constexpr base::TimeDelta kPrewarmDuration = base::Milliseconds(250);
  const PrewarmMetricsTestCase& test_case = GetParam();
  base::HistogramTester histogram_tester;
  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());

  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .WillOnce([&](const GURL&,
                    network::mojom::DeviceBoundSessionManager::
                        PrewarmSessionsForUrlCallback callback) {
        task_environment_.AdvanceClock(kPrewarmDuration);
        std::optional<base::Time> next_refresh_time;
        if (test_case.next_refresh_offset) {
          next_refresh_time =
              base::Time::Now() + *test_case.next_refresh_offset;
        }
        std::move(callback).Run(test_case.results, next_refresh_time);
      });

  prewarmer.Start(/*is_startup_prewarm=*/true);
  task_environment_.FastForwardBy(base::Seconds(1));

  base::HistogramTester::CountsMap duration_counts =
      histogram_tester.GetTotalCountsForPrefix(
          "Net.DeviceBoundSessions.Prewarm.Duration.");
  if (test_case.expected_duration_suffix) {
    const std::string duration_histogram =
        base::StrCat({"Net.DeviceBoundSessions.Prewarm.Duration",
                      test_case.expected_duration_suffix});
    EXPECT_THAT(duration_counts,
                testing::ElementsAre(testing::Pair(duration_histogram, 1)));
    histogram_tester.ExpectUniqueTimeSample(duration_histogram,
                                            kPrewarmDuration, 1);
  } else {
    EXPECT_THAT(duration_counts, testing::IsEmpty());
  }

  if (test_case.expected_next_refresh_delay) {
    histogram_tester.ExpectUniqueTimeSample(
        "Net.DeviceBoundSessions.Prewarm.NextRefreshDelay",
        *test_case.expected_next_refresh_delay, 1);
  } else {
    histogram_tester.ExpectTotalCount(
        "Net.DeviceBoundSessions.Prewarm.NextRefreshDelay", 0);
  }

  prewarmer.Stop();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceBoundSessionPrewarmerMetricsTest,
    testing::ValuesIn(kPrewarmMetricsTestCases),
    [](const testing::TestParamInfo<PrewarmMetricsTestCase>& info) {
      return info.param.name;
    });

// Each pre-warm is timed from its own start, even when two are in flight.
TEST_F(DeviceBoundSessionPrewarmerTest, TimesOverlappingPrewarmsIndependently) {
  base::HistogramTester histogram_tester;
  std::vector<
      network::mojom::DeviceBoundSessionManager::PrewarmSessionsForUrlCallback>
      callbacks;
  EXPECT_CALL(mock_session_manager(), PrewarmSessionsForUrl(target_url_, _))
      .Times(2)
      .WillRepeatedly([&](const GURL&,
                          network::mojom::DeviceBoundSessionManager::
                              PrewarmSessionsForUrlCallback callback) {
        callbacks.push_back(std::move(callback));
      });

  DeviceBoundSessionPrewarmer prewarmer(target_url_, GetManagerProvider(),
                                        network_connection_tracker());
  prewarmer.Start(/*is_startup_prewarm=*/true);

  // A session creation issues a second pre-warm 100ms into the first one.
  task_environment_.AdvanceClock(base::Milliseconds(100));
  WaitForObserverRemote()->OnDeviceBoundSessionAccessed(
      {SessionAccess::AccessType::kCreation,
       SessionKey{net::SchemefulSite(target_url_),
                  SessionKey::Id("session_id")}});
  task_environment_.RunUntilIdle();
  ASSERT_EQ(callbacks.size(), 2u);

  task_environment_.AdvanceClock(base::Milliseconds(50));
  std::move(callbacks[0]).Run({RefreshResult::kRefreshed}, std::nullopt);
  std::move(callbacks[1]).Run({RefreshResult::kRefreshed}, std::nullopt);

  const char kHistogram[] = "Net.DeviceBoundSessions.Prewarm.Duration.Success";
  histogram_tester.ExpectTimeBucketCount(kHistogram, base::Milliseconds(150),
                                         1);
  histogram_tester.ExpectTimeBucketCount(kHistogram, base::Milliseconds(50), 1);

  prewarmer.Stop();
}
