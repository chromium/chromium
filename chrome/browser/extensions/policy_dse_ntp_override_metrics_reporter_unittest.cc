// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/search_engines/template_url_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom.h"

namespace extensions {

namespace {

const char kDseOverrideId[] = "abcdefghijklmnopabcdefghijklmnop";
const char kNtpOverrideId[] = "ponmlkjihgfedcbaponmlkjihgfedcba";
const char kBothOverrideId[] = "abcdefponmlkjihgabcdefponmlkjihg";
const char kNormalExtensionId[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

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
    const std::string& id) {
  return ExtensionBuilder("NTP Override")
      .SetLocation(mojom::ManifestLocation::kExternalPrefDownload)
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
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    ExtensionServiceInitParams params;
    params.testing_factories.emplace_back(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    InitializeExtensionService(std::move(params));
  }

  using TestingPrefUpdater = ExtensionManagementPrefUpdater<
      sync_preferences::TestingPrefServiceSyncable>;
};

TEST_F(PolicyDseNtpOverrideMetricsReporterTest,
       LogEnabledDseOverrideInLowTrustForced) {
  base::HistogramTester histograms;

  // 1. Setup Low Trust.
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);

  // 2. Install extension and set policy to forced.
  auto extension = CreateDseOverrideExtension(kDseOverrideId);
  {
    TestingPrefUpdater updater(testing_profile()->GetTestingPrefService());
    updater.SetIndividualExtensionAutoInstalled(
        kDseOverrideId, "https://clients2.google.com/service/update2/crx",
        /*forced=*/true);
  }
  ExtensionRegistrar::Get(profile())->AddExtension(extension);

  // 3. Trigger metrics report via ExtensionService. We call
  // OnInstalledExtensionsLoadedForTest() directly because service()->Init()
  // asserts that the registry is empty at startup, which would crash here
  // since we pre-populate it with mock extensions for testing.
  service()->OnInstalledExtensionsLoadedForTest();

  // 4. Verify logging.
  histograms.ExpectUniqueSample("Extensions.DseOverride.LowTrust.Forced", 1, 1);
  histograms.ExpectUniqueSample(
      "Extensions.SettingsOverrideV2.Dse.LowTrust.Forced", 1, 1);
  EXPECT_TRUE(
      histograms
          .GetAllSamples("Extensions.SettingsOverrideV2.Ntp.LowTrust.Forced")
          .empty());
}

TEST_F(PolicyDseNtpOverrideMetricsReporterTest,
       LogDisabledNtpOverrideInHighTrustRecommended) {
  base::HistogramTester histograms;

  // 1. Setup High Trust.
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::CLOUD);

  // 2. Install extension, disable it, and set policy to recommended.
  auto extension = CreateNtpOverrideExtension(kNtpOverrideId);
  {
    TestingPrefUpdater updater(testing_profile()->GetTestingPrefService());
    updater.SetIndividualExtensionAutoInstalled(
        kNtpOverrideId, "https://clients2.google.com/service/update2/crx",
        /*forced=*/false);
  }
  ExtensionRegistrar::Get(profile())->AddExtension(extension);
  ExtensionRegistrar::Get(profile())->DisableExtension(
      kNtpOverrideId, {disable_reason::DISABLE_USER_ACTION});

  // 3. Trigger metrics report.
  service()->OnInstalledExtensionsLoadedForTest();

  // 4. Verify logging.
  histograms.ExpectUniqueSample("Extensions.NtpOverride.HighTrust.Recommended",
                                0, 1);
  histograms.ExpectUniqueSample(
      "Extensions.SettingsOverrideV2.Ntp.HighTrust.Recommended", 0, 1);
  EXPECT_TRUE(histograms
                  .GetAllSamples(
                      "Extensions.SettingsOverrideV2.Dse.HighTrust.Recommended")
                  .empty());
}

TEST_F(PolicyDseNtpOverrideMetricsReporterTest, LogBothOverrideEnabled) {
  base::HistogramTester histograms;

  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);

  auto extension = CreateBothOverrideExtension(kBothOverrideId);
  {
    TestingPrefUpdater updater(testing_profile()->GetTestingPrefService());
    updater.SetIndividualExtensionAutoInstalled(
        kBothOverrideId, "https://clients2.google.com/service/update2/crx",
        /*forced=*/true);
  }
  ExtensionRegistrar::Get(profile())->AddExtension(extension);

  service()->OnInstalledExtensionsLoadedForTest();

  histograms.ExpectUniqueSample("Extensions.BothOverride.LowTrust.Forced", 1,
                                1);
  histograms.ExpectUniqueSample(
      "Extensions.SettingsOverrideV2.Dse.LowTrust.Forced", 1, 1);
  histograms.ExpectUniqueSample(
      "Extensions.SettingsOverrideV2.Ntp.LowTrust.Forced", 1, 1);
}

TEST_F(PolicyDseNtpOverrideMetricsReporterTest, IgnoreNonPolicyExtensions) {
  base::HistogramTester histograms;

  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);

  // Create extension with kInternal (user installed) instead of policy.
  auto extension = CreateDseOverrideExtension(
      kDseOverrideId, mojom::ManifestLocation::kInternal);
  ExtensionRegistrar::Get(profile())->AddExtension(extension);
  // Even if we have it in settings as allowed, it's not policy-installed.
  {
    TestingPrefUpdater updater(testing_profile()->GetTestingPrefService());
    updater.SetIndividualExtensionInstallationAllowed(kDseOverrideId, true);
  }

  service()->OnInstalledExtensionsLoadedForTest();

  // Verify no samples are logged to any of our histograms.
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
}

TEST_F(PolicyDseNtpOverrideMetricsReporterTest,
       IgnoreNonOverridePolicyExtensions) {
  base::HistogramTester histograms;

  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);

  auto extension = CreateNormalExtension(kNormalExtensionId);
  {
    TestingPrefUpdater updater(testing_profile()->GetTestingPrefService());
    updater.SetIndividualExtensionAutoInstalled(
        kNormalExtensionId, "https://clients2.google.com/service/update2/crx",
        /*forced=*/true);
  }
  ExtensionRegistrar::Get(profile())->AddExtension(extension);

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

}  // namespace extensions
