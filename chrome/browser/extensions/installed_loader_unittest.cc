// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/installed_loader.h"

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"

namespace extensions {

namespace {

constexpr const char kHasWithheldHostsHistogram[] =
    "Extensions.RuntimeHostPermissions.ExtensionHasWithheldHosts";
constexpr const char kGrantedHostCountHistogram[] =
    "Extensions.RuntimeHostPermissions.GrantedHostCount";
constexpr const char kGrantedAccessHistogram[] =
    "Extensions.HostPermissions.GrantedAccess";
// Use an internal location for extensions since metrics aren't recorded for
// unpacked extensions.
constexpr mojom::ManifestLocation kManifestInternal =
    mojom::ManifestLocation::kInternal;
constexpr mojom::ManifestLocation kManifestExternalPolicy =
    mojom::ManifestLocation::kExternalPolicy;

}  // namespace

class InstalledLoaderUnitTest : public ExtensionServiceTestBase {
 public:
  InstalledLoaderUnitTest() {}
  ~InstalledLoaderUnitTest() override = default;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
  }

  const Extension* AddExtension(const std::vector<std::string>& permissions,
                                mojom::ManifestLocation location);

 private:
  DISALLOW_COPY_AND_ASSIGN(InstalledLoaderUnitTest);
};

const Extension* InstalledLoaderUnitTest::AddExtension(
    const std::vector<std::string>& permissions,
    mojom::ManifestLocation location) {
  scoped_refptr<const Extension> extension = ExtensionBuilder("test")
                                                 .AddPermissions(permissions)
                                                 .SetLocation(location)
                                                 .Build();
  PermissionsUpdater updater(profile());
  updater.InitializePermissions(extension.get());
  updater.GrantActivePermissions(extension.get());
  service()->AddExtension(extension.get());

  return extension.get();
}

TEST_F(InstalledLoaderUnitTest,
       RuntimeHostPermissions_Metrics_HasWithheldHosts_False) {
  AddExtension({"<all_urls>"}, kManifestInternal);

  base::HistogramTester histograms;
  InstalledLoader loader(service());
  loader.RecordExtensionsMetricsForTesting();

  // The extension didn't have withheld hosts, so a single `false` record
  // should be present.
  histograms.ExpectUniqueSample(kHasWithheldHostsHistogram, false, 1);
  // Granted host counts should only be recorded if the extension had withheld
  // hosts.
  histograms.ExpectTotalCount(kGrantedHostCountHistogram, 0);
}

TEST_F(InstalledLoaderUnitTest,
       RuntimeHostPermissions_Metrics_HasWithheldHosts_True) {
  const Extension* extension = AddExtension({"<all_urls>"}, kManifestInternal);
  ScriptingPermissionsModifier(profile(), extension)
      .SetWithholdHostPermissions(true);

  base::HistogramTester histograms;
  InstalledLoader loader(service());
  loader.RecordExtensionsMetricsForTesting();

  // The extension had withheld hosts, so a single `true` record should be
  // present.
  histograms.ExpectUniqueSample(kHasWithheldHostsHistogram, true, 1);
  // There were no granted hosts, so a single `0` record should be present.
  constexpr int kGrantedHostCount = 0;
  constexpr int kEmitCount = 1;
  histograms.ExpectUniqueSample(kGrantedHostCountHistogram, kGrantedHostCount,
                                kEmitCount);
}

TEST_F(InstalledLoaderUnitTest,
       RuntimeHostPermissions_Metrics_GrantedHostCount) {
  const Extension* extension = AddExtension({"<all_urls>"}, kManifestInternal);
  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);
  modifier.GrantHostPermission(GURL("https://example.com/"));
  modifier.GrantHostPermission(GURL("https://chromium.org/"));

  base::HistogramTester histograms;
  InstalledLoader loader(service());
  loader.RecordExtensionsMetricsForTesting();

  histograms.ExpectUniqueSample(kHasWithheldHostsHistogram, true, 1);
  // The extension had granted hosts, so a single `2` record should be present.
  constexpr int kGrantedHostCount = 2;
  constexpr int kEmitCount = 1;
  histograms.ExpectUniqueSample(kGrantedHostCountHistogram, kGrantedHostCount,
                                kEmitCount);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_CannotAffect) {
  // The extension is loaded from an external policy, so the user cannot
  // configure the host permissions that affect the extension.
  AddExtension({}, kManifestExternalPolicy);

  base::HistogramTester histograms;
  InstalledLoader loader(service());
  loader.RecordExtensionsMetricsForTesting();

  histograms.ExpectUniqueSample(kGrantedAccessHistogram,
                                HostPermissionsAccess::kCannotAffect, 1);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_NotRequested) {
  // The extension has no host permissions, so host permissions cannot be
  // requested.
  AddExtension({}, kManifestInternal);

  base::HistogramTester histograms;
  InstalledLoader loader(service());
  loader.RecordExtensionsMetricsForTesting();

  histograms.ExpectUniqueSample(kGrantedAccessHistogram,
                                HostPermissionsAccess::kNotRequested, 1);
}

TEST_F(InstalledLoaderUnitTest, HostPermissions_Metrics_GrantedAccess_OnClick) {
  const Extension* extension = AddExtension({"<all_urls>"}, kManifestInternal);

  // The user withhelds all host permissions.
  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);

  base::HistogramTester histograms;
  InstalledLoader loader(service());
  loader.RecordExtensionsMetricsForTesting();

  histograms.ExpectUniqueSample(kGrantedAccessHistogram,
                                HostPermissionsAccess::kOnClick, 1);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_OnSpecificSites) {
  const Extension* extension = AddExtension({"<all_urls>"}, kManifestInternal);

  // The user grants host permisions to one of all the urls requested.
  ScriptingPermissionsModifier modifier(profile(), extension);
  modifier.SetWithholdHostPermissions(true);
  modifier.GrantHostPermission(GURL("https://example.com/"));

  base::HistogramTester histograms;
  InstalledLoader loader(service());
  loader.RecordExtensionsMetricsForTesting();

  histograms.ExpectUniqueSample(kGrantedAccessHistogram,
                                HostPermissionsAccess::kOnSpecificSites, 1);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_OnAllRequestedSites) {
  {
    AddExtension({"<all_urls>"}, kManifestInternal);

    // The user doesn't withheld any host permissions.
    base::HistogramTester histograms;
    InstalledLoader loader(service());
    loader.RecordExtensionsMetricsForTesting();

    histograms.ExpectUniqueSample(kGrantedAccessHistogram,
                                  HostPermissionsAccess::kOnAllRequestedSites,
                                  1);
  }

  {
    const Extension* extension =
        AddExtension({"https://example.com/"}, kManifestInternal);

    // The user grants host permisions to all requested urls.
    ScriptingPermissionsModifier modifier(profile(), extension);
    modifier.SetWithholdHostPermissions(true);
    modifier.GrantHostPermission(GURL("https://example.com/"));

    base::HistogramTester histograms;
    InstalledLoader loader(service());
    loader.RecordExtensionsMetricsForTesting();

    histograms.ExpectUniqueSample(kGrantedAccessHistogram,
                                  HostPermissionsAccess::kOnAllRequestedSites,
                                  1);
  }
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_OnActiveTabOnly) {
  // The extension has activeTab API permission and no host permissions, so host
  // permission access is on active tab only.
  AddExtension({"activeTab"}, kManifestInternal);

  base::HistogramTester histograms;
  InstalledLoader loader(service());
  loader.RecordExtensionsMetricsForTesting();

  histograms.ExpectUniqueSample(kGrantedAccessHistogram,
                                HostPermissionsAccess::kOnActiveTabOnly, 1);
}

}  // namespace extensions
