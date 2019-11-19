// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/optimization_hints_component_installer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/optimization_guide/optimization_guide_constants.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

static const char kTestHintsVersion[] = "1.2.3";

class TestOptimizationGuideService
    : public optimization_guide::OptimizationGuideService {
 public:
  explicit TestOptimizationGuideService(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner)
      : optimization_guide::OptimizationGuideService(io_thread_task_runner) {}
  ~TestOptimizationGuideService() override {}

  void MaybeUpdateHintsComponent(
      const optimization_guide::HintsComponentInfo& info) override {
    hints_component_info_ =
        std::make_unique<optimization_guide::HintsComponentInfo>(info);
  }

  optimization_guide::HintsComponentInfo* hints_component_info() const {
    return hints_component_info_.get();
  }

 private:
  std::unique_ptr<optimization_guide::HintsComponentInfo> hints_component_info_;

  DISALLOW_COPY_AND_ASSIGN(TestOptimizationGuideService);
};

class OptimizationHintsMockComponentUpdateService
    : public component_updater::MockComponentUpdateService {
 public:
  OptimizationHintsMockComponentUpdateService() {}
  ~OptimizationHintsMockComponentUpdateService() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(OptimizationHintsMockComponentUpdateService);
};

}  // namespace

namespace component_updater {

class OptimizationHintsComponentInstallerTest : public PlatformTest {
 public:
  OptimizationHintsComponentInstallerTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());

    auto optimization_guide_service =
        std::make_unique<TestOptimizationGuideService>(
            base::ThreadTaskRunnerHandle::Get());
    optimization_guide_service_ = optimization_guide_service.get();

    TestingBrowserProcess::GetGlobal()->SetOptimizationGuideService(
        std::move(optimization_guide_service));
    policy_ = std::make_unique<OptimizationHintsComponentInstallerPolicy>();

    drp_test_context_ =
        data_reduction_proxy::DataReductionProxyTestContext::Builder()
            .WithMockConfig()
            .Build();
    drp_test_context_->DisableWarmupURLFetch();
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetOptimizationGuideService(nullptr);
    drp_test_context_->DestroySettings();
    PlatformTest::TearDown();
  }

  TestOptimizationGuideService* service() {
    return optimization_guide_service_;
  }

  base::FilePath component_install_dir() {
    return component_install_dir_.GetPath();
  }

  TestingPrefServiceSimple* profile_prefs() {
    return drp_test_context_->pref_service();
  }

  base::Version ruleset_format_version() {
    return policy_->ruleset_format_version_;
  }

  void SetDataSaverEnabled(bool enabled) {
    drp_test_context_->SetDataReductionProxyEnabled(enabled);
  }

  void CreateTestOptimizationHints(const std::string& hints_content) {
    base::FilePath hints_path = component_install_dir().Append(
        optimization_guide::kUnindexedHintsFileName);
    ASSERT_EQ(static_cast<int32_t>(hints_content.length()),
              base::WriteFile(hints_path, hints_content.data(),
                              hints_content.length()));
  }

  void LoadOptimizationHints(const base::Version& ruleset_format) {
    std::unique_ptr<base::DictionaryValue> manifest(new base::DictionaryValue);
    if (ruleset_format.IsValid()) {
      manifest->SetString(
          OptimizationHintsComponentInstallerPolicy::kManifestRulesetFormatKey,
          ruleset_format.GetString());
    }
    ASSERT_TRUE(
        policy_->VerifyInstallation(*manifest, component_install_dir()));
    const base::Version expected_version(kTestHintsVersion);
    policy_->ComponentReady(expected_version, component_install_dir(),
                            std::move(manifest));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir component_install_dir_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;

  std::unique_ptr<OptimizationHintsComponentInstallerPolicy> policy_;

  std::unique_ptr<data_reduction_proxy::DataReductionProxyTestContext>
      drp_test_context_;

  TestOptimizationGuideService* optimization_guide_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(OptimizationHintsComponentInstallerTest);
};

TEST_F(OptimizationHintsComponentInstallerTest,
       ComponentRegistrationWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      optimization_guide::features::kOptimizationHints);
  std::unique_ptr<OptimizationHintsMockComponentUpdateService> cus(
      new OptimizationHintsMockComponentUpdateService());
  EXPECT_CALL(*cus, RegisterComponent(testing::_)).Times(0);
  RegisterOptimizationHintsComponent(cus.get(), false, profile_prefs());
  RunUntilIdle();
}

TEST_F(OptimizationHintsComponentInstallerTest,
       ComponentRegistrationWhenFeatureEnabledButDataSaverDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHints);
  SetDataSaverEnabled(false);
  std::unique_ptr<OptimizationHintsMockComponentUpdateService> cus(
      new OptimizationHintsMockComponentUpdateService());
  EXPECT_CALL(*cus, RegisterComponent(testing::_)).Times(0);
  RegisterOptimizationHintsComponent(cus.get(), false, profile_prefs());
  RunUntilIdle();
}

TEST_F(OptimizationHintsComponentInstallerTest,
       ComponentRegistrationWhenFeatureEnabledButNoProfilePrefs) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHints);
  std::unique_ptr<OptimizationHintsMockComponentUpdateService> cus(
      new OptimizationHintsMockComponentUpdateService());
  EXPECT_CALL(*cus, RegisterComponent(testing::_)).Times(0);
  RegisterOptimizationHintsComponent(cus.get(), false, nullptr);
  RunUntilIdle();
}

TEST_F(OptimizationHintsComponentInstallerTest,
       ComponentRegistrationWhenFeatureEnabledAndDataSaverEnabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      optimization_guide::features::kOptimizationHints);
  SetDataSaverEnabled(true);
  std::unique_ptr<OptimizationHintsMockComponentUpdateService> cus(
      new OptimizationHintsMockComponentUpdateService());
  EXPECT_CALL(*cus, RegisterComponent(testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  RegisterOptimizationHintsComponent(cus.get(), false, profile_prefs());
  RunUntilIdle();
}

TEST_F(OptimizationHintsComponentInstallerTest, NoRulesetFormatIgnored) {
  ASSERT_TRUE(service());
  ASSERT_NO_FATAL_FAILURE(CreateTestOptimizationHints("some hints"));

  ASSERT_NO_FATAL_FAILURE(LoadOptimizationHints(base::Version("")));
  EXPECT_EQ(nullptr, service()->hints_component_info());
}

TEST_F(OptimizationHintsComponentInstallerTest, FutureRulesetFormatIgnored) {
  ASSERT_TRUE(service());
  ASSERT_NO_FATAL_FAILURE(CreateTestOptimizationHints("some hints"));
  base::Version version = ruleset_format_version();
  const std::vector<uint32_t> future_ruleset_components = {
      version.components()[0] + 1, version.components()[1],
      version.components()[2]};

  ASSERT_NO_FATAL_FAILURE(
      LoadOptimizationHints(base::Version(future_ruleset_components)));
  EXPECT_EQ(nullptr, service()->hints_component_info());
}

TEST_F(OptimizationHintsComponentInstallerTest, LoadFileWithData) {
  ASSERT_TRUE(service());

  const std::string expected_hints = "some hints";
  ASSERT_NO_FATAL_FAILURE(CreateTestOptimizationHints(expected_hints));
  ASSERT_NO_FATAL_FAILURE(LoadOptimizationHints(ruleset_format_version()));

  auto* component_info = service()->hints_component_info();
  EXPECT_NE(nullptr, component_info);
  EXPECT_EQ(base::Version(kTestHintsVersion), component_info->version);
  std::string actual_hints;
  ASSERT_TRUE(base::ReadFileToString(component_info->path, &actual_hints));
  EXPECT_EQ(expected_hints, actual_hints);
}

}  // namespace component_updater
