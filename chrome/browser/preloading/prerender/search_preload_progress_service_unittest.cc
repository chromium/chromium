// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/search_preload_progress_service.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/preloading/prerender/search_preload_progress_test_utils.h"
#include "content/public/browser/prerender_handle.h"
#include "content/public/browser/prerender_host_id.h"
#include "testing/gtest/include/gtest/gtest.h"

class SearchPreloadProgressServiceTest : public testing::Test {
 public:
  SearchPreloadProgressServiceTest() = default;

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(SearchPreloadProgressServiceTest, InitialState) {
  SearchPreloadProgressService service;
  EXPECT_FALSE(service.HasOnGoingSearchPrewarm());
}

TEST_F(SearchPreloadProgressServiceTest, StartAndFinish) {
  SearchPreloadProgressService service;
  content::PrerenderHostId host_id(1);

  service.OnSearchPrewarmStarted(host_id);
  EXPECT_TRUE(service.HasOnGoingSearchPrewarm());
  EXPECT_TRUE(service.IsOnGoingSearchPrewarm(host_id));

  service.OnSearchPrewarmFinished(
      host_id, content::PrerenderLifecycleStatus::kHTTPSuccessResponse);
  EXPECT_FALSE(service.HasOnGoingSearchPrewarm());
  EXPECT_FALSE(service.IsOnGoingSearchPrewarm(host_id));
}

TEST_F(SearchPreloadProgressServiceTest, MultiplePrewarms) {
  SearchPreloadProgressService service;
  content::PrerenderHostId host_id1(1);
  content::PrerenderHostId host_id2(2);

  service.OnSearchPrewarmStarted(host_id1);
  service.OnSearchPrewarmStarted(host_id2);
  EXPECT_TRUE(service.HasOnGoingSearchPrewarm());
  EXPECT_TRUE(service.IsOnGoingSearchPrewarm(host_id1));
  EXPECT_TRUE(service.IsOnGoingSearchPrewarm(host_id2));

  service.OnSearchPrewarmFinished(
      host_id1, content::PrerenderLifecycleStatus::kHTTPSuccessResponse);
  EXPECT_TRUE(service.HasOnGoingSearchPrewarm());
  EXPECT_FALSE(service.IsOnGoingSearchPrewarm(host_id1));
  EXPECT_TRUE(service.IsOnGoingSearchPrewarm(host_id2));

  service.OnSearchPrewarmFinished(
      host_id2, content::PrerenderLifecycleStatus::kHTTPSuccessResponse);
  EXPECT_FALSE(service.HasOnGoingSearchPrewarm());
  EXPECT_FALSE(service.IsOnGoingSearchPrewarm(host_id1));
  EXPECT_FALSE(service.IsOnGoingSearchPrewarm(host_id2));
}

TEST_F(SearchPreloadProgressServiceTest, CallbacksNotifiedOnFinish) {
  SearchPreloadProgressService service;
  content::PrerenderHostId host_id1(1);
  content::PrerenderHostId host_id2(2);

  service.OnSearchPrewarmStarted(host_id1);
  service.OnSearchPrewarmStarted(host_id2);

  SearchPreloadProgressTestObserver observer1(&service);
  SearchPreloadProgressTestObserver observer2(&service);

  EXPECT_FALSE(observer1.was_notified());
  EXPECT_FALSE(observer2.was_notified());

  service.OnSearchPrewarmFinished(
      host_id1, content::PrerenderLifecycleStatus::kHTTPSuccessResponse);

  EXPECT_FALSE(observer1.was_notified());
  EXPECT_FALSE(observer2.was_notified());

  service.OnSearchPrewarmFinished(
      host_id2, content::PrerenderLifecycleStatus::kHTTPSuccessResponse);

  EXPECT_TRUE(observer1.was_notified());
  EXPECT_TRUE(observer2.was_notified());
}

TEST_F(SearchPreloadProgressServiceTest, DisableFeature) {
  SearchPreloadProgressService service;
  EXPECT_FALSE(service.ShouldBlockPrewarm());

  service.EnterBlackoutPeriod();
  EXPECT_TRUE(service.ShouldBlockPrewarm());

  // Advance time by 1 day.
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_FALSE(service.ShouldBlockPrewarm());
}
