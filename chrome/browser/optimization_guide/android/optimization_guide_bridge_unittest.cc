// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/android/optimization_guide_bridge.h"

#include "base/android/jni_android.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/optimization_guide/android/native_j_unittests_jni_headers/OptimizationGuideBridgeNativeUnitTest_jni.h"
#include "chrome/browser/optimization_guide/optimization_guide_hints_manager.h"
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
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

namespace optimization_guide {
namespace android {

class MockOptimizationGuideHintsManager : public OptimizationGuideHintsManager {
 public:
  MockOptimizationGuideHintsManager(Profile* profile, PrefService* pref_service)
      : OptimizationGuideHintsManager(profile,
                                      pref_service,
                                      /*hint_store=*/nullptr,
                                      /*top_host_provider=*/nullptr,
                                      /*tab_url_provider=*/nullptr,
                                      /*url_loader_factory=*/nullptr) {}
  ~MockOptimizationGuideHintsManager() override = default;
  MOCK_METHOD4(CanApplyOptimizationAsync,
               void(const GURL&,
                    const base::Optional<int64_t>&,
                    optimization_guide::proto::OptimizationType,
                    optimization_guide::OptimizationGuideDecisionCallback));
};

class MockOptimizationGuideKeyedService : public OptimizationGuideKeyedService {
 public:
  explicit MockOptimizationGuideKeyedService(
      content::BrowserContext* browser_context)
      : OptimizationGuideKeyedService(browser_context) {}
  ~MockOptimizationGuideKeyedService() override = default;

  MOCK_METHOD0(GetHintsManager, OptimizationGuideHintsManager*());
  MOCK_METHOD1(
      RegisterOptimizationTypes,
      void(const std::vector<optimization_guide::proto::OptimizationType>&));
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
         optimization_guide::proto::PERFORMANCE_HINTS});
  }

 protected:
  base::android::ScopedJavaGlobalRef<jobject> j_test_;
  JNIEnv* env_;
  MockOptimizationGuideKeyedService* optimization_guide_keyed_service_;
  std::unique_ptr<MockOptimizationGuideHintsManager>
      optimization_guide_hints_manager_;

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(OptimizationGuideBridgeTest, RegisterOptimizationTypes) {
  EXPECT_CALL(*optimization_guide_keyed_service_,
              RegisterOptimizationTypes(UnorderedElementsAre(
                  optimization_guide::proto::PERFORMANCE_HINTS,
                  optimization_guide::proto::DEFER_ALL_SCRIPT)));

  Java_OptimizationGuideBridgeNativeUnitTest_testRegisterOptimizationTypes(
      env_, j_test_);
}

TEST_F(OptimizationGuideBridgeTest, CanApplyOptimizationPreInit) {
  EXPECT_CALL(*optimization_guide_keyed_service_, GetHintsManager())
      .WillOnce(Return(nullptr));

  RegisterOptimizationTypes();
  Java_OptimizationGuideBridgeNativeUnitTest_testCanApplyOptimizationPreInit(
      env_, j_test_);
}

TEST_F(OptimizationGuideBridgeTest, CanApplyOptimizationHasHint) {
  RegisterOptimizationTypes();
  EXPECT_CALL(*optimization_guide_keyed_service_, GetHintsManager())
      .Times(2)
      .WillRepeatedly(Return(optimization_guide_hints_manager_.get()));
  optimization_guide::proto::PerformanceHintsMetadata hints_metadata;
  auto* hint = hints_metadata.add_performance_hints();
  hint->set_wildcard_pattern("test.com");
  hint->set_performance_class(optimization_guide::proto::PERFORMANCE_SLOW);
  optimization_guide::OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(hints_metadata);
  EXPECT_CALL(
      *optimization_guide_hints_manager_,
      CanApplyOptimizationAsync(GURL("https://example.com/"), Eq(base::nullopt),
                                optimization_guide::proto::PERFORMANCE_HINTS,
                                base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<3>(
          optimization_guide::OptimizationGuideDecision::kTrue,
          ByRef(metadata)));

  Java_OptimizationGuideBridgeNativeUnitTest_testCanApplyOptimizationHasHint(
      env_, j_test_);
}

}  // namespace android
}  // namespace optimization_guide
