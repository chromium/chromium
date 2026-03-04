// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/search_prewarm_progress_service.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class SearchPrewarmProgressServiceTest : public testing::Test {
 public:
  SearchPrewarmProgressServiceTest() = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SearchPrewarmProgressServiceTest, InitialState) {
  SearchPrewarmProgressService service;
  EXPECT_FALSE(service.HasOnGoingSearchPrewarm());
}

TEST_F(SearchPrewarmProgressServiceTest, StartAndFinish) {
  SearchPrewarmProgressService service;
  service.OnSearchPrewarmStarted();
  EXPECT_TRUE(service.HasOnGoingSearchPrewarm());

  service.OnSearchPrewarmFinished();
  EXPECT_FALSE(service.HasOnGoingSearchPrewarm());
}

TEST_F(SearchPrewarmProgressServiceTest, MultiplePrewarms) {
  SearchPrewarmProgressService service;
  service.OnSearchPrewarmStarted();
  service.OnSearchPrewarmStarted();
  EXPECT_TRUE(service.HasOnGoingSearchPrewarm());

  service.OnSearchPrewarmFinished();
  EXPECT_TRUE(service.HasOnGoingSearchPrewarm());

  service.OnSearchPrewarmFinished();
  EXPECT_FALSE(service.HasOnGoingSearchPrewarm());
}

TEST_F(SearchPrewarmProgressServiceTest, CallbacksTriggeredOnFinish) {
  SearchPrewarmProgressService service;
  service.OnSearchPrewarmStarted();

  bool callback1_called = false;
  bool callback2_called = false;

  service.AddSearchPrewarmFinishedCallback(
      base::BindLambdaForTesting([&]() { callback1_called = true; }));
  service.AddSearchPrewarmFinishedCallback(
      base::BindLambdaForTesting([&]() { callback2_called = true; }));

  EXPECT_FALSE(callback1_called);
  EXPECT_FALSE(callback2_called);

  service.OnSearchPrewarmFinished();

  EXPECT_TRUE(callback1_called);
  EXPECT_TRUE(callback2_called);
}
