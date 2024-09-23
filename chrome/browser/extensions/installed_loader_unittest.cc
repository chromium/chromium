// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/installed_loader.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_service_user_test_base.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/permissions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

namespace {

constexpr const char kHasWithheldHostsHistogram[] =
    "Extensions.RuntimeHostPermissions.ExtensionHasWithheldHosts";
constexpr const char kGrantedHostCountHistogram[] =
    "Extensions.RuntimeHostPermissions.GrantedHostCount";
constexpr const char kGrantedAccessHistogram[] =
    "Extensions.HostPermissions.GrantedAccess";
constexpr const char kGrantedAccessForBroadRequestsHistogram[] =
    "Extensions.HostPermissions.GrantedAccessForBroadRequests";
constexpr const char kGrantedAccessForTargetedRequestsHistogram[] =
    "Extensions.HostPermissions.GrantedAccessForTargetedRequests";
// Use an internal location for extensions since metrics aren't recorded for
// unpacked extensions.
constexpr mojom::ManifestLocation kManifestInternal =
    mojom::ManifestLocation::kInternal;
constexpr mojom::ManifestLocation kManifestExternalPolicy =
    mojom::ManifestLocation::kExternalPolicy;

struct HostPermissionsMetricsTestParams {
  // The manifest location of the extension to install.
  mojom::ManifestLocation manifest_location = kManifestInternal;

  // The host permissions the extension requests.
  std::vector<std::string> requested_host_permissions;

  // Whether the extension requests activeTab.
  bool requests_active_tab = false;

  // Whether the user enables host permission withholding for the extension.
  bool has_withholding_permissions = false;

  // The host permissions the user grants. Only valid if
  // `has_withholding_permissions` is true.
  std::vector<std::string> granted_host_permissions;

  // The expected access level to be reported by the histogram.
  HostPermissionsAccess expected_access_level;

  // The scope of the extension request, which determines if it is reported in
  // additional histograms.
  enum class RequestScope { kNone, kTargeted, kBroad };
  RequestScope request_scope = RequestScope::kNone;
};

}  // namespace

class InstalledLoaderUnitTest : public ExtensionServiceUserTestBase {
 public:
  InstalledLoaderUnitTest() {}

  InstalledLoaderUnitTest(const InstalledLoaderUnitTest&) = delete;
  InstalledLoaderUnitTest& operator=(const InstalledLoaderUnitTest&) = delete;

  ~InstalledLoaderUnitTest() override = default;

  void SetUp() override {
    ExtensionServiceUserTestBase::SetUp();
    InitializeEmptyExtensionService();
  }

  const Extension* AddExtension(const std::vector<std::string>& permissions,
                                mojom::ManifestLocation location,
                                bool requests_active_tab = false);

  void RunHostPermissionsMetricsTest(HostPermissionsMetricsTestParams params);

  void RunEmitUserHistogramsTest(int nonuser_expected_total_count,
                                 int user_expected_total_count);
};

const Extension* InstalledLoaderUnitTest::AddExtension(
    const std::vector<std::string>& host_permissions,
    mojom::ManifestLocation location,
    bool requests_active_tab) {
  ExtensionBuilder builder("test");
  builder.AddHostPermissions(host_permissions);
  builder.SetLocation(location);
  if (requests_active_tab) {
    builder.AddAPIPermission("activeTab");
  }

  scoped_refptr<const Extension> extension = builder.Build();
  PermissionsUpdater updater(profile());
  updater.InitializePermissions(extension.get());
  updater.GrantActivePermissions(extension.get());
  service()->AddExtension(extension.get());

  return extension.get();
}

void InstalledLoaderUnitTest::RunHostPermissionsMetricsTest(
    HostPermissionsMetricsTestParams params) {
  const Extension* extension =
      AddExtension(params.requested_host_permissions, params.manifest_location,
                   params.requests_active_tab);

  ScriptingPermissionsModifier modifier(profile(), extension);
  if (params.has_withholding_permissions) {
    modifier.SetWithholdHostPermissions(true);
    for (auto const& permission : params.granted_host_permissions) {
      modifier.GrantHostPermission(GURL(permission));
    }
  } else {
    DCHECK(params.granted_host_permissions.empty())
        << "granted_host_permissions are only valid if "
           "has_withholding_permission is true";
  }

  base::HistogramTester histograms;
  InstalledLoader loader(service());
  loader.RecordExtensionsMetricsForTesting();

  histograms.ExpectUniqueSample(kGrantedAccessHistogram,
                                params.expected_access_level, 1);

  switch (params.request_scope) {
    case HostPermissionsMetricsTestParams::RequestScope::kNone:
      histograms.ExpectTotalCount(kGrantedAccessForBroadRequestsHistogram, 0);
      histograms.ExpectTotalCount(kGrantedAccessForTargetedRequestsHistogram,
                                  0);
      break;
    case HostPermissionsMetricsTestParams::RequestScope::kBroad:
      histograms.ExpectUniqueSample(kGrantedAccessForBroadRequestsHistogram,
                                    params.expected_access_level, 1);
      histograms.ExpectTotalCount(kGrantedAccessForTargetedRequestsHistogram,
                                  0);
      break;
    case HostPermissionsMetricsTestParams::RequestScope::kTargeted:
      histograms.ExpectTotalCount(kGrantedAccessForBroadRequestsHistogram, 0);
      histograms.ExpectUniqueSample(kGrantedAccessForTargetedRequestsHistogram,
                                    params.expected_access_level, 1);
      break;
  }
}

// Test that certain histograms are emitted for user and non-user profiles
// (users for ChromeOS Ash).
void InstalledLoaderUnitTest::RunEmitUserHistogramsTest(
    int nonuser_expected_total_count,
    int user_expected_total_count) {
  base::HistogramTester histograms;
  InstalledLoader loader(service());
  loader.RecordExtensionsIncrementedMetricsForTesting(testing_profile());

  histograms.ExpectTotalCount("Extensions.LoadAllTime2", 1);
  histograms.ExpectTotalCount("Extensions.LoadAll", 1);
  histograms.ExpectTotalCount("Extensions.Disabled", 1);
  histograms.ExpectTotalCount("Extensions.ManifestVersion", 1);
  histograms.ExpectTotalCount("Extensions.LoadAllTime2.NonUser",
                              nonuser_expected_total_count);
  histograms.ExpectTotalCount("Extensions.LoadAllTime2.User",
                              user_expected_total_count);
  histograms.ExpectTotalCount("Extensions.LoadAll2", user_expected_total_count);
  histograms.ExpectTotalCount("Extensions.Disabled2",
                              user_expected_total_count);
  histograms.ExpectTotalCount("Extensions.ManifestVersion2",
                              user_expected_total_count);
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
  HostPermissionsMetricsTestParams params;
  // The extension is loaded from an external policy, so the user cannot
  // configure the host permissions that affect the extension.
  params.manifest_location = kManifestExternalPolicy;
  // The extension didn't request access to any eTLDs or requested host
  // permissions.
  params.expected_access_level = HostPermissionsAccess::kCannotAffect;
  params.request_scope = HostPermissionsMetricsTestParams::RequestScope::kNone;

  RunHostPermissionsMetricsTest(params);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_CannotAffect_Broad_AllUrls) {
  HostPermissionsMetricsTestParams params;
  // The extension with host permissions is loaded from an external policy, so
  // the user cannot configure the host permissions that affect the extension.
  params.manifest_location = kManifestExternalPolicy;
  params.requested_host_permissions = {"<all_urls>"};
  params.expected_access_level = HostPermissionsAccess::kCannotAffect;
  params.request_scope = HostPermissionsMetricsTestParams::RequestScope::kBroad;

  RunHostPermissionsMetricsTest(params);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_CannotAffect_Broad_Patterns) {
  HostPermissionsMetricsTestParams params;
  // The extension with host permissions is loaded from an external policy, so
  // the user cannot configure the host permissions that affect the extension.
  params.manifest_location = kManifestExternalPolicy;
  params.requested_host_permissions = {"*://*/*"};
  params.expected_access_level = HostPermissionsAccess::kCannotAffect;
  params.request_scope = HostPermissionsMetricsTestParams::RequestScope::kBroad;

  RunHostPermissionsMetricsTest(params);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_CannotAffect_Targeted) {
  HostPermissionsMetricsTestParams params;
  // The extension with host permissions is loaded from an external policy, so
  // the user cannot configure the host permissions that affect the extension.
  params.manifest_location = kManifestExternalPolicy;
  params.requested_host_permissions = {"https://example.com/"};
  params.expected_access_level = HostPermissionsAccess::kCannotAffect;
  params.request_scope =
      HostPermissionsMetricsTestParams::RequestScope::kTargeted;

  RunHostPermissionsMetricsTest(params);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_NotRequested) {
  HostPermissionsMetricsTestParams params;
  params.manifest_location = kManifestInternal;
  // The extension has no host permissions, so host permissions cannot be
  // requested.
  params.expected_access_level = HostPermissionsAccess::kNotRequested;
  params.request_scope = HostPermissionsMetricsTestParams::RequestScope::kNone;

  RunHostPermissionsMetricsTest(params);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_OnClick_Broad_AllUrls) {
  HostPermissionsMetricsTestParams params;
  params.manifest_location = kManifestInternal;
  params.requested_host_permissions = {"<all_urls>"};
  params.has_withholding_permissions = true;
  params.expected_access_level = HostPermissionsAccess::kOnClick;
  params.request_scope = HostPermissionsMetricsTestParams::RequestScope::kBroad;

  RunHostPermissionsMetricsTest(params);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_OnClick_Broad_Pattern) {
  HostPermissionsMetricsTestParams params;
  params.manifest_location = kManifestInternal;
  params.requested_host_permissions = {"*://*/*"};
  params.has_withholding_permissions = true;
  params.expected_access_level = HostPermissionsAccess::kOnClick;
  params.request_scope = HostPermissionsMetricsTestParams::RequestScope::kBroad;

  RunHostPermissionsMetricsTest(params);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_OnClick_Targeted) {
  HostPermissionsMetricsTestParams params;
  params.manifest_location = kManifestInternal;
  params.requested_host_permissions = {"https://example.com/"};
  params.has_withholding_permissions = true;
  params.expected_access_level = HostPermissionsAccess::kOnClick;
  params.request_scope =
      HostPermissionsMetricsTestParams::RequestScope::kTargeted;

  RunHostPermissionsMetricsTest(params);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_OnSpecificSites_Broad_AllUrls) {
  HostPermissionsMetricsTestParams params;
  params.manifest_location = kManifestInternal;
  params.requested_host_permissions = {"<all_urls>"};
  params.has_withholding_permissions = true;
  params.granted_host_permissions = {"https://example.com/"};
  params.expected_access_level = HostPermissionsAccess::kOnSpecificSites;
  params.request_scope = HostPermissionsMetricsTestParams::RequestScope::kBroad;

  RunHostPermissionsMetricsTest(params);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_OnSpecificSites_Broad_Pattern) {
  HostPermissionsMetricsTestParams params;
  params.manifest_location = kManifestInternal;
  params.requested_host_permissions = {"*://*/*"};
  params.has_withholding_permissions = true;
  params.granted_host_permissions = {"https://example.com/"};
  params.expected_access_level = HostPermissionsAccess::kOnSpecificSites;
  params.request_scope = HostPermissionsMetricsTestParams::RequestScope::kBroad;

  RunHostPermissionsMetricsTest(params);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_OnSpecificSites_Targeted) {
  HostPermissionsMetricsTestParams params;
  params.manifest_location = kManifestInternal;
  params.requested_host_permissions = {"https://example.com/",
                                       "https://google.com/"};
  params.has_withholding_permissions = true;
  params.granted_host_permissions = {"https://example.com/"};
  params.expected_access_level = HostPermissionsAccess::kOnSpecificSites;
  params.request_scope =
      HostPermissionsMetricsTestParams::RequestScope::kTargeted;

  RunHostPermissionsMetricsTest(params);
}

TEST_F(
    InstalledLoaderUnitTest,
    HostPermissions_Metrics_GrantedAccess_OnAllRequestedSites_Broad_AllUrls) {
  HostPermissionsMetricsTestParams params;
  params.manifest_location = kManifestInternal;
  params.requested_host_permissions = {"<all_urls>"};
  params.expected_access_level = HostPermissionsAccess::kOnAllRequestedSites;
  params.request_scope = HostPermissionsMetricsTestParams::RequestScope::kBroad;

  RunHostPermissionsMetricsTest(params);
}

TEST_F(
    InstalledLoaderUnitTest,
    HostPermissions_Metrics_GrantedAccess_OnAllRequestedSites_Broad_Pattern) {
  HostPermissionsMetricsTestParams params;
  params.manifest_location = kManifestInternal;
  params.requested_host_permissions = {"*://*/*"};
  params.expected_access_level = HostPermissionsAccess::kOnAllRequestedSites;
  params.request_scope = HostPermissionsMetricsTestParams::RequestScope::kBroad;

  RunHostPermissionsMetricsTest(params);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_OnAllRequestedSites_Targeted) {
  HostPermissionsMetricsTestParams params;
  params.manifest_location = kManifestInternal;
  params.requested_host_permissions = {"https://example.com/"};
  params.expected_access_level = HostPermissionsAccess::kOnAllRequestedSites;
  params.has_withholding_permissions = true;
  params.granted_host_permissions = {"https://example.com/"};
  params.request_scope =
      HostPermissionsMetricsTestParams::RequestScope::kTargeted;

  RunHostPermissionsMetricsTest(params);
}

TEST_F(InstalledLoaderUnitTest,
       HostPermissions_Metrics_GrantedAccess_OnActiveTabOnly) {
  HostPermissionsMetricsTestParams params;
  params.manifest_location = kManifestInternal;
  // The extension has activeTab API permission and no host permissions, so host
  // permission access is on active tab only.
  params.requests_active_tab = true;
  params.expected_access_level = HostPermissionsAccess::kOnActiveTabOnly;
  params.request_scope = HostPermissionsMetricsTestParams::RequestScope::kNone;

  RunHostPermissionsMetricsTest(params);
}

// TODO(crbug.com/40878021): After deleting the deprecated unincremented
// histograms, consider modifying these to becomes less of change detectors in
// metrics being modified.
// Tests that some histograms that only emit for profiles that can use
// non-component extensions emit as expected.
TEST_F(InstalledLoaderUnitTest, UserMetrics_UserMetricsEmitForRegularUser) {
  ASSERT_TRUE(AddExtension({"<all_urls>"}, kManifestInternal));
  ASSERT_NO_FATAL_FAILURE(MaybeSetUpTestUser(/*is_guest=*/false));

  RunEmitUserHistogramsTest(
      /*nonuser_expected_total_count=*/0,
      /*user_expected_total_count=*/1);
}

// Tests that some histograms that only emit for profiles that can use
// non-component extensions do not emit as expected.
TEST_F(InstalledLoaderUnitTest, UserMetrics_UserMetricsDoNotEmitForGuestUser) {
  ASSERT_TRUE(AddExtension({"<all_urls>"}, kManifestInternal));
  ASSERT_NO_FATAL_FAILURE(MaybeSetUpTestUser(/*is_guest=*/true));

  RunEmitUserHistogramsTest(
      /*nonuser_expected_total_count=*/1,
      /*user_expected_total_count=*/0);
}

}  // namespace extensions
