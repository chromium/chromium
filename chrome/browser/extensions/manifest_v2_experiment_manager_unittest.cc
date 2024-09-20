// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_management_internal.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_user_test_base.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/manifest.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

class ManifestV2ExperimentManagerUnitTestBase
    : public ExtensionServiceUserTestBase {
 public:
  ManifestV2ExperimentManagerUnitTestBase(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features);
  ~ManifestV2ExperimentManagerUnitTestBase() override = default;

  void SetUp() override {
    ExtensionServiceUserTestBase::SetUp();

    // Note: This is (subtly) different from
    // `InitializeEmptyExtensionService()`, which doesn't initialize a
    // testing PrefService.
    InitializeExtensionService(ExtensionServiceInitParams{});

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Log in the user on CrOS. This is necessary for the profile to be
    // considered one that can install extensions, which itself is
    // necessary for metrics testing.
    ASSERT_NO_FATAL_FAILURE(LoginChromeOSAshUser(
        GetFakeUserManager()->AddUser(account_id_), account_id_));
#endif

    experiment_manager_ = ManifestV2ExperimentManager::Get(profile());
  }

  void TearDown() override {
    experiment_manager_ = nullptr;
    ExtensionServiceUserTestBase::TearDown();
  }

  // Since this is testing the MV2 deprecation experiments, we don't want to
  // bypass their disabling for testing.
  bool ShouldAllowMV2Extensions() override { return false; }

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

// Test suite for cases where the user is in the "unsupported" experiment phase.
class ManifestV2ExperimentManagerUnsupportedUnitTest
    : public ManifestV2ExperimentManagerUnitTestBase {
 public:
  ManifestV2ExperimentManagerUnsupportedUnitTest()
      : ManifestV2ExperimentManagerUnitTestBase(
            {extensions_features::kExtensionManifestV2Unsupported},
            {}) {}
  ~ManifestV2ExperimentManagerUnsupportedUnitTest() override = default;
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
       MarkingNoticeAsAcknowledged) {
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

  EXPECT_FALSE(experiment_manager()->DidUserAcknowledgeNotice(ext1->id()));
  EXPECT_FALSE(experiment_manager()->DidUserAcknowledgeNotice(ext2->id()));

  experiment_manager()->MarkNoticeAsAcknowledged(ext1->id());

  EXPECT_TRUE(experiment_manager()->DidUserAcknowledgeNotice(ext1->id()));
  EXPECT_FALSE(experiment_manager()->DidUserAcknowledgeNotice(ext2->id()));
}

TEST_F(ManifestV2ExperimentManagerWarningUnitTest,
       MarkingGlobalNoticeAsAcknowledged) {
  EXPECT_FALSE(experiment_manager()->DidUserAcknowledgeNoticeGlobally());
  experiment_manager()->MarkNoticeAsAcknowledgedGlobally();
  EXPECT_TRUE(experiment_manager()->DidUserAcknowledgeNoticeGlobally());
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

TEST_F(ManifestV2ExperimentManagerDisableWithReEnableUnitTest,
       MarkingNoticeAsAcknowledged) {
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

  EXPECT_FALSE(experiment_manager()->DidUserAcknowledgeNotice(ext1->id()));
  EXPECT_FALSE(experiment_manager()->DidUserAcknowledgeNotice(ext2->id()));

  experiment_manager()->MarkNoticeAsAcknowledged(ext1->id());

  EXPECT_TRUE(experiment_manager()->DidUserAcknowledgeNotice(ext1->id()));
  EXPECT_FALSE(experiment_manager()->DidUserAcknowledgeNotice(ext2->id()));
}

// Tests that the proper manifest group is used when emitting metrics for
// disabled extensions.
TEST_F(ManifestV2ExperimentManagerDisableWithReEnableUnitTest,
       ProfileMetrics_ExtensionLocationsAreProperlyGrouped) {
  struct {
    mojom::ManifestLocation manifest_location;
    std::string name;
    std::string expected_histogram;
  } test_cases[] = {
      {mojom::ManifestLocation::kInternal, "Internal",
       "Extensions.MV2Deprecation.MV2ExtensionState.Internal"},
      // Note: component extensions aren't considered in the metrics, so
      // shouldn't have any emitted histograms.
      {mojom::ManifestLocation::kComponent, "Component", ""},
      {mojom::ManifestLocation::kExternalPolicy, "Policy",
       "Extensions.MV2Deprecation.MV2ExtensionState.Policy"},
      {mojom::ManifestLocation::kExternalPolicyDownload, "Policy Download",
       "Extensions.MV2Deprecation.MV2ExtensionState.Policy"},
      {mojom::ManifestLocation::kExternalPref, "Pref",
       "Extensions.MV2Deprecation.MV2ExtensionState.External"},
      {mojom::ManifestLocation::kExternalPrefDownload, "Pref Download",
       "Extensions.MV2Deprecation.MV2ExtensionState.External"},
      {mojom::ManifestLocation::kExternalRegistry, "Registry",
       "Extensions.MV2Deprecation.MV2ExtensionState.External"},
      {mojom::ManifestLocation::kUnpacked, "Unpacked",
       "Extensions.MV2Deprecation.MV2ExtensionState.Unpacked"},
      {mojom::ManifestLocation::kCommandLine, "Command Line",
       "Extensions.MV2Deprecation.MV2ExtensionState.Unpacked"},
  };

  const char* histograms[] = {
      "Extensions.MV2Deprecation.MV2ExtensionState.Internal",
      "Extensions.MV2Deprecation.MV2ExtensionState.Component",
      "Extensions.MV2Deprecation.MV2ExtensionState.Unpacked",
      "Extensions.MV2Deprecation.MV2ExtensionState.Policy",
      "Extensions.MV2Deprecation.MV2ExtensionState.External",
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    base::HistogramTester histogram_tester;

    // Install the extension, disable affected extensions (which should usually
    // include this extension), and record histograms.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(test_case.name)
            .SetManifestVersion(2)
            .SetLocation(test_case.manifest_location)
            .Build();
    service()->AddExtension(extension.get());

    experiment_manager()->DisableAffectedExtensionsForTesting();
    experiment_manager()->EmitMetricsForProfileReadyForTesting();

    // In each case, at most one histogram should have any records, and it
    // should have exactly one entry: the extension is soft-disabled.
    for (const char* histogram : histograms) {
      if (test_case.expected_histogram == histogram) {
        histogram_tester.ExpectBucketCount(
            histogram,
            ManifestV2ExperimentManager::MV2ExtensionState::kSoftDisabled, 1);
      } else {
        histogram_tester.ExpectTotalCount(histogram, 0);
      }
    }

    // Unload the extension so it doesn't interfere in later cases.
    service()->UnloadExtension(extension->id(),
                               UnloadedExtensionReason::UNINSTALL);
  }
}

// Tests that MV3 extensions don't emit any metrics.
TEST_F(ManifestV2ExperimentManagerDisableWithReEnableUnitTest,
       ProfileMetrics_MV3ExtensionsArentIncluded) {
  base::HistogramTester histogram_tester;

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetManifestVersion(3)
          .SetLocation(mojom::ManifestLocation::kInternal)
          .Build();
  service()->AddExtension(extension.get());

  experiment_manager()->DisableAffectedExtensionsForTesting();
  experiment_manager()->EmitMetricsForProfileReadyForTesting();

  histogram_tester.ExpectTotalCount(
      "Extensions.MV2Deprecation.MV2ExtensionState.Internal", 0);
}

// Tests that extensions that are re-enabled by the user are properly emitted
// as `kUserReEnabled`.
TEST_F(ManifestV2ExperimentManagerDisableWithReEnableUnitTest,
       ProfileMetrics_UserReEnabledAreProperlyEmitted) {
  base::HistogramTester histogram_tester;

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetManifestVersion(2)
          .SetLocation(mojom::ManifestLocation::kInternal)
          .Build();
  service()->AddExtension(extension.get());

  experiment_manager()->DisableAffectedExtensionsForTesting();
  service()->EnableExtension(extension->id());
  experiment_manager()->EmitMetricsForProfileReadyForTesting();

  histogram_tester.ExpectTotalCount(
      "Extensions.MV2Deprecation.MV2ExtensionState.Internal", 1);
  histogram_tester.ExpectBucketCount(
      "Extensions.MV2Deprecation.MV2ExtensionState.Internal",
      ManifestV2ExperimentManager::MV2ExtensionState::kUserReEnabled, 1);
}

// Tests that extensions that are disabled for other reasons (such as by user
// action) emit `kOther` for their state.
TEST_F(ManifestV2ExperimentManagerDisableWithReEnableUnitTest,
       ProfileMetrics_ExtensionsThatAreDisabledForOtherReasonsEmitOther) {
  base::HistogramTester histogram_tester;

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetManifestVersion(2)
          .SetLocation(mojom::ManifestLocation::kInternal)
          .Build();
  service()->AddExtension(extension.get());

  experiment_manager()->DisableAffectedExtensionsForTesting();
  service()->EnableExtension(extension->id());
  service()->DisableExtension(extension->id(),
                              disable_reason::DISABLE_USER_ACTION);
  experiment_manager()->EmitMetricsForProfileReadyForTesting();

  histogram_tester.ExpectTotalCount(
      "Extensions.MV2Deprecation.MV2ExtensionState.Internal", 1);
  histogram_tester.ExpectBucketCount(
      "Extensions.MV2Deprecation.MV2ExtensionState.Internal",
      ManifestV2ExperimentManager::MV2ExtensionState::kOther, 1);
}

enum class MV2PolicyLevel {
  kAllowed,
  kDisallowed,
  kAllowedForAdminInstalledOnly,
};

// A test suite to allow setting various MV2-related policies.
class ManifestV2ExperimentManagerDisableWithReEnableAndPolicyUnitTest
    : public ManifestV2ExperimentManagerDisableWithReEnableUnitTest {
 public:
  ManifestV2ExperimentManagerDisableWithReEnableAndPolicyUnitTest() = default;
  ~ManifestV2ExperimentManagerDisableWithReEnableAndPolicyUnitTest() override =
      default;

  // Sets the current level of the MV2 admin policy.
  void SetMV2PolicyLevel(MV2PolicyLevel policy_level) {
    internal::GlobalSettings::ManifestV2Setting pref_value;
    switch (policy_level) {
      case MV2PolicyLevel::kAllowed:
        pref_value = internal::GlobalSettings::ManifestV2Setting::kEnabled;
        break;
      case MV2PolicyLevel::kDisallowed:
        pref_value = internal::GlobalSettings::ManifestV2Setting::kDisabled;
        break;
      case MV2PolicyLevel::kAllowedForAdminInstalledOnly:
        pref_value = internal::GlobalSettings::ManifestV2Setting::
            kEnabledForForceInstalled;
        break;
    }

    sync_preferences::TestingPrefServiceSyncable* pref_service =
        testing_profile()->GetTestingPrefService();
    pref_service->SetManagedPref(pref_names::kManifestV2Availability,
                                 base::Value(static_cast<int>(pref_value)));
  }

  // Clears the MV2 policy.
  void ClearMV2Policy() {
    sync_preferences::TestingPrefServiceSyncable* pref_service =
        testing_profile()->GetTestingPrefService();
    pref_service->RemoveManagedPref(pref_names::kManifestV2Availability);
  }

  void AddPolicyInstalledMV2Extension(const ExtensionId& id,
                                      mojom::ManifestLocation location) {
    sync_preferences::TestingPrefServiceSyncable* pref_service =
        testing_profile()->GetTestingPrefService();
    const base::Value* existing_value =
        pref_service->GetManagedPref(pref_names::kExtensionManagement);
    base::Value::Dict new_value;
    if (existing_value) {
      new_value = existing_value->Clone().TakeDict();
    }

    new_value.Set(id, base::Value::Dict()
                          .Set("installation_mode", "force_installed")
                          .Set("update_url", "http://example.com/"));

    pref_service->SetManagedPref(pref_names::kExtensionManagement,
                                 std::move(new_value));
  }
};

// Tests that installation of all extensions is allowed if MV2 is allowed by
// policy.
TEST_F(ManifestV2ExperimentManagerDisableWithReEnableAndPolicyUnitTest,
       ShouldBlockInstallation_DontBlockWhenAllMV2Allowed) {
  SetMV2PolicyLevel(MV2PolicyLevel::kAllowed);

  struct {
    mojom::ManifestLocation manifest_location;
    const char* name;
  } test_cases[] = {
      {mojom::ManifestLocation::kInternal, "internal"},
      {mojom::ManifestLocation::kExternalPref, "pref"},
      {mojom::ManifestLocation::kExternalPrefDownload, "pref download"},
      {mojom::ManifestLocation::kExternalPolicy, "policy"},
      {mojom::ManifestLocation::kExternalPolicyDownload, "policy download"},
      {mojom::ManifestLocation::kUnpacked, "unpacked"},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    ExtensionId extension_id = crx_file::id_util::GenerateId(test_case.name);

    EXPECT_FALSE(experiment_manager()->ShouldBlockExtensionInstallation(
        extension_id, /*manifest_version=*/2, Manifest::TYPE_EXTENSION,
        test_case.manifest_location, HashedExtensionId(extension_id)));
  }
}

// Tests installation of all extensions (other than component extensions) is
// disallowed if disallowed by policy.
TEST_F(ManifestV2ExperimentManagerDisableWithReEnableAndPolicyUnitTest,
       ShouldBlockInstallation_AllAreBlockedWhenMV2Disallowed) {
  SetMV2PolicyLevel(MV2PolicyLevel::kDisallowed);

  struct {
    mojom::ManifestLocation manifest_location;
    const char* name;
  } test_cases[] = {
      {mojom::ManifestLocation::kInternal, "internal"},
      {mojom::ManifestLocation::kExternalPref, "pref"},
      {mojom::ManifestLocation::kExternalPrefDownload, "pref download"},
      {mojom::ManifestLocation::kExternalPolicy, "policy"},
      {mojom::ManifestLocation::kExternalPolicyDownload, "policy download"},
      {mojom::ManifestLocation::kUnpacked, "unpacked"},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    ExtensionId extension_id = crx_file::id_util::GenerateId(test_case.name);

    EXPECT_TRUE(experiment_manager()->ShouldBlockExtensionInstallation(
        extension_id, /*manifest_version=*/2, Manifest::TYPE_EXTENSION,
        test_case.manifest_location, HashedExtensionId(extension_id)));
  }
}

// Tests admin-installed extensions may be installed, while others may not be,
// if the MV2 policy is set to admin-installed-only.
TEST_F(
    ManifestV2ExperimentManagerDisableWithReEnableAndPolicyUnitTest,
    ShouldBlockInstallation_UserInstalledAreBlockedWhenForceInstalledAllowed) {
  SetMV2PolicyLevel(MV2PolicyLevel::kAllowedForAdminInstalledOnly);

  constexpr bool kInstallShouldBeBlocked = true;
  constexpr bool kInstallShouldBeAllowed = false;
  constexpr bool kForceInstalled = true;
  constexpr bool kUserInstalled = false;

  struct {
    mojom::ManifestLocation manifest_location;
    const char* name;
    bool force_installed;
    bool should_block_install;
  } test_cases[] = {
      {mojom::ManifestLocation::kInternal, "internal", kUserInstalled,
       kInstallShouldBeBlocked},
      {mojom::ManifestLocation::kExternalPref, "pref", kUserInstalled,
       kInstallShouldBeBlocked},
      {mojom::ManifestLocation::kUnpacked, "unpacked", kUserInstalled,
       kInstallShouldBeBlocked},

      {mojom::ManifestLocation::kExternalPolicy, "policy", kForceInstalled,
       kInstallShouldBeAllowed},
      {mojom::ManifestLocation::kExternalPolicyDownload, "policy download",
       kForceInstalled, kInstallShouldBeAllowed},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    ExtensionId extension_id = crx_file::id_util::GenerateId(test_case.name);
    if (test_case.force_installed) {
      AddPolicyInstalledMV2Extension(extension_id, test_case.manifest_location);
    }

    EXPECT_EQ(
        test_case.should_block_install,
        experiment_manager()->ShouldBlockExtensionInstallation(
            extension_id, /*manifest_version=*/2, Manifest::TYPE_EXTENSION,
            test_case.manifest_location, HashedExtensionId(extension_id)));
  }
}

TEST_F(ManifestV2ExperimentManagerDisableWithReEnableAndPolicyUnitTest,
       ExtensionsAreReEnabledOrDisabledOnPolicyChange) {
  // Install an MV2 extension and bootstrap the manager by disabling affected
  // extensions.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("test extension").SetManifestVersion(2).Build();
  service()->AddExtension(extension.get());
  const ExtensionId extension_id = extension->id();

  experiment_manager()->DisableAffectedExtensionsForTesting();

  // The extension should be disabled.
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());

  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));
  EXPECT_EQ(
      static_cast<int>(disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION),
      extension_prefs->GetDisableReasons(extension_id));

  // Set the MV2 policy to allow all MV2 extensions.
  SetMV2PolicyLevel(MV2PolicyLevel::kAllowed);

  // The extension should be enabled, since it's now allowed.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_id));
  EXPECT_EQ(0, extension_prefs->GetDisableReasons(extension_id));

  // Clear the MV2 policy. The extension should now be disabled again.
  ClearMV2Policy();
  EXPECT_TRUE(registry()->disabled_extensions().Contains(extension_id));
  EXPECT_EQ(
      static_cast<int>(disable_reason::DISABLE_UNSUPPORTED_MANIFEST_VERSION),
      extension_prefs->GetDisableReasons(extension_id));
}

// Tests that MV2 extensions that are allowed by policy emit `kUnaffected` for
// their state.
TEST_F(ManifestV2ExperimentManagerDisableWithReEnableAndPolicyUnitTest,
       ProfileMetrics_ExtensionsAllowedByPolicyEmitUnaffected) {
  SetMV2PolicyLevel(MV2PolicyLevel::kAllowed);

  base::HistogramTester histogram_tester;

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetManifestVersion(2)
          .SetLocation(mojom::ManifestLocation::kInternal)
          .Build();
  service()->AddExtension(extension.get());

  experiment_manager()->DisableAffectedExtensionsForTesting();
  experiment_manager()->EmitMetricsForProfileReadyForTesting();

  histogram_tester.ExpectTotalCount(
      "Extensions.MV2Deprecation.MV2ExtensionState.Internal", 1);
  histogram_tester.ExpectBucketCount(
      "Extensions.MV2Deprecation.MV2ExtensionState.Internal",
      ManifestV2ExperimentManager::MV2ExtensionState::kUnaffected, 1);
}

// Tests that the experiment phase is properly set for a user in the
// "unsupported" experiment phase.
TEST_F(ManifestV2ExperimentManagerUnsupportedUnitTest,
       ExperimentStageIsSetToUnsupported) {
  EXPECT_EQ(MV2ExperimentStage::kUnsupported,
            experiment_manager()->GetCurrentExperimentStage());
}

}  // namespace extensions
