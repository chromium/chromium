// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/android/optimization_guide_bridge.h"

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/optimization_guide/android/native_j_unittests_jni_headers/OptimizationGuideBridgeNativeUnitTest_jni.h"
#include "chrome/browser/optimization_guide/chrome_hints_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ByRef;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::UnorderedElementsAre;

namespace optimization_guide {
namespace android {

class MockOptimizationGuideHintsManager
    : public optimization_guide::ChromeHintsManager {
 public:
  MockOptimizationGuideHintsManager(Profile* profile, PrefService* pref_service)
      : optimization_guide::ChromeHintsManager(
            profile,
            pref_service,
            /*hint_store=*/nullptr,
            /*top_host_provider=*/nullptr,
            /*tab_url_provider=*/nullptr,
            /*url_loader_factory=*/nullptr,
            /*push_notification_manager=*/nullptr,
            /*optimization_guide_logger=*/nullptr) {}
  ~MockOptimizationGuideHintsManager() override = default;
  MOCK_METHOD3(CanApplyOptimizationAsync,
               void(const GURL&,
                    optimization_guide::proto::OptimizationType,
                    optimization_guide::OptimizationGuideDecisionCallback));
};

class MockOptimizationGuideKeyedService : public OptimizationGuideKeyedService {
 public:
  explicit MockOptimizationGuideKeyedService(
      content::BrowserContext* browser_context)
      : OptimizationGuideKeyedService(browser_context) {}
  ~MockOptimizationGuideKeyedService() override = default;

  MOCK_METHOD0(GetHintsManager, optimization_guide::ChromeHintsManager*());
  MOCK_METHOD1(
      RegisterOptimizationTypes,
      void(const std::vector<optimization_guide::proto::OptimizationType>&));
  MOCK_METHOD3(CanApplyOptimization,
               optimization_guide::OptimizationGuideDecision(
                   const GURL& gurl,
                   optimization_guide::proto::OptimizationType,
                   optimization_guide::OptimizationMetadata* metadata));
};

class OptimizationGuideBridgeTest : public testing::Test {
 public:
  OptimizationGuideBridgeTest()
      : j_test_(Java_OptimizationGuideBridgeNativeUnitTest_Constructor(
            base::android::AttachCurrentThread())),
        env_(base::android::AttachCurrentThread()),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~OptimizationGuideBridgeTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(profile_manager_.SetUp(temp_dir_.GetPath()));
    profile_ = profile_manager_.CreateTestingProfile(chrome::kInitialProfile);
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    optimization_guide::prefs::RegisterProfilePrefs(pref_service_->registry());

    optimization_guide_keyed_service_ =
        static_cast<MockOptimizationGuideKeyedService*>(
            OptimizationGuideKeyedServiceFactory::GetInstance()
                ->SetTestingFactoryAndUse(
                    profile_,
                    base::BindRepeating([](content::BrowserContext* context)
                                            -> std::unique_ptr<KeyedService> {
                      return std::make_unique<
                          MockOptimizationGuideKeyedService>(context);
                    })));
    optimization_guide_hints_manager_ =
        std::make_unique<MockOptimizationGuideHintsManager>(
            profile_, pref_service_.get());
  }

  void TearDown() override {
    optimization_guide_hints_manager_->Shutdown();
    optimization_guide_hints_manager_.reset();
  }

  void RegisterOptimizationTypes() {
    optimization_guide_keyed_service_->RegisterOptimizationTypes(
        {optimization_guide::proto::DEFER_ALL_SCRIPT,
         optimization_guide::proto::LOADING_PREDICTOR});
  }

 protected:
  base::android::ScopedJavaGlobalRef<jobject> j_test_;
  raw_ptr<JNIEnv> env_;
  raw_ptr<MockOptimizationGuideKeyedService> optimization_guide_keyed_service_;
  std::unique_ptr<MockOptimizationGuideHintsManager>
      optimization_guide_hints_manager_;

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(OptimizationGuideBridgeTest, RegisterOptimizationTypes) {
  EXPECT_CALL(*optimization_guide_keyed_service_,
              RegisterOptimizationTypes(UnorderedElementsAre(
                  optimization_guide::proto::LOADING_PREDICTOR,
                  optimization_guide::proto::DEFER_ALL_SCRIPT)));

  Java_OptimizationGuideBridgeNativeUnitTest_testRegisterOptimizationTypes(
      env_, j_test_);
}

TEST_F(OptimizationGuideBridgeTest, CanApplyOptimizationAsyncHasHint) {
  RegisterOptimizationTypes();
  EXPECT_CALL(*optimization_guide_keyed_service_, GetHintsManager())
      .WillRepeatedly(Return(optimization_guide_hints_manager_.get()));
  optimization_guide::proto::LoadingPredictorMetadata hints_metadata;
  optimization_guide::OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(hints_metadata);
  EXPECT_CALL(
      *optimization_guide_hints_manager_,
      CanApplyOptimizationAsync(GURL("https://example.com/"),
                                optimization_guide::proto::LOADING_PREDICTOR,
                                base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<2>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          ByRef(metadata)));

  Java_OptimizationGuideBridgeNativeUnitTest_testCanApplyOptimizationAsyncHasHint(
      env_, j_test_);
}

TEST_F(OptimizationGuideBridgeTest, CanApplyOptimizationHasHint) {
  RegisterOptimizationTypes();
  optimization_guide::proto::LoadingPredictorMetadata hints_metadata;
  optimization_guide::OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(hints_metadata);

  ON_CALL(*optimization_guide_keyed_service_,
          CanApplyOptimization(GURL("https://example.com/"),
                               optimization_guide::proto::LOADING_PREDICTOR,
                               NotNull()))
      .WillByDefault(
          DoAll(SetArgPointee<2>(metadata),
                Return(optimization_guide::OptimizationGuideDecision::kTrue)));

  Java_OptimizationGuideBridgeNativeUnitTest_testCanApplyOptimizationHasHint(
      env_, j_test_);
}

}  // namespace android
}  // namespace optimization_guide
