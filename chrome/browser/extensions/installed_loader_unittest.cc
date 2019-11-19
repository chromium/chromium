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

}  // namespace

class InstalledLoaderUnitTest : public ExtensionServiceTestBase {
 public:
  InstalledLoaderUnitTest() {}
  ~InstalledLoaderUnitTest() override = default;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
  }

  const Extension* AddExtension();

 private:
  DISALLOW_COPY_AND_ASSIGN(InstalledLoaderUnitTest);
};

const Extension* InstalledLoaderUnitTest::AddExtension() {
  // Metrics aren't recorded for unpacked extensions, so we need to make sure
  // the extension has an INTERNAL location.
  constexpr Manifest::Location kManifestLocation = Manifest::INTERNAL;
  scoped_refptr<const Extension> extension = ExtensionBuilder("test")
                                                 .AddPermissions({"<all_urls>"})
                                                 .SetLocation(kManifestLocation)
                                                 .Build();
  PermissionsUpdater updater(profile());
  updater.InitializePermissions(extension.get());
  updater.GrantActivePermissions(extension.get());
  service()->AddExtension(extension.get());

  return extension.get();
}

TEST_F(InstalledLoaderUnitTest,
       RuntimeHostPermissions_Metrics_HasWithheldHosts_False) {
  AddExtension();

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
  const Extension* extension = AddExtension();
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
  const Extension* extension = AddExtension();
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

}  // namespace extensions
