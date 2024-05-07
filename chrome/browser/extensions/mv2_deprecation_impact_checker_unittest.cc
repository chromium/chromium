// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
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
#include "extensions/common/mojom/manifest.mojom.h"

namespace extensions {

namespace {

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

// Describes the current level; used in describing parameterized tests.
const char* DescribeMV2PolicyLevel(MV2PolicyLevel level) {
  switch (level) {
    case MV2PolicyLevel::kUnset:
      return "Unset";
    case MV2PolicyLevel::kAllowed:
      return "MV2Allowed";
    case MV2PolicyLevel::kDisallowed:
      return "MV2Disallowed";
    case MV2PolicyLevel::kAllowedForAdminInstalledOnly:
      return "MV2ForAdminInstalledOnly";
  }
}

}  // namespace

class MV2DeprecationImpactCheckerUnitTestBase
    : public ExtensionServiceTestBase {
 public:
  MV2DeprecationImpactCheckerUnitTestBase(MV2ExperimentStage experiment_stage,
                                          MV2PolicyLevel mv2_policy_level);
  ~MV2DeprecationImpactCheckerUnitTestBase() override = default;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    // Note: This is (subtly) different from
    // `InitializeEmptyExtensionService()`, which doesn't initialize a
    // testing PrefService.
    ExtensionServiceInitParams params;
    InitializeExtensionService(params);

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

  MV2DeprecationImpactChecker* impact_checker() {
    return impact_checker_.get();
  }

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

MV2DeprecationImpactCheckerUnitTestBase::
    MV2DeprecationImpactCheckerUnitTestBase(MV2ExperimentStage experiment_stage,
                                            MV2PolicyLevel mv2_policy_level)
    : experiment_stage_(experiment_stage),
      mv2_policy_level_(mv2_policy_level) {}

class MV2DeprecationImpactCheckerUnitTest
    : public MV2DeprecationImpactCheckerUnitTestBase,
      public testing::WithParamInterface<MV2PolicyLevel> {
 public:
  MV2DeprecationImpactCheckerUnitTest()
      : MV2DeprecationImpactCheckerUnitTestBase(MV2ExperimentStage::kWarning,
                                                GetParam()) {}
  ~MV2DeprecationImpactCheckerUnitTest() override = default;
};

class MV2DeprecationImpactCheckerDisabledUnitTest
    : public MV2DeprecationImpactCheckerUnitTestBase,
      public testing::WithParamInterface<MV2PolicyLevel> {
 public:
  MV2DeprecationImpactCheckerDisabledUnitTest()
      : MV2DeprecationImpactCheckerUnitTestBase(MV2ExperimentStage::kNone,
                                                GetParam()) {}
  ~MV2DeprecationImpactCheckerDisabledUnitTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    MV2DeprecationImpactCheckerUnitTest,
    testing::Values(MV2PolicyLevel::kUnset,
                    MV2PolicyLevel::kAllowed,
                    MV2PolicyLevel::kDisallowed,
                    MV2PolicyLevel::kAllowedForAdminInstalledOnly),
    [](const testing::TestParamInfo<MV2PolicyLevel>& info) {
      return DescribeMV2PolicyLevel(info.param);
    });
INSTANTIATE_TEST_SUITE_P(
    ,
    MV2DeprecationImpactCheckerDisabledUnitTest,
    testing::Values(MV2PolicyLevel::kUnset,
                    MV2PolicyLevel::kAllowed,
                    MV2PolicyLevel::kDisallowed,
                    MV2PolicyLevel::kAllowedForAdminInstalledOnly),
    [](const testing::TestParamInfo<MV2PolicyLevel>& info) {
      return DescribeMV2PolicyLevel(info.param);
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

  // These user-facing MV2 extensions would be affected if the policy is
  // anything other than set to "Allowed" (which allows all MV2 extensions).
  bool expected_affected = GetParam() != MV2PolicyLevel::kAllowed;
  EXPECT_EQ(expected_affected,
            impact_checker()->IsExtensionAffected(*user_installed));
  EXPECT_EQ(expected_affected,
            impact_checker()->IsExtensionAffected(*external_registry));
  EXPECT_EQ(expected_affected,
            impact_checker()->IsExtensionAffected(*external_pref));
  EXPECT_EQ(expected_affected,
            impact_checker()->IsExtensionAffected(*external_pref_download));
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
      GetParam() == MV2PolicyLevel::kAllowed ||
      GetParam() == MV2PolicyLevel::kAllowedForAdminInstalledOnly;

  bool policy_installs_affected = !policy_installed_mv2_extensions_allowed;
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

  bool all_mv2_extensions_allowed = GetParam() == MV2PolicyLevel::kAllowed;
  bool allowed_installs_affected = !all_mv2_extensions_allowed;
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

// Tests that we won't warn about any MV2 extensions when the experiment is
// disabled.
TEST_P(MV2DeprecationImpactCheckerDisabledUnitTest,
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

  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*user_installed));
  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*external_registry));
  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*external_pref));
  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*external_pref_download));
}

}  // namespace extensions
