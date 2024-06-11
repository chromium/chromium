// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/manifest.mojom.h"

namespace extensions {

class ManifestV2ExperimentManagerUnitTestBase
    : public ExtensionServiceTestBase {
 public:
  ManifestV2ExperimentManagerUnitTestBase(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features);
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
    ManifestV2ExperimentManagerUnitTestBase(
        const std::vector<base::test::FeatureRef>& enabled_features,
        const std::vector<base::test::FeatureRef>& disabled_features) {
  feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

// Test suite for cases where the user is in the "warning" experiment phase.
class ManifestV2ExperimentManagerWarningUnitTest
    : public ManifestV2ExperimentManagerUnitTestBase {
 public:
  ManifestV2ExperimentManagerWarningUnitTest()
      : ManifestV2ExperimentManagerUnitTestBase(
            {extensions_features::kExtensionManifestV2DeprecationWarning},
            {extensions_features::kExtensionManifestV2Disabled}) {}
  ~ManifestV2ExperimentManagerWarningUnitTest() override = default;
};

// Test suite for cases where the user is not in any experiment phase; i.e., the
// experiment is disabled.
class ManifestV2ExperimentManagerDisabledUnitTest
    : public ManifestV2ExperimentManagerUnitTestBase {
 public:
  ManifestV2ExperimentManagerDisabledUnitTest()
      : ManifestV2ExperimentManagerUnitTestBase(
            {},
            {extensions_features::kExtensionManifestV2DeprecationWarning,
             extensions_features::kExtensionManifestV2Disabled}) {}
  ~ManifestV2ExperimentManagerDisabledUnitTest() override = default;
};

// Test suite for cases where the user is in the "disable with re-enable"
// experiment phase.
class ManifestV2ExperimentManagerDisableWithReEnableUnitTest
    : public ManifestV2ExperimentManagerUnitTestBase {
 public:
  ManifestV2ExperimentManagerDisableWithReEnableUnitTest()
      : ManifestV2ExperimentManagerUnitTestBase(
            {extensions_features::kExtensionManifestV2Disabled},
            {}) {}
  ~ManifestV2ExperimentManagerDisableWithReEnableUnitTest() override = default;
};

// Test suite for cases where the user is in the "disable with re-enable"
// experiment phase *and* the warning experiment is still active.
class ManifestV2ExperimentManagerDisableWithReEnableAndWarningUnitTest
    : public ManifestV2ExperimentManagerUnitTestBase {
 public:
  ManifestV2ExperimentManagerDisableWithReEnableAndWarningUnitTest()
      : ManifestV2ExperimentManagerUnitTestBase(
            {extensions_features::kExtensionManifestV2Disabled,
             extensions_features::kExtensionManifestV2DeprecationWarning},
            {}) {}
  ~ManifestV2ExperimentManagerDisableWithReEnableAndWarningUnitTest() override =
      default;
};

// Tests that the experiment stage is properly set when the manifest V2
// deprecation warning experiment is enabled.
TEST_F(ManifestV2ExperimentManagerWarningUnitTest,
       ExperimentStageIsSetToWarning) {
  EXPECT_EQ(MV2ExperimentStage::kWarning,
            experiment_manager()->GetCurrentExperimentStage());
}

// Sanity check that MV2 extensions are considered affected when the
// experiment is enabled. The "is affected" logic is much more heavily tested
// in mv2_deprecation_impact_checker_unittest.cc.
TEST_F(ManifestV2ExperimentManagerWarningUnitTest, MV2ExtensionsAreAffected) {
  struct {
    mojom::ManifestLocation manifest_location;
    const char* name;
  } test_cases[] = {
      {mojom::ManifestLocation::kInternal, "internal"},
      {mojom::ManifestLocation::kExternalPref, "external pref"},
      {mojom::ManifestLocation::kExternalRegistry, "external registry"},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    scoped_refptr<const Extension> mv2_extension =
        ExtensionBuilder(test_case.name)
            .SetManifestVersion(2)
            .SetLocation(test_case.manifest_location)
            .Build();
    EXPECT_TRUE(experiment_manager()->IsExtensionAffected(*mv2_extension));

    scoped_refptr<const Extension> mv3_extension =
        ExtensionBuilder(test_case.name)
            .SetManifestVersion(3)
            .SetLocation(test_case.manifest_location)
            .Build();
    EXPECT_FALSE(experiment_manager()->IsExtensionAffected(*mv3_extension));
  }
}

TEST_F(ManifestV2ExperimentManagerWarningUnitTest,
       MarkingWarningsAsAcknowledged) {
  scoped_refptr<const Extension> ext1 =
      ExtensionBuilder("one")
          .SetManifestVersion(2)
          .SetLocation(mojom::ManifestLocation::kInternal)
          .Build();
  scoped_refptr<const Extension> ext2 =
      ExtensionBuilder("two")
          .SetManifestVersion(2)
          .SetLocation(mojom::ManifestLocation::kInternal)
          .Build();

  service()->AddExtension(ext1.get());
  service()->AddExtension(ext2.get());

  EXPECT_FALSE(experiment_manager()->DidUserAcknowledgeWarning(ext1->id()));
  EXPECT_FALSE(experiment_manager()->DidUserAcknowledgeWarning(ext2->id()));

  experiment_manager()->MarkWarningAsAcknowledged(ext1->id());

  EXPECT_TRUE(experiment_manager()->DidUserAcknowledgeWarning(ext1->id()));
  EXPECT_FALSE(experiment_manager()->DidUserAcknowledgeWarning(ext2->id()));
}

TEST_F(ManifestV2ExperimentManagerWarningUnitTest,
       MarkingGeneralWwarningAsAcknowledged) {
  EXPECT_FALSE(experiment_manager()->DidUserAcknowledgeWarningGlobally());
  experiment_manager()->MarkWarningAsAcknowledgedGlobally();
  EXPECT_TRUE(experiment_manager()->DidUserAcknowledgeWarningGlobally());
}

// Tests that the experiment stage is properly set when the manifest V2
// deprecation warning experiment is disabled.
TEST_F(ManifestV2ExperimentManagerDisabledUnitTest,
       ExperimentStageIsSetToNone) {
  EXPECT_EQ(MV2ExperimentStage::kNone,
            experiment_manager()->GetCurrentExperimentStage());
}

// Sanity check that no extensions are considered affected when the
// experiment is disabled. The "is affected" logic is much more heavily tested
// in mv2_deprecation_impact_checker_unittest.cc.
TEST_F(ManifestV2ExperimentManagerDisabledUnitTest, NoExtensionsAreAffected) {
  struct {
    mojom::ManifestLocation manifest_location;
    const char* name;
  } test_cases[] = {
      {mojom::ManifestLocation::kInternal, "internal"},
      {mojom::ManifestLocation::kExternalPref, "external pref"},
      {mojom::ManifestLocation::kExternalRegistry, "external registry"},
      {mojom::ManifestLocation::kExternalComponent, "external component"},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    scoped_refptr<const Extension> mv2_extension =
        ExtensionBuilder(test_case.name)
            .SetManifestVersion(2)
            .SetLocation(test_case.manifest_location)
            .Build();
    EXPECT_FALSE(experiment_manager()->IsExtensionAffected(*mv2_extension));

    scoped_refptr<const Extension> mv3_extension =
        ExtensionBuilder(test_case.name)
            .SetManifestVersion(3)
            .SetLocation(test_case.manifest_location)
            .Build();
    EXPECT_FALSE(experiment_manager()->IsExtensionAffected(*mv3_extension));
  }
}

// Tests that the experiment phase is properly set for a user in the
// "disable with re-enable" experiment phase.
TEST_F(ManifestV2ExperimentManagerDisableWithReEnableUnitTest,
       ExperimentStageIsSetToDisableWithReEnable) {
  EXPECT_EQ(MV2ExperimentStage::kDisableWithReEnable,
            experiment_manager()->GetCurrentExperimentStage());
}

// Tests that the experiment phase is properly set for a user in the
// "disable with re-enable" experiment phase if the "warning" experiment is
// still active. That is, verifies that the "latest" stage takes precedence.
TEST_F(ManifestV2ExperimentManagerDisableWithReEnableAndWarningUnitTest,
       ExperimentStageIsSetToDisableWithReEnable) {
  EXPECT_EQ(MV2ExperimentStage::kDisableWithReEnable,
            experiment_manager()->GetCurrentExperimentStage());
}

// Sanity check that MV2 extensions are considered affected when the
// experiment is enabled in the "disable with re-enable" phase. The
// "is affected" logic is much more heavily tested
// in mv2_deprecation_impact_checker_unittest.cc.
TEST_F(ManifestV2ExperimentManagerDisableWithReEnableUnitTest,
       MV2ExtensionsAreAffected) {
  struct {
    mojom::ManifestLocation manifest_location;
    const char* name;
  } test_cases[] = {
      {mojom::ManifestLocation::kInternal, "internal"},
      {mojom::ManifestLocation::kExternalPref, "external pref"},
      {mojom::ManifestLocation::kExternalRegistry, "external registry"},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    scoped_refptr<const Extension> mv2_extension =
        ExtensionBuilder(test_case.name)
            .SetManifestVersion(2)
            .SetLocation(test_case.manifest_location)
            .Build();
    EXPECT_TRUE(experiment_manager()->IsExtensionAffected(*mv2_extension));

    scoped_refptr<const Extension> mv3_extension =
        ExtensionBuilder(test_case.name)
            .SetManifestVersion(3)
            .SetLocation(test_case.manifest_location)
            .Build();
    EXPECT_FALSE(experiment_manager()->IsExtensionAffected(*mv3_extension));
  }
}

}  // namespace extensions
