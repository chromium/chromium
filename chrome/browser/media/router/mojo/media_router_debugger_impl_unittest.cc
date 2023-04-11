// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_debugger_impl.h"

#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace media_router {

namespace {

class MockMirroringStatsObserver
    : public MediaRouterDebugger::MirroringStatsObserver {
 public:
  MOCK_METHOD(void, OnMirroringStatsUpdated, (const base::Value::Dict&));
};
}  // namespace

class MediaRouterDebuggerImplTest : public ::testing::Test {
 public:
  MediaRouterDebuggerImplTest()
      : debugger_(std::make_unique<MediaRouterDebuggerImpl>()) {}
  MediaRouterDebuggerImplTest(const MediaRouterDebuggerImplTest&) = delete;
  ~MediaRouterDebuggerImplTest() override = default;
  MediaRouterDebuggerImplTest& operator=(const MediaRouterDebuggerImplTest&) =
      delete;

 protected:
  void SetUp() override { debugger_->AddObserver(observer_); }
  void TearDown() override { debugger_->RemoveObserver(observer_); }

  std::unique_ptr<MediaRouterDebuggerImpl> debugger_;
  testing::NiceMock<MockMirroringStatsObserver> observer_;
};

TEST_F(MediaRouterDebuggerImplTest, ShouldFetchMirroringStats) {
  // By default reports should be disabled.
  debugger_->ShouldFetchMirroringStats(
      base::BindOnce([](bool enabled) { EXPECT_FALSE(enabled); }));

  debugger_->EnableRtcpReports();
  debugger_->ShouldFetchMirroringStats(
      base::BindOnce([](bool enabled) { EXPECT_TRUE(enabled); }));
}

TEST_F(MediaRouterDebuggerImplTest, ShouldFetchMirroringStatsFeatureEnabled) {
  // By default reports should be disabled.
  debugger_->ShouldFetchMirroringStats(
      base::BindOnce([](bool enabled) { EXPECT_FALSE(enabled); }));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({media::kEnableRtcpReporting}, {});
  debugger_->ShouldFetchMirroringStats(
      base::BindOnce([](bool enabled) { EXPECT_TRUE(enabled); }));
}

TEST_F(MediaRouterDebuggerImplTest, OnMirroringStatsRtcpReportsDisabled) {
  EXPECT_CALL(observer_, OnMirroringStatsUpdated(_)).Times(0);
  debugger_->NotifyGetMirroringStats(base::Value::Dict());
}

TEST_F(MediaRouterDebuggerImplTest, OnMirroringStats) {
  debugger_->EnableRtcpReports();

  base::Value non_dict = base::Value("foo");
  base::Value::Dict empty_dict = base::Value::Dict();

  base::Value::Dict dict = base::Value::Dict();
  dict.Set("foo_key", "foo_value");

  // Verify that a non dict value will call notify the observers with a newly
  // constructed empty base::Value::Dict();
  EXPECT_CALL(observer_, OnMirroringStatsUpdated)
      .WillOnce(testing::Invoke([&](const base::Value::Dict& json_logs) {
        EXPECT_EQ(empty_dict, json_logs);
      }));
  debugger_->OnMirroringStats(non_dict.Clone());

  // Verify that a valid dict will notify observers of that value.
  EXPECT_CALL(observer_, OnMirroringStatsUpdated)
      .WillOnce(testing::Invoke([&](const base::Value::Dict& json_logs) {
        EXPECT_EQ(dict, json_logs);
      }));
  debugger_->OnMirroringStats(base::Value(dict.Clone()));
}

TEST_F(MediaRouterDebuggerImplTest, TestShouldFetchMirroringStats) {
  // Tests default condition.
  EXPECT_FALSE(debugger_->ShouldFetchMirroringStats());

  // Reports should still be disabled since we the feature flag has not been
  // set.
  debugger_->EnableRtcpReports();

  EXPECT_TRUE(debugger_->ShouldFetchMirroringStats());
}

}  // namespace media_router
