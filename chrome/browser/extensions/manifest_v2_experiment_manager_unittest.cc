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
#include "components/crx_file/id_util.h"
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
    // Even though the MV2 extension is affected by the experiment, it should
    // *not* be blocked from installation in the warning phase.
    EXPECT_FALSE(experiment_manager()->ShouldBlockExtensionInstallation(
        mv2_extension->id(), mv2_extension->manifest_version(),
        mv2_extension->GetType(), mv2_extension->location(),
        mv2_extension->hashed_id()));

    scoped_refptr<const Extension> mv3_extension =
        ExtensionBuilder(test_case.name)
            .SetManifestVersion(3)
            .SetLocation(test_case.manifest_location)
            .Build();
    EXPECT_FALSE(experiment_manager()->IsExtensionAffected(*mv3_extension));
    EXPECT_FALSE(experiment_manager()->ShouldBlockExtensionInstallation(
        mv3_extension->id(), mv3_extension->manifest_version(),
        mv3_extension->GetType(), mv3_extension->location(),
        mv3_extension->hashed_id()));
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
    EXPECT_FALSE(experiment_manager()->ShouldBlockExtensionInstallation(
        mv2_extension->id(), mv2_extension->manifest_version(),
        mv2_extension->GetType(), mv2_extension->location(),
        mv2_extension->hashed_id()));

    scoped_refptr<const Extension> mv3_extension =
        ExtensionBuilder(test_case.name)
            .SetManifestVersion(3)
            .SetLocation(test_case.manifest_location)
            .Build();
    EXPECT_FALSE(experiment_manager()->IsExtensionAffected(*mv3_extension));
    EXPECT_FALSE(experiment_manager()->ShouldBlockExtensionInstallation(
        mv3_extension->id(), mv3_extension->manifest_version(),
        mv3_extension->GetType(), mv3_extension->location(),
        mv3_extension->hashed_id()));
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

// Tests the manager properly indicates when to block user-installed extensions
// while the "soft disable" experiment stage is active.
TEST_F(ManifestV2ExperimentManagerDisableWithReEnableUnitTest,
       ShouldBlockInstallation_UserInstalledExtensions) {
  constexpr bool kInstallShouldBeBlocked = true;
  constexpr bool kInstallShouldBeAllowed = false;
  struct {
    mojom::ManifestLocation manifest_location;
    int manifest_version;
    const char* name;
    bool should_block_install;
  } test_cases[] = {
      // Unpacked extensions (including commandline-loaded extensions) should
      // still be installable. This allows developers to continue testing their
      // extensions during the experiment periods.
      {mojom::ManifestLocation::kUnpacked, 2, "unpacked - mv2",
       kInstallShouldBeAllowed},
      {mojom::ManifestLocation::kUnpacked, 3, "unpacked - mv3",
       kInstallShouldBeAllowed},
      {mojom::ManifestLocation::kCommandLine, 2, "command line - mv2",
       kInstallShouldBeAllowed},
      {mojom::ManifestLocation::kCommandLine, 3, "command line - mv3",
       kInstallShouldBeAllowed},

      // Other user-visible extension types should only be blocked if they are
      // MV2.
      {mojom::ManifestLocation::kInternal, 2, "internal - mv2",
       kInstallShouldBeBlocked},
      {mojom::ManifestLocation::kInternal, 3, "internal - mv3",
       kInstallShouldBeAllowed},
      {mojom::ManifestLocation::kExternalPref, 2, "external pref - mv2",
       kInstallShouldBeBlocked},
      {mojom::ManifestLocation::kExternalPref, 3, "external pref - mv3",
       kInstallShouldBeAllowed},
      {mojom::ManifestLocation::kExternalPref, 2, "external registry - mv2",
       kInstallShouldBeBlocked},
      {mojom::ManifestLocation::kExternalRegistry, 3, "external registry - mv3",
       kInstallShouldBeAllowed},
      {mojom::ManifestLocation::kExternalPrefDownload, 2,
       "external download - mv2", kInstallShouldBeBlocked},
      {mojom::ManifestLocation::kExternalPrefDownload, 3,
       "external download - mv3", kInstallShouldBeAllowed},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    ExtensionId extension_id = crx_file::id_util::GenerateId(test_case.name);

    EXPECT_EQ(
        test_case.should_block_install,
        experiment_manager()->ShouldBlockExtensionInstallation(
            extension_id, test_case.manifest_version, Manifest::TYPE_EXTENSION,
            test_case.manifest_location, HashedExtensionId(extension_id)));
  }
}

// Tests the manager never blocks component extensions from installing.
TEST_F(ManifestV2ExperimentManagerDisableWithReEnableUnitTest,
       ShouldBlockInstallation_ComponentExtensions) {
  struct {
    mojom::ManifestLocation manifest_location;
    int manifest_version;
    const char* name;
  } test_cases[] = {
      {mojom::ManifestLocation::kComponent, 2, "component - mv2"},
      {mojom::ManifestLocation::kComponent, 3, "component - mv3"},
      {mojom::ManifestLocation::kExternalComponent, 2,
       "external component - mv2"},
      {mojom::ManifestLocation::kExternalComponent, 3,
       "external component - mv3"},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    ExtensionId extension_id = crx_file::id_util::GenerateId(test_case.name);

    // Component extensions are built-in parts of Chrome that are extensions as
    // an implementation detail. They should always be allowed to install.
    EXPECT_FALSE(experiment_manager()->ShouldBlockExtensionInstallation(
        extension_id, test_case.manifest_version, Manifest::TYPE_EXTENSION,
        test_case.manifest_location, HashedExtensionId(extension_id)));
  }
}

// TODO(https://crbug.com/339061151): Add tests for policy-installed and policy-
// allowed extensions.

}  // namespace extensions
