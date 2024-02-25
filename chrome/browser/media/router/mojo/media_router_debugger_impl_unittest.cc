// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_debugger_impl.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
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
  MediaRouterDebuggerImplTest() = default;
  MediaRouterDebuggerImplTest(const MediaRouterDebuggerImplTest&) = delete;
  ~MediaRouterDebuggerImplTest() override = default;
  MediaRouterDebuggerImplTest& operator=(const MediaRouterDebuggerImplTest&) =
      delete;

 protected:
  void SetUp() override {
    debugger_ = std::make_unique<MediaRouterDebuggerImpl>(&profile_);
    debugger_->AddObserver(observer_);
  }
  void TearDown() override { debugger_->RemoveObserver(observer_); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<MediaRouterDebuggerImpl> debugger_;
  testing::NiceMock<MockMirroringStatsObserver> observer_;
  TestingProfile profile_;
};

TEST_F(MediaRouterDebuggerImplTest, ShouldFetchMirroringStats) {
  // By default reports should be enabled.
  debugger_->ShouldFetchMirroringStats(
      base::BindOnce([](bool enabled) { EXPECT_TRUE(enabled); }));
}

TEST_F(MediaRouterDebuggerImplTest, ShouldFetchMirroringStatsFeatureDisabled) {
  // If the feature is disabled, then stats can still be fetched by enabling
  // them.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(media::kEnableRtcpReporting);

  EXPECT_CALL(observer_, OnMirroringStatsUpdated(_)).Times(0);
  debugger_->NotifyGetMirroringStats(base::Value::Dict());

  // Reports should now be disabled.
  debugger_->ShouldFetchMirroringStats(
      base::BindOnce([](bool enabled) { EXPECT_FALSE(enabled); }));

  debugger_->EnableRtcpReports();
  debugger_->ShouldFetchMirroringStats(
      base::BindOnce([](bool enabled) { EXPECT_TRUE(enabled); }));
}

TEST_F(MediaRouterDebuggerImplTest,
       ShouldFetchMirroringStatsAccessCodeCastFeature) {
  // If the feature is disabled, then fall back to the value of
  // AccessCodeCastEnabled.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(media::kEnableRtcpReporting);
  profile_.GetTestingPrefService()->SetManagedPref(
      prefs::kAccessCodeCastEnabled, std::make_unique<base::Value>(true));
  auto debugger_with_feature =
      std::make_unique<MediaRouterDebuggerImpl>(&profile_);
  debugger_with_feature->ShouldFetchMirroringStats(
      base::BindOnce([](bool enabled) { EXPECT_TRUE(enabled); }));

  // User settings should override policy pref.
  debugger_with_feature->DisableRtcpReports();
  debugger_with_feature->ShouldFetchMirroringStats(
      base::BindOnce([](bool enabled) { EXPECT_FALSE(enabled); }));
}

TEST_F(MediaRouterDebuggerImplTest, OnMirroringStats) {
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

TEST_F(MediaRouterDebuggerImplTest, GetMirroringStats) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(media::kEnableRtcpReporting);
  EXPECT_TRUE(debugger_->GetMirroringStats().empty());

  base::Value::Dict dict = base::Value::Dict();
  dict.Set("foo_key", "foo_value");
  debugger_->OnMirroringStats(base::Value(dict.Clone()));

  // GetLogs should only work if logs have been enabled.
  EXPECT_TRUE(debugger_->GetMirroringStats().empty());

  debugger_->EnableRtcpReports();
  debugger_->OnMirroringStats(base::Value(dict.Clone()));

  EXPECT_EQ(dict, debugger_->GetMirroringStats());
}

}  // namespace media_router
