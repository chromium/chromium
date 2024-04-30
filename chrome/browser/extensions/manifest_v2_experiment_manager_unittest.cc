// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
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

// Tests that user-visible MV2 extensions are properly considered affected by
// the MV2 deprecation experiment.
TEST_F(ManifestV2ExperimentManagerUnitTest,
       UserVisibleMV2ExtensionsAreAffected) {
  scoped_refptr<const Extension> user_installed =
      ExtensionBuilder("user installed")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .SetManifestVersion(2)
          .Build();
  scoped_refptr<const Extension> external_registry =
      ExtensionBuilder("external registry")
          .SetLocation(mojom::ManifestLocation::kExternalRegistry)
          .SetManifestVersion(2)
          .Build();
  scoped_refptr<const Extension> external_pref =
      ExtensionBuilder("external pref")
          .SetLocation(mojom::ManifestLocation::kExternalPref)
          .SetManifestVersion(2)
          .Build();
  scoped_refptr<const Extension> external_pref_download =
      ExtensionBuilder("external pref download")
          .SetLocation(mojom::ManifestLocation::kExternalPrefDownload)
          .SetManifestVersion(2)
          .Build();

  EXPECT_TRUE(experiment_manager()->IsExtensionAffected(*user_installed));
  EXPECT_TRUE(experiment_manager()->IsExtensionAffected(*external_registry));
  EXPECT_TRUE(experiment_manager()->IsExtensionAffected(*external_pref));
  EXPECT_TRUE(
      experiment_manager()->IsExtensionAffected(*external_pref_download));
}

// Tests that component extensions are not included in the MV2 deprecation
// experiment (they're implementation details of the browser).
TEST_F(ManifestV2ExperimentManagerUnitTest, ComponentExtensionsAreNotAffected) {
  scoped_refptr<const Extension> component =
      ExtensionBuilder("component")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .SetManifestVersion(2)
          .Build();
  scoped_refptr<const Extension> external_component =
      ExtensionBuilder("external component")
          .SetLocation(mojom::ManifestLocation::kExternalComponent)
          .SetManifestVersion(2)
          .Build();

  EXPECT_FALSE(experiment_manager()->IsExtensionAffected(*component));
  EXPECT_FALSE(experiment_manager()->IsExtensionAffected(*external_component));
}

// Tests that MV3 extensions, of any location, are not affected by the MV2
// deprecation experiment.
TEST_F(ManifestV2ExperimentManagerUnitTest, NoMV3ExtensionsAreAffected) {
  struct {
    mojom::ManifestLocation manifest_location;
    const char* name;
  } test_cases[] = {
      {mojom::ManifestLocation::kInternal, "internal"},
      {mojom::ManifestLocation::kExternalPref, "external pref"},
      {mojom::ManifestLocation::kExternalRegistry, "external registry"},
      {mojom::ManifestLocation::kUnpacked, "unpacked"},
      {mojom::ManifestLocation::kComponent, "component"},
      {mojom::ManifestLocation::kExternalPrefDownload,
       "external pref download"},
      {mojom::ManifestLocation::kExternalPolicyDownload,
       "external policy download"},
      {mojom::ManifestLocation::kCommandLine, "command line"},
      {mojom::ManifestLocation::kExternalPolicy, "external policy"},
      {mojom::ManifestLocation::kExternalComponent, "external component"},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(test_case.name)
            .SetManifestVersion(3)
            .SetLocation(test_case.manifest_location)
            .Build();
    EXPECT_FALSE(experiment_manager()->IsExtensionAffected(*extension));
  }
}

// Tests that non-extension "extension-like" things (such as platform apps and
// hosted apps) are not affected by the MV2 deprecation experiment.
TEST_F(ManifestV2ExperimentManagerUnitTest, NonExtensionsAreNotAffected) {
  scoped_refptr<const Extension> platform_app =
      ExtensionBuilder("app", ExtensionBuilder::Type::PLATFORM_APP)
          .SetManifestVersion(2)
          .Build();
  ASSERT_TRUE(platform_app->is_platform_app());

  static constexpr char kHostedAppManifest[] =
      R"({
           "name": "hosted app",
           "manifest_version": 2,
           "version": "0.1",
           "app": {"urls": ["http://example.com/"]}
         })";
  scoped_refptr<const Extension> hosted_app =
      ExtensionBuilder()
          .SetManifest(base::test::ParseJsonDict(kHostedAppManifest))
          .Build();
  ASSERT_TRUE(hosted_app->is_hosted_app());

  EXPECT_FALSE(experiment_manager()->IsExtensionAffected(*platform_app));
  EXPECT_FALSE(experiment_manager()->IsExtensionAffected(*hosted_app));
}

// Tests that the experiment stage is properly set when the manifest V2
// deprecation warning experiment is disabled.
TEST_F(ManifestV2ExperimentManagerDisabledUnitTest,
       ExperimentStageIsSetToNone) {
  EXPECT_EQ(MV2ExperimentStage::kNone,
            experiment_manager()->GetCurrentExperimentStage());
}

// Tests that we won't warn about any MV2 extensions when the experiment is
// disabled.
TEST_F(ManifestV2ExperimentManagerDisabledUnitTest,
       DontWarnAboutMV2ExtensionsWhenExperimentIsDisabled) {
  scoped_refptr<const Extension> user_installed =
      ExtensionBuilder("user installed")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .SetManifestVersion(2)
          .Build();
  scoped_refptr<const Extension> external_registry =
      ExtensionBuilder("external registry")
          .SetLocation(mojom::ManifestLocation::kExternalRegistry)
          .SetManifestVersion(2)
          .Build();
  scoped_refptr<const Extension> external_pref =
      ExtensionBuilder("external pref")
          .SetLocation(mojom::ManifestLocation::kExternalPref)
          .SetManifestVersion(2)
          .Build();
  scoped_refptr<const Extension> external_pref_download =
      ExtensionBuilder("external pref download")
          .SetLocation(mojom::ManifestLocation::kExternalPrefDownload)
          .SetManifestVersion(2)
          .Build();

  EXPECT_FALSE(experiment_manager()->IsExtensionAffected(*user_installed));
  EXPECT_FALSE(experiment_manager()->IsExtensionAffected(*external_registry));
  EXPECT_FALSE(experiment_manager()->IsExtensionAffected(*external_pref));
  EXPECT_FALSE(
      experiment_manager()->IsExtensionAffected(*external_pref_download));
}

}  // namespace extensions
