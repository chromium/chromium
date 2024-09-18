// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_management_internal.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/manifest.mojom.h"

namespace extensions {

namespace {

// Hashed ID for 'aaaa...a'.
constexpr char kTestHashedIdA[] = "68F84A59A3CA2D0E5CB1646FBB164DA409B5D8F2";
// Hashed ID for 'bbbb...b'.
constexpr char kTestHashedIdB[] = "142E27CA6D179970507F4076E2AC96FEC5834F82";

// The setting of the MV2 policy to use.
enum class MV2PolicyLevel {
  // Unset. Default browser behavior.
  kUnset,
  // All MV2 extensions are allowed.
  kAllowed,
  // All MV2 extensions are disallowed.
  kDisallowed,
  // Only MV2 extensions installed by admin policy are allowed.
  kAllowedForAdminInstalledOnly,
};

// A test variant that allows parameterization of both the experiment stage and
// the policy level.
using TestVariant = std::tuple<MV2ExperimentStage, MV2PolicyLevel>;

// Describes the current test variants; used in describing parameterized tests.
std::string DescribeTestVariant(const TestVariant& test_variant) {
  const MV2ExperimentStage experiment_stage = std::get<0>(test_variant);
  const MV2PolicyLevel policy_level = std::get<1>(test_variant);

  std::string description;

  switch (policy_level) {
    case MV2PolicyLevel::kUnset:
      description += "PolicyUnset";
      break;
    case MV2PolicyLevel::kAllowed:
      description += "MV2AllowedByPolicy";
      break;
    case MV2PolicyLevel::kDisallowed:
      description += "MV2DisallowedByPolicy";
      break;
    case MV2PolicyLevel::kAllowedForAdminInstalledOnly:
      description += "MV2ForAdminInstalledOnly";
      break;
  }

  description += "And";

  switch (experiment_stage) {
    case MV2ExperimentStage::kNone:
      description += "ExperimentIsDisabled";
      break;
    case MV2ExperimentStage::kWarning:
      description += "WarningExperiment";
      break;
    case MV2ExperimentStage::kDisableWithReEnable:
      description += "DisableExperiment";
      break;
    case MV2ExperimentStage::kUnsupported:
      description += "UnsupportedExperiment";
      break;
  }

  return description;
}

}  // namespace

class MV2DeprecationImpactCheckerUnitTest
    : public ExtensionServiceTestBase,
      public testing::WithParamInterface<TestVariant> {
 public:
  MV2DeprecationImpactCheckerUnitTest();
  ~MV2DeprecationImpactCheckerUnitTest() override = default;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    // Note: This is (subtly) different from
    // `InitializeEmptyExtensionService()`, which doesn't initialize a
    // testing PrefService.
    InitializeExtensionService(ExtensionServiceInitParams{});

    // Sets the current level of the MV2 admin policy.
    sync_preferences::TestingPrefServiceSyncable* pref_service =
        testing_profile()->GetTestingPrefService();
    std::optional<internal::GlobalSettings::ManifestV2Setting> pref_value;
    switch (mv2_policy_level_) {
      case MV2PolicyLevel::kUnset:
        break;  // Don't set the policy.
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

    if (pref_value) {
      pref_service->SetManagedPref(pref_names::kManifestV2Availability,
                                   base::Value(static_cast<int>(*pref_value)));
    }

    impact_checker_ = std::make_unique<MV2DeprecationImpactChecker>(
        experiment_stage_,
        ExtensionManagementFactory::GetForBrowserContext(profile()));
  }

  void TearDown() override {
    impact_checker_ = nullptr;
    ExtensionServiceTestBase::TearDown();
  }

  // Since this is testing the MV2 deprecation experiments, we don't want to
  // bypass their disabling for testing.
  bool ShouldAllowMV2Extensions() override { return false; }

  // Adds a new force-installed extension with the given `name`,
  // `manifest_location`, and `manifest_version`.
  scoped_refptr<const Extension> AddForceInstalledExtension(
      const std::string& name,
      mojom::ManifestLocation location,
      int manifest_version) {
    return AddPolicyInstalledExtension(name, location, manifest_version,
                                       "force_installed");
  }

  // Adds a new "recommended" extension with the given `name`,
  // `manifest_location`, and `manifest_version`. "Recommended" extensions
  // are installed and always kept installed, but may be disabled by the user.
  scoped_refptr<const Extension> AddRecommendedPolicyExtension(
      const std::string& name,
      mojom::ManifestLocation location,
      int manifest_version) {
    return AddPolicyInstalledExtension(name, location, manifest_version,
                                       "normal_installed");
  }

  // Adds a new extension that's explicitly allowed by admin policy, but is
  // not force- or default-installed.
  scoped_refptr<const Extension> AddAllowedPolicyExtension(
      const std::string& name,
      mojom::ManifestLocation location,
      int manifest_version) {
    return AddPolicyInstalledExtension(name, location, manifest_version,
                                       "allowed");
  }

  // Returns true if the MV2 deprecation experiment is active in any stage.
  bool ExperimentIsActive() const {
    return experiment_stage_ != MV2ExperimentStage::kNone;
  }

  MV2DeprecationImpactChecker* impact_checker() {
    return impact_checker_.get();
  }
  MV2PolicyLevel policy_level() const { return mv2_policy_level_; }

 private:
  // Helper function to add a policy extension entry.
  scoped_refptr<const Extension> AddPolicyInstalledExtension(
      const std::string& name,
      mojom::ManifestLocation location,
      int manifest_version,
      const std::string& installation_mode) {
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(name)
            .SetLocation(location)
            .SetManifestVersion(manifest_version)
            .Build();

    sync_preferences::TestingPrefServiceSyncable* pref_service =
        testing_profile()->GetTestingPrefService();
    const base::Value* existing_value =
        pref_service->GetManagedPref(pref_names::kExtensionManagement);
    base::Value::Dict new_value;
    if (existing_value) {
      new_value = existing_value->Clone().TakeDict();
    }

    new_value.Set(extension->id(),
                  base::Value::Dict()
                      .Set("installation_mode", installation_mode)
                      .Set("update_url", "http://example.com/"));

    pref_service->SetManagedPref(pref_names::kExtensionManagement,
                                 std::move(new_value));

    return extension;
  }

  const MV2ExperimentStage experiment_stage_;
  const MV2PolicyLevel mv2_policy_level_;
  std::unique_ptr<MV2DeprecationImpactChecker> impact_checker_;
};

MV2DeprecationImpactCheckerUnitTest::MV2DeprecationImpactCheckerUnitTest()
    : experiment_stage_(std::get<0>(GetParam())),
      mv2_policy_level_(std::get<1>(GetParam())) {}

class MV2DeprecationImpactCheckerUnitTestWithAllowlist
    : public MV2DeprecationImpactCheckerUnitTest {
 public:
  MV2DeprecationImpactCheckerUnitTestWithAllowlist() {
    std::string params =
        base::StringPrintf("%s,%s", kTestHashedIdA, kTestHashedIdB);
    feature_list_.InitAndEnableFeatureWithParameters(
        extensions_features::kExtensionManifestV2ExceptionList,
        {{extensions_features::kExtensionManifestV2ExceptionListParam.name,
          params}});
  }
  ~MV2DeprecationImpactCheckerUnitTestWithAllowlist() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    MV2DeprecationImpactCheckerUnitTest,
    testing::Combine(
        testing::Values(MV2ExperimentStage::kNone,
                        MV2ExperimentStage::kWarning,
                        MV2ExperimentStage::kDisableWithReEnable,
                        MV2ExperimentStage::kUnsupported),
        testing::Values(MV2PolicyLevel::kUnset,
                        MV2PolicyLevel::kAllowed,
                        MV2PolicyLevel::kDisallowed,
                        MV2PolicyLevel::kAllowedForAdminInstalledOnly)),
    [](const testing::TestParamInfo<TestVariant>& info) {
      return DescribeTestVariant(info.param);
    });

INSTANTIATE_TEST_SUITE_P(
    ,
    MV2DeprecationImpactCheckerUnitTestWithAllowlist,
    testing::Combine(
        testing::Values(MV2ExperimentStage::kNone,
                        MV2ExperimentStage::kWarning,
                        MV2ExperimentStage::kDisableWithReEnable,
                        MV2ExperimentStage::kUnsupported),
        testing::Values(MV2PolicyLevel::kUnset,
                        MV2PolicyLevel::kAllowed,
                        MV2PolicyLevel::kDisallowed,
                        MV2PolicyLevel::kAllowedForAdminInstalledOnly)),
    [](const testing::TestParamInfo<TestVariant>& info) {
      return DescribeTestVariant(info.param);
    });

// Tests that user-visible MV2 extensions are properly considered affected by
// the MV2 deprecation experiment.
TEST_P(MV2DeprecationImpactCheckerUnitTest,
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

  // Unpacked (and commandline) extensions *are* affected by the MV2
  // deprecation.  They will be treated differently depending on the experiment
  // stage, but should be included in e.g. the warning.
  scoped_refptr<const Extension> unpacked =
      ExtensionBuilder("unpacked")
          .SetLocation(mojom::ManifestLocation::kUnpacked)
          .SetManifestVersion(2)
          .Build();
  scoped_refptr<const Extension> commandline =
      ExtensionBuilder("commandline")
          .SetLocation(mojom::ManifestLocation::kCommandLine)
          .SetManifestVersion(2)
          .Build();

  // These user-facing MV2 extensions would be affected if the experiment is
  // active and the policy is anything other than set to "Allowed" (which
  // allows all MV2 extensions).
  bool expected_affected =
      ExperimentIsActive() && policy_level() != MV2PolicyLevel::kAllowed;
  EXPECT_EQ(expected_affected,
            impact_checker()->IsExtensionAffected(*user_installed));
  EXPECT_EQ(expected_affected,
            impact_checker()->IsExtensionAffected(*external_registry));
  EXPECT_EQ(expected_affected,
            impact_checker()->IsExtensionAffected(*external_pref));
  EXPECT_EQ(expected_affected,
            impact_checker()->IsExtensionAffected(*external_pref_download));
  EXPECT_EQ(expected_affected,
            impact_checker()->IsExtensionAffected(*unpacked));
  EXPECT_EQ(expected_affected,
            impact_checker()->IsExtensionAffected(*commandline));
}

// Checks that certain special cases of extensions, such as default-installed
// and installed by OEM, are also affected by the MV2 deprecation experiments.
TEST_P(MV2DeprecationImpactCheckerUnitTest,
       DefaultInstalledMV2ExtensionsAreAffected) {
  scoped_refptr<const Extension> default_installed =
      ExtensionBuilder("default installed")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .SetManifestVersion(2)
          .AddFlags(Extension::WAS_INSTALLED_BY_DEFAULT)
          .Build();
  scoped_refptr<const Extension> oem_installed =
      ExtensionBuilder("oem installed")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .SetManifestVersion(2)
          .AddFlags(Extension::WAS_INSTALLED_BY_DEFAULT &
                    Extension::WAS_INSTALLED_BY_OEM)
          .Build();

  // These extensions should be affected if the experiment is enabled and the
  // policy isn't set to allow all MV2 extensions.
  bool expected_affected =
      ExperimentIsActive() && policy_level() != MV2PolicyLevel::kAllowed;
  EXPECT_EQ(expected_affected,
            impact_checker()->IsExtensionAffected(*default_installed));
  EXPECT_EQ(expected_affected,
            impact_checker()->IsExtensionAffected(*oem_installed));
}

// Tests that component extensions are not included in the MV2 deprecation
// experiment (they're implementation details of the browser).
TEST_P(MV2DeprecationImpactCheckerUnitTest, ComponentExtensionsAreNotAffected) {
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

  // Component extensions are never affected by the experiment.
  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*component));
  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*external_component));
}

// Tests that MV3 extensions, of any location, are not affected by the MV2
// deprecation experiment.
TEST_P(MV2DeprecationImpactCheckerUnitTest, NoMV3ExtensionsAreAffected) {
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
    EXPECT_FALSE(impact_checker()->IsExtensionAffected(*extension));
  }
}

// Tests that MV2 policy-installed extensions are affected if and only if the
// policy does not exempt them.
TEST_P(MV2DeprecationImpactCheckerUnitTest,
       MV2PolicyInstalledExtensionsMayBeAffected) {
  scoped_refptr<const Extension> forced_policy = AddForceInstalledExtension(
      "forced policy", mojom::ManifestLocation::kExternalPolicy, 2);
  scoped_refptr<const Extension> forced_policy_download =
      AddForceInstalledExtension(
          "forced policy download",
          mojom::ManifestLocation::kExternalPolicyDownload, 2);
  scoped_refptr<const Extension> recommended_policy =
      AddRecommendedPolicyExtension(
          "recommended policy", mojom::ManifestLocation::kExternalPolicy, 2);
  scoped_refptr<const Extension> recommended_policy_download =
      AddRecommendedPolicyExtension(
          "recommended policy download",
          mojom::ManifestLocation::kExternalPolicyDownload, 2);

  bool policy_installed_mv2_extensions_allowed =
      policy_level() == MV2PolicyLevel::kAllowed ||
      policy_level() == MV2PolicyLevel::kAllowedForAdminInstalledOnly;

  // Policy installs are affected if they are not exempt by policy and if the
  // experiment is active.
  bool policy_installs_affected =
      ExperimentIsActive() && !policy_installed_mv2_extensions_allowed;

  EXPECT_EQ(policy_installs_affected,
            impact_checker()->IsExtensionAffected(*forced_policy));
  EXPECT_EQ(policy_installs_affected,
            impact_checker()->IsExtensionAffected(*forced_policy_download));
  EXPECT_EQ(policy_installs_affected,
            impact_checker()->IsExtensionAffected(*recommended_policy));
  EXPECT_EQ(policy_installs_affected, impact_checker()->IsExtensionAffected(
                                          *recommended_policy_download));
}

// Tests that MV2 extensions that are allowed by policy, but not policy-
// installed, are treated as other MV2 extensions.
TEST_P(MV2DeprecationImpactCheckerUnitTest,
       MV2PolicyAllowedExtensionsMayBeAffected) {
  scoped_refptr<const Extension> allowed_policy = AddAllowedPolicyExtension(
      "allowed policy", mojom::ManifestLocation::kExternalPolicy, 2);
  scoped_refptr<const Extension> allowed_policy_download =
      AddAllowedPolicyExtension(
          "allowed policy download",
          mojom::ManifestLocation::kExternalPolicyDownload, 2);

  // "allowed" policy installs (as opposed to recommend or force-installed
  // installs) are affected if the experiment is on and the policy does not
  // allow *all* MV2 extensions.
  bool all_mv2_extensions_allowed = policy_level() == MV2PolicyLevel::kAllowed;
  bool allowed_installs_affected =
      ExperimentIsActive() && !all_mv2_extensions_allowed;
  EXPECT_EQ(allowed_installs_affected,
            impact_checker()->IsExtensionAffected(*allowed_policy));
  EXPECT_EQ(allowed_installs_affected,
            impact_checker()->IsExtensionAffected(*allowed_policy_download));
}

// Tests that any MV3 extension installed by policy is never affected by
// MV2 experiments.
TEST_P(MV2DeprecationImpactCheckerUnitTest,
       MV3PolicyInstalledExtensionsNeverAffected) {
  scoped_refptr<const Extension> forced_policy = AddForceInstalledExtension(
      "forced policy", mojom::ManifestLocation::kExternalPolicy, 3);
  scoped_refptr<const Extension> forced_policy_download =
      AddForceInstalledExtension(
          "forced policy download",
          mojom::ManifestLocation::kExternalPolicyDownload, 3);
  scoped_refptr<const Extension> recommended_policy =
      AddRecommendedPolicyExtension(
          "recommended policy", mojom::ManifestLocation::kExternalPolicy, 3);
  scoped_refptr<const Extension> recommended_policy_download =
      AddRecommendedPolicyExtension(
          "recommended policy download",
          mojom::ManifestLocation::kExternalPolicyDownload, 3);
  scoped_refptr<const Extension> allowed_policy = AddAllowedPolicyExtension(
      "allowed policy", mojom::ManifestLocation::kExternalPolicy, 3);
  scoped_refptr<const Extension> allowed_policy_download =
      AddAllowedPolicyExtension(
          "allowed policy download",
          mojom::ManifestLocation::kExternalPolicyDownload, 3);

  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*forced_policy));
  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*forced_policy_download));
  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*recommended_policy));
  EXPECT_FALSE(
      impact_checker()->IsExtensionAffected(*recommended_policy_download));
  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*allowed_policy));
  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*allowed_policy_download));
}

// Tests that non-extension "extension-like" things (such as platform apps and
// hosted apps) are not affected by the MV2 deprecation experiment.
TEST_P(MV2DeprecationImpactCheckerUnitTest, NonExtensionsAreNotAffected) {
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

  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*platform_app));
  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*hosted_app));
}

// Tests the allowlist is taken into account.
TEST_P(MV2DeprecationImpactCheckerUnitTestWithAllowlist, AllowlistWorks) {
  scoped_refptr<const Extension> ext_a =
      ExtensionBuilder("ext-a")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .SetManifestVersion(2)
          .SetID(std::string(32, 'a'))
          .Build();
  scoped_refptr<const Extension> ext_b =
      ExtensionBuilder("ext-b")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .SetManifestVersion(2)
          .SetID(std::string(32, 'b'))
          .Build();
  scoped_refptr<const Extension> ext_c =
      ExtensionBuilder("ext-c")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .SetManifestVersion(2)
          .SetID(std::string(32, 'c'))
          .Build();

  // User-facing MV2 extensions would be affected if the experiment is active
  // and the policy is anything other than set to "Allowed" (which allows all
  // MV2 extensions).
  bool expected_affected =
      ExperimentIsActive() && policy_level() != MV2PolicyLevel::kAllowed;

  // `ext_a` and `ext_b` are in the allowlist, so aren't affected.
  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*ext_a));
  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*ext_b));
  // `ext_c` is not in the allowlist, so is affected if the experiment is active
  // and the policy isn't set.
  EXPECT_EQ(expected_affected, impact_checker()->IsExtensionAffected(*ext_c));
}

}  // namespace extensions
