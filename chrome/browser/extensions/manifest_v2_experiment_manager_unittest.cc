// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/manifest_v2_experiment_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service_user_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace extensions {

class ManifestV2ExperimentManagerUnitTest
    : public ExtensionServiceUserTestBase {
 public:
  ManifestV2ExperimentManagerUnitTest() = default;
  ~ManifestV2ExperimentManagerUnitTest() override = default;

  void SetUp() override {
    ExtensionServiceUserTestBase::SetUp();

    // Note: This is (subtly) different from
    // `InitializeEmptyExtensionService()`, which doesn't initialize a
    // testing PrefService.
    InitializeExtensionService(ExtensionServiceInitParams{});

#if BUILDFLAG(IS_CHROMEOS)
    // Log in the user on CrOS. This is necessary for the profile to be
    // considered one that can install extensions, which itself is
    // necessary for metrics testing.
    ASSERT_NO_FATAL_FAILURE(LoginChromeOSUser(
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
  raw_ptr<ManifestV2ExperimentManager> experiment_manager_;
};

// Sanity check that MV2 extensions are considered affected when the
// experiment is enabled in the "unsupported" phase. The "is affected" logic is
// much more heavily tested in mv2_deprecation_impact_checker_unittest.cc.
TEST_F(ManifestV2ExperimentManagerUnitTest, MV2ExtensionsAreAffected) {
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

    // Modern extensions are not affected by the experiment.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(test_case.name)
            .SetLocation(test_case.manifest_location)
            .Build();
    EXPECT_FALSE(experiment_manager()->IsExtensionAffected(*extension));
  }
}

// Tests the manager properly indicates when to block user-installed extensions
// in the "unsupported" phase.
TEST_F(ManifestV2ExperimentManagerUnitTest,
       ShouldBlockInstallation_UserInstalledExtensions) {
  constexpr bool kInstallShouldBeBlocked = true;
  constexpr bool kInstallShouldBeAllowed = false;
  struct {
    mojom::ManifestLocation manifest_location;
    int manifest_version;
    const char* name;
    bool should_block_install;
  } test_cases[] = {
      // User-installed extensions are only allowed if they are MV3 or higher.
      {mojom::ManifestLocation::kUnpacked, 2, "unpacked - mv2",
       kInstallShouldBeBlocked},
      {mojom::ManifestLocation::kUnpacked, 3, "unpacked - mv3",
       kInstallShouldBeAllowed},
      {mojom::ManifestLocation::kCommandLine, 2, "command line - mv2",
       kInstallShouldBeBlocked},
      {mojom::ManifestLocation::kCommandLine, 3, "command line - mv3",
       kInstallShouldBeAllowed},
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

    scoped_refptr<const Extension> extension =
        ExtensionBuilder(test_case.name)
            .SetManifestVersion(test_case.manifest_version)
            .SetLocation(test_case.manifest_location)
            .Build();

    EXPECT_EQ(test_case.should_block_install,
              experiment_manager()->ShouldBlockExtensionInstallation(
                  extension->manifest_version(), extension->GetType(),
                  extension->location()));

    EXPECT_EQ(test_case.should_block_install,
              experiment_manager()->ShouldBlockExtensionEnable(*extension));
  }
}

// Tests the manager never blocks component extensions from installing.
TEST_F(ManifestV2ExperimentManagerUnitTest,
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

    scoped_refptr<const Extension> extension =
        ExtensionBuilder(test_case.name)
            .SetManifestVersion(test_case.manifest_version)
            .SetLocation(test_case.manifest_location)
            .Build();

    // Component extensions are built-in parts of Chrome that are extensions as
    // an implementation detail. They should always be allowed to install and
    // remain enabled.
    EXPECT_FALSE(experiment_manager()->ShouldBlockExtensionInstallation(
        extension->manifest_version(), extension->GetType(),
        extension->location()));
    EXPECT_FALSE(experiment_manager()->ShouldBlockExtensionEnable(*extension));
  }
}

// Tests that the proper manifest group is used when emitting metrics for
// disabled extensions.
TEST_F(ManifestV2ExperimentManagerUnitTest,
       ProfileMetrics_ExtensionLocationsAreProperlyGrouped) {
  struct {
    mojom::ManifestLocation manifest_location;
    std::string name;
    std::string expected_histogram;
    ManifestV2ExperimentManager::MV2ExtensionState expected_state =
        ManifestV2ExperimentManager::MV2ExtensionState::kHardDisabled;
  } test_cases[] = {
      {mojom::ManifestLocation::kInternal, "Internal",
       "Extensions.MV2Deprecation.MV2ExtensionState.Internal"},
      {mojom::ManifestLocation::kComponent, "Component",
       "Extensions.MV2Deprecation.MV2ExtensionState.Component",
       ManifestV2ExperimentManager::MV2ExtensionState::kUnaffected},
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
    registrar()->AddExtension(extension.get());

    experiment_manager()->DisableAffectedExtensionsForTesting();
    experiment_manager()->EmitMetricsForProfileReadyForTesting();

    // In each case, at most one histogram should have any records, and it
    // should have exactly one entry: the extension is hard-disabled.
    for (const char* histogram : histograms) {
      if (test_case.expected_histogram == histogram) {
        histogram_tester.ExpectBucketCount(histogram, test_case.expected_state,
                                           1);
      } else {
        histogram_tester.ExpectTotalCount(histogram, 0);
      }
    }

    // Unload the extension so it doesn't interfere in later cases.
    registrar()->RemoveExtension(extension->id(),
                                 UnloadedExtensionReason::UNINSTALL);
  }
}

// Tests that modern extensions don't emit any metrics.
TEST_F(ManifestV2ExperimentManagerUnitTest,
       ProfileMetrics_ModernExtensionsArentIncluded) {
  base::HistogramTester histogram_tester;

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .Build();
  registrar()->AddExtension(extension.get());

  experiment_manager()->DisableAffectedExtensionsForTesting();
  experiment_manager()->EmitMetricsForProfileReadyForTesting();

  histogram_tester.ExpectTotalCount(
      "Extensions.MV2Deprecation.MV2ExtensionState.Internal", 0);
}

// Tests that extensions that are disabled for other reasons (such as by user
// action) emit `kOther` for their state.
TEST_F(ManifestV2ExperimentManagerUnitTest,
       ProfileMetrics_ExtensionsThatAreDisabledForOtherReasonsEmitOther) {
  base::HistogramTester histogram_tester;

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetManifestVersion(2)
          .SetLocation(mojom::ManifestLocation::kInternal)
          .Build();
  registrar()->AddExtension(extension.get());

  registrar()->DisableExtension(extension->id(),
                                {disable_reason::DISABLE_USER_ACTION});
  experiment_manager()->EmitMetricsForProfileReadyForTesting();

  histogram_tester.ExpectTotalCount(
      "Extensions.MV2Deprecation.MV2ExtensionState.Internal", 1);
  histogram_tester.ExpectBucketCount(
      "Extensions.MV2Deprecation.MV2ExtensionState.Internal",
      ManifestV2ExperimentManager::MV2ExtensionState::kOther, 1);
}

// Tests that MV2 extensions cannot be re-enabled in the "unsupported"
// experiment phase.
TEST_F(ManifestV2ExperimentManagerUnitTest, MV2ExtensionsCannotBeEnabled) {
  constexpr bool kEnableShouldBeBlocked = true;
  constexpr bool kEnableShouldBeAllowed = false;
  struct {
    mojom::ManifestLocation manifest_location;
    int manifest_version;
    const char* name;
    bool should_block_enable;
  } test_cases[] = {
      // The vast majority of extensions should be not be enable-able if they
      // are MV2.
      {mojom::ManifestLocation::kUnpacked, 2, "unpacked - mv2",
       kEnableShouldBeBlocked},
      {mojom::ManifestLocation::kUnpacked, 3, "unpacked - mv3",
       kEnableShouldBeAllowed},
      {mojom::ManifestLocation::kCommandLine, 2, "command line - mv2",
       kEnableShouldBeBlocked},
      {mojom::ManifestLocation::kCommandLine, 3, "command line - mv3",
       kEnableShouldBeAllowed},
      {mojom::ManifestLocation::kInternal, 2, "internal - mv2",
       kEnableShouldBeBlocked},
      {mojom::ManifestLocation::kInternal, 3, "internal - mv3",
       kEnableShouldBeAllowed},
      {mojom::ManifestLocation::kExternalPref, 2, "external pref - mv2",
       kEnableShouldBeBlocked},
      {mojom::ManifestLocation::kExternalPref, 3, "external pref - mv3",
       kEnableShouldBeAllowed},
      {mojom::ManifestLocation::kExternalPref, 2, "external registry - mv2",
       kEnableShouldBeBlocked},
      {mojom::ManifestLocation::kExternalRegistry, 3, "external registry - mv3",
       kEnableShouldBeAllowed},
      {mojom::ManifestLocation::kExternalPrefDownload, 2,
       "external download - mv2", kEnableShouldBeBlocked},
      {mojom::ManifestLocation::kExternalPrefDownload, 3,
       "external download - mv3", kEnableShouldBeAllowed},
      {mojom::ManifestLocation::kExternalPolicy, 2, "external policy - mv2",
       kEnableShouldBeBlocked},
      {mojom::ManifestLocation::kExternalPolicy, 3, "external policy - mv3",
       kEnableShouldBeAllowed},

      {mojom::ManifestLocation::kComponent, 2, "component - mv2",
       kEnableShouldBeAllowed},
      {mojom::ManifestLocation::kComponent, 3, "component - mv3",
       kEnableShouldBeAllowed},
      {mojom::ManifestLocation::kExternalComponent, 2, "component - mv2",
       kEnableShouldBeAllowed},
      {mojom::ManifestLocation::kExternalComponent, 3, "component - mv3",
       kEnableShouldBeAllowed},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);

    scoped_refptr<const Extension> extension =
        ExtensionBuilder(test_case.name)
            .SetManifestVersion(test_case.manifest_version)
            .SetLocation(test_case.manifest_location)
            .Build();

    EXPECT_EQ(test_case.should_block_enable,
              experiment_manager()->ShouldBlockExtensionEnable(*extension));
  }
}

}  // namespace extensions
