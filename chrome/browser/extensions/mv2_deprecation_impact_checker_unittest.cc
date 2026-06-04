// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_management_internal.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/manifest_v2_experiment_manager.h"
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

}  // namespace

class MV2DeprecationImpactCheckerUnitTest : public ExtensionServiceTestBase {
 public:
  MV2DeprecationImpactCheckerUnitTest() = default;
  ~MV2DeprecationImpactCheckerUnitTest() override = default;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeExtensionService(ExtensionServiceInitParams{});
    impact_checker_ = std::make_unique<MV2DeprecationImpactChecker>(profile());
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
        testing_pref_service();
    const base::Value* existing_value =
        pref_service->GetManagedPref(pref_names::kExtensionManagement);
    base::DictValue new_value;
    if (existing_value) {
      new_value = existing_value->Clone().TakeDict();
    }

    new_value.Set(extension->id(),
                  base::DictValue()
                      .Set("installation_mode", installation_mode)
                      .Set("update_url", "http://example.com/"));

    pref_service->SetManagedPref(pref_names::kExtensionManagement,
                                 std::move(new_value));

    return extension;
  }

  std::unique_ptr<MV2DeprecationImpactChecker> impact_checker_;
};

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

// Tests that user-visible MV2 extensions are properly considered affected by
// the MV2 deprecation experiment.
TEST_F(MV2DeprecationImpactCheckerUnitTest,
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

  EXPECT_TRUE(impact_checker()->IsExtensionAffected(*user_installed));
  EXPECT_TRUE(impact_checker()->IsExtensionAffected(*external_registry));
  EXPECT_TRUE(impact_checker()->IsExtensionAffected(*external_pref));
  EXPECT_TRUE(impact_checker()->IsExtensionAffected(*external_pref_download));
  EXPECT_TRUE(impact_checker()->IsExtensionAffected(*unpacked));
  EXPECT_TRUE(impact_checker()->IsExtensionAffected(*commandline));
}

// Checks that certain special cases of extensions, such as default-installed
// and installed by OEM, are also affected by the MV2 deprecation experiments.
TEST_F(MV2DeprecationImpactCheckerUnitTest,
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

  EXPECT_TRUE(impact_checker()->IsExtensionAffected(*default_installed));
  EXPECT_TRUE(impact_checker()->IsExtensionAffected(*oem_installed));
}

// Tests that component extensions are not included in the MV2 deprecation
// experiment (they're implementation details of the browser).
TEST_F(MV2DeprecationImpactCheckerUnitTest, ComponentExtensionsAreNotAffected) {
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
TEST_F(MV2DeprecationImpactCheckerUnitTest, NoMV3ExtensionsAreAffected) {
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
TEST_F(MV2DeprecationImpactCheckerUnitTest,
       MV2PolicyInstalledExtensionsAreAffected) {
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

  // Policy installs are affected; we no longer support the exemption policy.
  EXPECT_TRUE(impact_checker()->IsExtensionAffected(*forced_policy));
  EXPECT_TRUE(impact_checker()->IsExtensionAffected(*forced_policy_download));
  EXPECT_TRUE(impact_checker()->IsExtensionAffected(*recommended_policy));
  EXPECT_TRUE(
      impact_checker()->IsExtensionAffected(*recommended_policy_download));
}

// Tests that MV2 extensions that are allowed by policy, but not policy-
// installed, are treated as other MV2 extensions.
TEST_F(MV2DeprecationImpactCheckerUnitTest,
       MV2PolicyAllowedExtensionsMayBeAffected) {
  scoped_refptr<const Extension> allowed_policy = AddAllowedPolicyExtension(
      "allowed policy", mojom::ManifestLocation::kExternalPolicy, 2);
  scoped_refptr<const Extension> allowed_policy_download =
      AddAllowedPolicyExtension(
          "allowed policy download",
          mojom::ManifestLocation::kExternalPolicyDownload, 2);

  EXPECT_TRUE(impact_checker()->IsExtensionAffected(*allowed_policy));
  EXPECT_TRUE(impact_checker()->IsExtensionAffected(*allowed_policy_download));
}

// Tests that any MV3 extension installed by policy is never affected by
// MV2 experiments.
TEST_F(MV2DeprecationImpactCheckerUnitTest,
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
TEST_F(MV2DeprecationImpactCheckerUnitTest, NonExtensionsAreNotAffected) {
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

// Tests that user script MV2 extensions are properly considered affected by
// the MV2 deprecation experiment.
TEST_F(MV2DeprecationImpactCheckerUnitTest, UserScriptsAreAffected) {
  scoped_refptr<const Extension> user_script =
      ExtensionBuilder("user script")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .SetManifestVersion(2)
          .SetManifestKey("converted_from_user_script", true)
          .Build();
  ASSERT_EQ(Manifest::Type::kUserScript, user_script->GetType());

  EXPECT_TRUE(impact_checker()->IsExtensionAffected(*user_script));
}

// Tests the allowlist is taken into account.
TEST_F(MV2DeprecationImpactCheckerUnitTestWithAllowlist, AllowlistWorks) {
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

  // `ext_a` and `ext_b` are in the allowlist, so aren't affected.
  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*ext_a));
  EXPECT_FALSE(impact_checker()->IsExtensionAffected(*ext_b));
  // `ext_c` is not in the allowlist, so is affected.
  EXPECT_TRUE(impact_checker()->IsExtensionAffected(*ext_c));
}

}  // namespace extensions
