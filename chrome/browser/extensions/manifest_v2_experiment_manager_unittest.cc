// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension_features.h"

namespace extensions {

class ManifestV2ExperimentManagerUnitTestBase
    : public ExtensionServiceTestBase {
 public:
  explicit ManifestV2ExperimentManagerUnitTestBase(bool feature_enabled);
  ~ManifestV2ExperimentManagerUnitTestBase() override = default;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();

    experiment_manager_ = ManifestV2ExperimentManager::Get(profile());
  }

  void TearDown() override {
    experiment_manager_ = nullptr;
    ExtensionServiceTestBase::TearDown();
  }

  ManifestV2ExperimentManager* experiment_manager() {
    return experiment_manager_.get();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<ManifestV2ExperimentManager> experiment_manager_;
};

ManifestV2ExperimentManagerUnitTestBase::
    ManifestV2ExperimentManagerUnitTestBase(bool feature_enabled) {
  feature_list_.InitWithFeatureState(
      extensions_features::kExtensionManifestV2DeprecationWarning,
      feature_enabled);
}

class ManifestV2ExperimentManagerUnitTest
    : public ManifestV2ExperimentManagerUnitTestBase {
 public:
  ManifestV2ExperimentManagerUnitTest()
      : ManifestV2ExperimentManagerUnitTestBase(true) {}
  ~ManifestV2ExperimentManagerUnitTest() override = default;
};

class ManifestV2ExperimentManagerDisabledUnitTest
    : public ManifestV2ExperimentManagerUnitTestBase {
 public:
  ManifestV2ExperimentManagerDisabledUnitTest()
      : ManifestV2ExperimentManagerUnitTestBase(false) {}
  ~ManifestV2ExperimentManagerDisabledUnitTest() override = default;
};

// Tests that the experiment stage is properly set when the manifest V2
// deprecation warning experiment is enabled.
TEST_F(ManifestV2ExperimentManagerUnitTest, ExperimentStageIsSetToWarning) {
  EXPECT_EQ(MV2ExperimentStage::kWarning,
            experiment_manager()->GetCurrentExperimentStage());
}

// Tests that the experiment stage is properly set when the manifest V2
// deprecation warning experiment is disabled.
TEST_F(ManifestV2ExperimentManagerDisabledUnitTest,
       ExperimentStageIsSetToNone) {
  EXPECT_EQ(MV2ExperimentStage::kNone,
            experiment_manager()->GetCurrentExperimentStage());
}

}  // namespace extensions
