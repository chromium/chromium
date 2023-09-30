// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_observer.h"
#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/client_result_prefs.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/prediction_options.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/segmentation_platform/public/service_proxy.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace segmentation_platform {
namespace {

// Observer that waits for service_ initialization.
class WaitServiceInitializedObserver : public ServiceProxy::Observer {
 public:
  explicit WaitServiceInitializedObserver(base::OnceClosure closure)
      : closure_(std::move(closure)) {}
  void OnServiceStatusChanged(bool initialized, int status_flags) override {
    if (initialized) {
      std::move(closure_).Run();
    }
  }

 private:
  base::OnceClosure closure_;
};

}  // namespace

class SegmentationPlatformServiceFactoryTest : public testing::Test {
 protected:
  SegmentationPlatformServiceFactoryTest() {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        kSegmentationPlatformRefreshResultsSwitch);
  }

  ~SegmentationPlatformServiceFactoryTest() override = default;

  void TearDown() override {
    service_ = nullptr;
    testing_profile_.reset();
    task_environment_.RunUntilIdle();
  }

  void InitServiceAndCacheResults(const std::string& segmentation_key) {
    InitService();
    WaitForClientResultPrefUpdate(segmentation_key);
    // Getting the updated prefs from this session to be copied to the next
    // session. In the test environment, new session doesn't have prefs from
    // previous session, hence copying is required to get the cached result from
    // last session.
    const std::string output =
        testing_profile_->GetPrefs()->GetString(kSegmentationClientResultPrefs);

    // TODO(b/297091996): Remove this when leak is fixed.
    task_environment_.RunUntilIdle();

    service_ = nullptr;
    testing_profile_ = nullptr;

    // Creating profile and initialising segmentation service again with prefs
    // from the last session.
    TestingProfile::Builder profile_builder1;
    testing_profile_ = profile_builder1.Build();
    // Copying the prefs from last session.
    testing_profile_->GetPrefs()->SetString(kSegmentationClientResultPrefs,
                                            output);
    service_ = SegmentationPlatformServiceFactory::GetForProfile(
        testing_profile_.get());
    WaitForServiceInit();
    // TODO(b/297091996): Remove this when leak is fixed.
    task_environment_.RunUntilIdle();
  }

  void InitService() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{optimization_guide::features::kOptimizationTargetPrediction, {}},
         {features::kSegmentationPlatformFeature, {}},
         {features::kSegmentationPlatformUkmEngine, {}}},
        {});

    // Creating profile and initialising segmentation service.
    TestingProfile::Builder profile_builder;
    testing_profile_ = profile_builder.Build();
    service_ = SegmentationPlatformServiceFactory::GetForProfile(
        testing_profile_.get());
    WaitForServiceInit();
    // TODO(b/297091996): Remove this when leak is fixed.
    task_environment_.RunUntilIdle();
  }

  void ExpectGetClassificationResult(
      const std::string& segmentation_key,
      const PredictionOptions& prediction_options,
      scoped_refptr<InputContext> input_context,
      PredictionStatus expected_status,
      absl::optional<std::vector<std::string>> expected_labels) {
    base::RunLoop loop;
    service_->GetClassificationResult(
        segmentation_key, prediction_options, input_context,
        base::BindOnce(
            &SegmentationPlatformServiceFactoryTest::OnGetClassificationResult,
            base::Unretained(this), loop.QuitClosure(), expected_status,
            expected_labels));
    loop.Run();
  }

  void OnGetClassificationResult(
      base::RepeatingClosure closure,
      PredictionStatus expected_status,
      absl::optional<std::vector<std::string>> expected_labels,
      const ClassificationResult& actual_result) {
    EXPECT_EQ(actual_result.status, expected_status);
    if (expected_labels.has_value()) {
      EXPECT_EQ(actual_result.ordered_labels, expected_labels.value());
    }
    std::move(closure).Run();
  }

  void ExpectGetAnnotatedNumericResult(
      const std::string& segmentation_key,
      const PredictionOptions& prediction_options,
      scoped_refptr<InputContext> input_context,
      PredictionStatus expected_status) {
    base::RunLoop loop;
    service_->GetAnnotatedNumericResult(
        segmentation_key, prediction_options, input_context,
        base::BindOnce(&SegmentationPlatformServiceFactoryTest::
                           OnGetAnnotatedNumericResult,
                       base::Unretained(this), loop.QuitClosure(),
                       expected_status));
    loop.Run();
  }

  void OnGetAnnotatedNumericResult(
      base::RepeatingClosure closure,
      PredictionStatus expected_status,
      const AnnotatedNumericResult& actual_result) {
    ASSERT_EQ(expected_status, actual_result.status);
    std::move(closure).Run();
  }

  void WaitForServiceInit() {
    base::RunLoop wait_for_init;
    WaitServiceInitializedObserver wait_observer(wait_for_init.QuitClosure());
    service_->GetServiceProxy()->AddObserver(&wait_observer);

    wait_for_init.Run();
    ASSERT_TRUE(service_->IsPlatformInitialized());
    service_->GetServiceProxy()->RemoveObserver(&wait_observer);
  }

  bool HasClientResultPref(const std::string& segmentation_key) {
    PrefService* pref_service_ = testing_profile_->GetPrefs();
    std::unique_ptr<ClientResultPrefs> result_prefs_ =
        std::make_unique<ClientResultPrefs>(pref_service_);
    return result_prefs_->ReadClientResultFromPrefs(segmentation_key)
        .has_value();
  }

  void OnClientResultPrefUpdated(const std::string& segmentation_key) {
    if (!wait_for_pref_callback_.is_null() &&
        HasClientResultPref(segmentation_key)) {
      std::move(wait_for_pref_callback_).Run();
    }
  }

  void WaitForClientResultPrefUpdate(const std::string& segmentation_key) {
    if (HasClientResultPref(segmentation_key)) {
      return;
    }

    base::RunLoop wait_for_pref;
    wait_for_pref_callback_ = wait_for_pref.QuitClosure();
    pref_registrar_.Init(testing_profile_->GetPrefs());
    pref_registrar_.Add(
        kSegmentationClientResultPrefs,
        base::BindRepeating(
            &SegmentationPlatformServiceFactoryTest::OnClientResultPrefUpdated,
            base::Unretained(this), segmentation_key));
    wait_for_pref.Run();

    pref_registrar_.RemoveAll();
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
  PrefChangeRegistrar pref_registrar_;
  base::OnceClosure wait_for_pref_callback_;
  std::unique_ptr<TestingProfile> testing_profile_;
  raw_ptr<SegmentationPlatformService> service_;
};

TEST_F(SegmentationPlatformServiceFactoryTest, TestPasswordManagerUserSegment) {
  InitServiceAndCacheResults(kPasswordManagerUserKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kPasswordManagerUserKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, "Not_PasswordManagerUser"));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestSearchUserModel) {
  InitServiceAndCacheResults(kSearchUserKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kSearchUserKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, kSearchUserModelLabelNone));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestShoppingUserModel) {
  InitServiceAndCacheResults(kShoppingUserSegmentationKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kShoppingUserSegmentationKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, kLegacyNegativeLabel));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestResumeHeavyUserModel) {
  InitServiceAndCacheResults(kResumeHeavyUserKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kResumeHeavyUserKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, kLegacyNegativeLabel));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestLowUserEngagementModel) {
  InitServiceAndCacheResults(kChromeLowUserEngagementSegmentationKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kChromeLowUserEngagementSegmentationKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, kChromeLowUserEngagementUmaName));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestCrossDeviceModel) {
  InitServiceAndCacheResults(segmentation_platform::kCrossDeviceUserKey);

  segmentation_platform::PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      segmentation_platform::kCrossDeviceUserKey, prediction_options, nullptr,
      /*expected_status=*/segmentation_platform::PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, segmentation_platform::kNoCrossDeviceUsage));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestDeviceSwitcherModel) {
  InitService();

  PredictionOptions prediction_options;
  prediction_options.on_demand_execution = true;

  auto input_context = base::MakeRefCounted<InputContext>();
  input_context->metadata_args.emplace("wait_for_device_info_in_seconds",
                                       processing::ProcessedValue(0));

  ExpectGetClassificationResult(
      kDeviceSwitcherKey, prediction_options, input_context,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/std::vector<std::string>(1, "NotSynced"));
}

#if BUILDFLAG(IS_ANDROID)
// Tests for models in android platform.
TEST_F(SegmentationPlatformServiceFactoryTest, TestDeviceTierSegment) {
  InitServiceAndCacheResults(kDeviceTierKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kDeviceTierKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/absl::nullopt);
}

TEST_F(SegmentationPlatformServiceFactoryTest,
       TestTabletProductivityUserModel) {
  InitServiceAndCacheResults(kTabletProductivityUserKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kTabletProductivityUserKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, kTabletProductivityUserModelLabelNone));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestFrequentFeatureModel) {
  InitServiceAndCacheResults(kFrequentFeatureUserKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kFrequentFeatureUserKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>{kLegacyNegativeLabel});
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestIntentionalUserModel) {
  InitServiceAndCacheResults(segmentation_platform::kIntentionalUserKey);

  segmentation_platform::PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      segmentation_platform::kIntentionalUserKey, prediction_options, nullptr,
      /*expected_status=*/segmentation_platform::PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>(1, kLegacyNegativeLabel));
}

TEST_F(SegmentationPlatformServiceFactoryTest, TestPowerUserSegment) {
  InitServiceAndCacheResults(kPowerUserKey);

  PredictionOptions prediction_options;

  ExpectGetClassificationResult(
      kPowerUserKey, prediction_options, nullptr,
      /*expected_status=*/PredictionStatus::kSucceeded,
      /*expected_labels=*/
      std::vector<std::string>{"None"});
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace segmentation_platform
