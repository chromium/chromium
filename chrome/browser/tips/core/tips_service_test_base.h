// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TIPS_CORE_TIPS_SERVICE_TEST_BASE_H_
#define CHROME_BROWSER_TIPS_CORE_TIPS_SERVICE_TEST_BASE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/segmentation_platform/ukm_data_manager_test_utils.h"
#include "chrome/browser/tips/core/tips_feature.h"
#include "chrome/browser/tips/core/tips_service.h"
#include "chrome/browser/tips/core/tips_types.h"
#include "chrome/test/base/testing_profile.h"
#include "components/segmentation_platform/internal/dummy_segmentation_platform_service.h"
#include "components/segmentation_platform/public/database_client.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tips {

class TestDatabaseClient : public segmentation_platform::DatabaseClient {
 public:
  explicit TestDatabaseClient(
      segmentation_platform::SegmentationPlatformService* real_service);
  ~TestDatabaseClient() override;

  void SetOverrideSignalValues(const std::vector<float>& override_values);
  void SetSimulateError(bool simulate_error);

  void ProcessFeatures(
      const segmentation_platform::proto::SegmentationModelMetadata& metadata,
      base::Time end_time,
      FeaturesCallback callback) override;

  void AddEvent(const StructuredEvent& event) override;

  segmentation_platform::proto::SegmentationModelMetadata last_metadata_;
  raw_ptr<segmentation_platform::SegmentationPlatformService> real_service_;
  std::optional<std::vector<float>> override_values_;
  bool simulate_error_ = false;
};

class TestSegmentationPlatformService
    : public segmentation_platform::DummySegmentationPlatformService {
 public:
  explicit TestSegmentationPlatformService(
      segmentation_platform::SegmentationPlatformService* real_service);
  ~TestSegmentationPlatformService() override;

  TestDatabaseClient* test_database_client();
  segmentation_platform::DatabaseClient* GetDatabaseClient() override;

 private:
  raw_ptr<segmentation_platform::SegmentationPlatformService> real_service_;
  std::unique_ptr<TestDatabaseClient> test_database_client_;
};

struct FeatureTestConfig {
  std::unique_ptr<TipsFeature> feature;
  std::optional<std::map<std::string, float>> mock_signal_values;
};

class TipsServiceTestBase : public ::testing::Test {
 public:
  TipsServiceTestBase();
  ~TipsServiceTestBase() override;

  void SetUp() override;
  void TearDown() override;

  void RecordUserAction(const std::string& action_name);
  std::optional<TipsNotificationsFeatureType> DetermineBestTipSync();

  void RunDetermineBestTipTest(
      std::vector<std::unique_ptr<TipsFeature>> features,
      std::optional<TipsNotificationsFeatureType> expected_best_tip);

  void RunDetermineBestTipTestWithOverrides(
      std::vector<FeatureTestConfig> configs,
      std::optional<TipsNotificationsFeatureType> expected_best_tip);

  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<TestSegmentationPlatformService> test_segmentation_service_;
  std::unique_ptr<TipsService> service_;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ukm::TestAutoSetUkmRecorder ukm_recorder_;
  std::unique_ptr<segmentation_platform::UkmDataManagerTestUtils> test_utils_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<segmentation_platform::SegmentationPlatformService>
      real_segmentation_service_;
};

}  // namespace tips

#endif  // CHROME_BROWSER_TIPS_CORE_TIPS_SERVICE_TEST_BASE_H_
