// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/policy_dse_ntp_override_metrics_reporter.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/low_trust_policy_install_block_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom.h"

namespace extensions {

namespace {

constexpr char kDseOverrideId[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kNtpOverrideId[] = "ponmlkjihgfedcbaponmlkjihgfedcba";
constexpr char kBothOverrideId[] = "abcdefponmlkjihgabcdefponmlkjihg";
constexpr char kNormalExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
constexpr char kUpdateUrl[] = "https://clients2.google.com/service/update2/crx";

base::DictValue CreateSearchProviderDict() {
  base::DictValue search_provider;
  search_provider.Set("name", "Fake Search");
  search_provider.Set("keyword", "fake");
  search_provider.Set("search_url", "http://fake.com/?q={searchTerms}");
  search_provider.Set("encoding", "UTF-8");
  search_provider.Set("favicon_url", "http://fake.com/favicon.ico");
  search_provider.Set("is_default", true);
  return search_provider;
}

base::DictValue CreateUrlOverridesDict() {
  base::DictValue chrome_url_overrides;
  chrome_url_overrides.Set("newtab", "custom_tab.html");
  return chrome_url_overrides;
}

scoped_refptr<const Extension> CreateDseOverrideExtension(
    const std::string& id,
    mojom::ManifestLocation location =
        mojom::ManifestLocation::kExternalPolicyDownload) {
  base::DictValue chrome_settings_overrides;
  chrome_settings_overrides.Set("search_provider", CreateSearchProviderDict());

  return ExtensionBuilder("DSE Override")
      .SetLocation(location)
      .SetID(id)
      .SetManifestKey("chrome_settings_overrides",
                      std::move(chrome_settings_overrides))
      .Build();
}

scoped_refptr<const Extension> CreateNtpOverrideExtension(
    const std::string& id,
    mojom::ManifestLocation location =
        mojom::ManifestLocation::kExternalPrefDownload) {
  return ExtensionBuilder("NTP Override")
      .SetLocation(location)
      .SetID(id)
      .SetManifestKey("chrome_url_overrides", CreateUrlOverridesDict())
      .Build();
}

scoped_refptr<const Extension> CreateBothOverrideExtension(
    const std::string& id) {
  base::DictValue chrome_settings_overrides;
  chrome_settings_overrides.Set("search_provider", CreateSearchProviderDict());

  return ExtensionBuilder("Both Override")
      .SetLocation(mojom::ManifestLocation::kExternalPolicyDownload)
      .SetID(id)
      .SetManifestKey("chrome_settings_overrides",
                      std::move(chrome_settings_overrides))
      .SetManifestKey("chrome_url_overrides", CreateUrlOverridesDict())
      .Build();
}

scoped_refptr<const Extension> CreateNormalExtension(const std::string& id) {
  return ExtensionBuilder("Normal Extension")
      .SetLocation(mojom::ManifestLocation::kExternalPolicyDownload)
      .SetID(id)
      .Build();
}

}  // namespace

class PolicyDseNtpOverrideMetricsReporterTest
    : public ExtensionServiceTestBase {
 protected:
  using TestingPrefUpdater = ExtensionManagementPrefUpdater<
      sync_preferences::TestingPrefServiceSyncable>;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    ExtensionServiceInitParams params;
    // SettingsOverridesAPI::OnExtensionLoaded() DCHECKs TemplateURLService when
    // ExtensionRegistrar::AddExtension() activates a DSE-overriding extension.
    params.testing_factories.emplace_back(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    InitializeExtensionService(std::move(params));
  }

  void SetAutoInstalled(const ExtensionId& id, bool forced) {
    TestingPrefUpdater updater(testing_profile()->GetTestingPrefService());
    updater.SetIndividualExtensionAutoInstalled(id, kUpdateUrl, forced);
  }

  void MarkBlocked(
      const ExtensionId& id,
      util::DseNtpOverrideType override_type = util::DseNtpOverrideType::kDse) {
    ExtensionManagementFactory::GetForBrowserContext(profile())
        ->low_trust_block_manager()
        ->MarkBlocked(id, BlockedExtensionInfo{.override_type = override_type,
                                               .update_url = kUpdateUrl,
                                               .timestamp = base::Time::Now()});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      kBlockPolicyDseNtpOverridesInLowTrust};
};

TEST_F(PolicyDseNtpOverrideMetricsReporterTest,
       LogEnabledDseOverrideInLowTrustForced) {
  // Disable the low-trust blocking feature so that a policy-installed DSE
  // override extension remains enabled in a low-trust environment, matching
  // the pre-enforcement baseline population.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kBlockPolicyDseNtpOverridesInLowTrust);

  base::HistogramTester histograms;
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);

  SetAutoInstalled(kDseOverrideId, /*forced=*/true);
  ExtensionRegistrar::Get(profile())->AddExtension(
      CreateDseOverrideExtension(kDseOverrideId));

  // Trigger metrics report via ExtensionService. We call
  // OnInstalledExtensionsLoadedForTest() directly because service()->Init()
  // asserts that the registry is empty at startup, which would crash here
  // since we pre-populate it with mock extensions for testing.
  service()->OnInstalledExtensionsLoadedForTest();

  histograms.ExpectUniqueSample("Extensions.DseOverride.LowTrust.Forced",
                                PolicyExtensionStatus::kEnabled, 1);
  histograms.ExpectUniqueSample(
      "Extensions.SettingsOverrideV2.Dse.LowTrust.Forced",
      PolicyExtensionStatus::kEnabled, 1);
  EXPECT_TRUE(
      histograms
          .GetAllSamples("Extensions.SettingsOverrideV2.Ntp.LowTrust.Forced")
          .empty());
}

TEST_F(PolicyDseNtpOverrideMetricsReporterTest,
       LogDisabledNtpOverrideInHighTrustRecommended) {
  base::HistogramTester histograms;
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::CLOUD);

  SetAutoInstalled(kNtpOverrideId, /*forced=*/false);
  ExtensionRegistrar::Get(profile())->AddExtension(
      CreateNtpOverrideExtension(kNtpOverrideId));
  ExtensionRegistrar::Get(profile())->DisableExtension(
      kNtpOverrideId, {disable_reason::DISABLE_USER_ACTION});

  service()->OnInstalledExtensionsLoadedForTest();

  histograms.ExpectUniqueSample("Extensions.NtpOverride.HighTrust.Recommended",
                                PolicyExtensionStatus::kDisabled, 1);
  histograms.ExpectUniqueSample(
      "Extensions.SettingsOverrideV2.Ntp.HighTrust.Recommended",
      PolicyExtensionStatus::kDisabled, 1);
  EXPECT_TRUE(histograms
                  .GetAllSamples(
                      "Extensions.SettingsOverrideV2.Dse.HighTrust.Recommended")
                  .empty());
}

TEST_F(PolicyDseNtpOverrideMetricsReporterTest, LogBothOverrideEnabled) {
  // Disable the low-trust blocking feature so that a policy-installed
  // DSE+NTP override extension remains enabled in a low-trust environment.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kBlockPolicyDseNtpOverridesInLowTrust);

  base::HistogramTester histograms;
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);

  SetAutoInstalled(kBothOverrideId, /*forced=*/true);
  ExtensionRegistrar::Get(profile())->AddExtension(
      CreateBothOverrideExtension(kBothOverrideId));

  service()->OnInstalledExtensionsLoadedForTest();

  histograms.ExpectUniqueSample("Extensions.BothOverride.LowTrust.Forced",
                                PolicyExtensionStatus::kEnabled, 1);
  histograms.ExpectUniqueSample(
      "Extensions.SettingsOverrideV2.Dse.LowTrust.Forced",
      PolicyExtensionStatus::kEnabled, 1);
  histograms.ExpectUniqueSample(
      "Extensions.SettingsOverrideV2.Ntp.LowTrust.Forced",
      PolicyExtensionStatus::kEnabled, 1);
}

TEST_F(PolicyDseNtpOverrideMetricsReporterTest, IgnoreNonPolicyExtensions) {
  base::HistogramTester histograms;
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);

  // 1. User-installed (kInternal) extension with "allowed" policy mode.
  ExtensionRegistrar::Get(profile())->AddExtension(CreateDseOverrideExtension(
      kDseOverrideId, mojom::ManifestLocation::kInternal));
  {
    TestingPrefUpdater updater(testing_profile()->GetTestingPrefService());
    updater.SetIndividualExtensionInstallationAllowed(kDseOverrideId, true);
  }

  // 2. User-installed (kInternal) extension with "forced" policy mode, but not
  // policy-installed and not in the low-trust block cache.
  ExtensionRegistrar::Get(profile())->AddExtension(CreateNtpOverrideExtension(
      kNtpOverrideId, mojom::ManifestLocation::kInternal));
  SetAutoInstalled(kNtpOverrideId, /*forced=*/true);

  service()->OnInstalledExtensionsLoadedForTest();

  EXPECT_TRUE(histograms.GetAllSamples("Extensions.DseOverride.LowTrust.Forced")
                  .empty());
  EXPECT_TRUE(
      histograms.GetAllSamples("Extensions.DseOverride.LowTrust.Recommended")
          .empty());
  EXPECT_TRUE(
      histograms
          .GetAllSamples("Extensions.SettingsOverrideV2.Dse.LowTrust.Forced")
          .empty());
  EXPECT_TRUE(histograms
                  .GetAllSamples(
                      "Extensions.SettingsOverrideV2.Dse.LowTrust.Recommended")
                  .empty());
  EXPECT_TRUE(histograms.GetAllSamples("Extensions.NtpOverride.LowTrust.Forced")
                  .empty());
  EXPECT_TRUE(
      histograms
          .GetAllSamples("Extensions.SettingsOverrideV2.Ntp.LowTrust.Forced")
          .empty());
}

TEST_F(PolicyDseNtpOverrideMetricsReporterTest,
       IgnoreNonOverridePolicyExtensions) {
  base::HistogramTester histograms;
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);

  SetAutoInstalled(kNormalExtensionId, /*forced=*/true);
  ExtensionRegistrar::Get(profile())->AddExtension(
      CreateNormalExtension(kNormalExtensionId));

  service()->OnInstalledExtensionsLoadedForTest();

  EXPECT_TRUE(histograms.GetAllSamples("Extensions.DseOverride.LowTrust.Forced")
                  .empty());
  EXPECT_TRUE(histograms.GetAllSamples("Extensions.NtpOverride.LowTrust.Forced")
                  .empty());
  EXPECT_TRUE(
      histograms.GetAllSamples("Extensions.BothOverride.LowTrust.Forced")
          .empty());
  EXPECT_TRUE(
      histograms
          .GetAllSamples("Extensions.SettingsOverrideV2.Dse.LowTrust.Forced")
          .empty());
  EXPECT_TRUE(
      histograms
          .GetAllSamples("Extensions.SettingsOverrideV2.Ntp.LowTrust.Forced")
          .empty());
}

TEST_F(PolicyDseNtpOverrideMetricsReporterTest,
       LogBlockedDseOverrideInLowTrustForced) {
  base::HistogramTester histograms;
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);

  SetAutoInstalled(kDseOverrideId, /*forced=*/true);
  MarkBlocked(kDseOverrideId, util::DseNtpOverrideType::kDse);

  // Also verify a blocked kBoth extension in recommended mode logs to
  // BothOverride (legacy) and both Dse and Ntp (V2).
  SetAutoInstalled(kBothOverrideId, /*forced=*/false);
  MarkBlocked(kBothOverrideId, util::DseNtpOverrideType::kBoth);

  service()->OnInstalledExtensionsLoadedForTest();

  histograms.ExpectUniqueSample("Extensions.DseOverride.LowTrust.Forced",
                                PolicyExtensionStatus::kBlocked, 1);
  histograms.ExpectUniqueSample(
      "Extensions.SettingsOverrideV2.Dse.LowTrust.Forced",
      PolicyExtensionStatus::kBlocked, 1);
  EXPECT_TRUE(
      histograms
          .GetAllSamples("Extensions.SettingsOverrideV2.Ntp.LowTrust.Forced")
          .empty());

  histograms.ExpectUniqueSample("Extensions.BothOverride.LowTrust.Recommended",
                                PolicyExtensionStatus::kBlocked, 1);
  histograms.ExpectUniqueSample(
      "Extensions.SettingsOverrideV2.Dse.LowTrust.Recommended",
      PolicyExtensionStatus::kBlocked, 1);
  histograms.ExpectUniqueSample(
      "Extensions.SettingsOverrideV2.Ntp.LowTrust.Recommended",
      PolicyExtensionStatus::kBlocked, 1);
}

TEST_F(PolicyDseNtpOverrideMetricsReporterTest,
       LogUserInstalledOverridesInLowTrustForced) {
  base::HistogramTester histograms;
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);

  // Enabled user-installed DSE override with blocked policy takeover.
  SetAutoInstalled(kDseOverrideId, /*forced=*/true);
  MarkBlocked(kDseOverrideId, util::DseNtpOverrideType::kDse);
  ExtensionRegistry::Get(profile())->AddEnabled(CreateDseOverrideExtension(
      kDseOverrideId, mojom::ManifestLocation::kInternal));

  // Disabled user-installed NTP override with blocked policy takeover.
  SetAutoInstalled(kNtpOverrideId, /*forced=*/true);
  MarkBlocked(kNtpOverrideId, util::DseNtpOverrideType::kNtp);
  ExtensionRegistry::Get(profile())->AddDisabled(CreateNtpOverrideExtension(
      kNtpOverrideId, mojom::ManifestLocation::kInternal));

  service()->OnInstalledExtensionsLoadedForTest();

  histograms.ExpectUniqueSample("Extensions.DseOverride.LowTrust.Forced",
                                PolicyExtensionStatus::kUserInstalledEnabled,
                                1);
  histograms.ExpectUniqueSample(
      "Extensions.SettingsOverrideV2.Dse.LowTrust.Forced",
      PolicyExtensionStatus::kUserInstalledEnabled, 1);
  histograms.ExpectUniqueSample("Extensions.NtpOverride.LowTrust.Forced",
                                PolicyExtensionStatus::kUserInstalledDisabled,
                                1);
  histograms.ExpectUniqueSample(
      "Extensions.SettingsOverrideV2.Ntp.LowTrust.Forced",
      PolicyExtensionStatus::kUserInstalledDisabled, 1);
}

TEST_F(PolicyDseNtpOverrideMetricsReporterTest,
       IgnoreBlockedCacheEntryWhenHighTrustOrPolicyRemoved) {
  base::HistogramTester histograms;
  LowTrustPolicyInstallBlockManager* block_manager =
      ExtensionManagementFactory::GetForBrowserContext(profile())
          ->low_trust_block_manager();

  // 1. In low trust, a cached block entry whose policy is no longer Forced or
  // Recommended should not be logged. Configure the "allowed" policy before
  // MarkBlocked() so that OnExtensionManagementSettingsChanged() does not evict
  // the cache entry before ReportMetrics() runs.
  {
    policy::ScopedManagementServiceOverrideForTesting browser_management(
        policy::ManagementServiceFactory::GetForPlatform(),
        policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);
    {
      TestingPrefUpdater updater(testing_profile()->GetTestingPrefService());
      updater.SetIndividualExtensionInstallationAllowed(kDseOverrideId, true);
    }
    MarkBlocked(kDseOverrideId, util::DseNtpOverrideType::kDse);
    ASSERT_TRUE(block_manager->IsBlocked(kDseOverrideId));
    service()->OnInstalledExtensionsLoadedForTest();
  }

  // 2. In high trust, a cached block entry (even with a Forced policy) should
  // not be logged because low-trust blocking is not active.
  {
    policy::ScopedManagementServiceOverrideForTesting browser_management(
        policy::ManagementServiceFactory::GetForPlatform(),
        policy::EnterpriseManagementAuthority::CLOUD);
    SetAutoInstalled(kDseOverrideId, /*forced=*/true);
    MarkBlocked(kDseOverrideId, util::DseNtpOverrideType::kDse);
    ASSERT_TRUE(block_manager->IsBlocked(kDseOverrideId));
    service()->OnInstalledExtensionsLoadedForTest();
  }

  EXPECT_TRUE(histograms.GetAllSamples("Extensions.DseOverride.LowTrust.Forced")
                  .empty());
  EXPECT_TRUE(
      histograms.GetAllSamples("Extensions.DseOverride.HighTrust.Forced")
          .empty());
  EXPECT_TRUE(
      histograms
          .GetAllSamples("Extensions.SettingsOverrideV2.Dse.LowTrust.Forced")
          .empty());
  EXPECT_TRUE(
      histograms
          .GetAllSamples("Extensions.SettingsOverrideV2.Dse.HighTrust.Forced")
          .empty());
}

}  // namespace extensions
