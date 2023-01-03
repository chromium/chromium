// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/installed_loader.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/scripting_permissions_modifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
using testing::NiceMock;
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
class MockProfileHelper : public ash::ProfileHelper {
 public:
  MOCK_METHOD(const user_manager::User*,
              GetUserByProfile,
              (const Profile* profile),
              (const, override));
  MOCK_METHOD(user_manager::User*,
              GetUserByProfile,
              (Profile * profile),
              (const, override));
  MOCK_METHOD(base::FilePath, GetActiveUserProfileDir, (), ());
  MOCK_METHOD(void, Initialize, (), ());
  MOCK_METHOD(void, FlushProfile, (Profile * profile), ());
  MOCK_METHOD(void,
              SetProfileToUserMappingForTesting,
              (user_manager::User * user),
              ());
  MOCK_METHOD(void,
              SetUserToProfileMappingForTesting,
              (const user_manager::User* user, Profile* profile),
              ());
  MOCK_METHOD(Profile*,
              GetProfileByAccountId,
              (const AccountId& account_id),
              ());
  MOCK_METHOD(Profile*, GetProfileByUser, (const user_manager::User* user), ());
  MOCK_METHOD(void,
              RemoveUserFromListForTesting,
              (const AccountId& account_id),
              ());
};

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

class InstalledLoaderUnitTest : public ExtensionServiceTestBase {
 public:
  InstalledLoaderUnitTest() {}

  InstalledLoaderUnitTest(const InstalledLoaderUnitTest&) = delete;
  InstalledLoaderUnitTest& operator=(const InstalledLoaderUnitTest&) = delete;

  ~InstalledLoaderUnitTest() override = default;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
  }

  const Extension* AddExtension(const std::vector<std::string>& permissions,
                                mojom::ManifestLocation location);

  void RunHostPermissionsMetricsTest(HostPermissionsMetricsTestParams params);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Set an expectation on the profile helper and expect a return from the
  // `ProfileCanUseNonComponentExtensions` method.
  void MockAndRunProfileCanUseNonComponentExtensionsTest(
      const user_manager::User* fake_user,
      bool expected_return);

  ash::FakeChromeUserManager* user_manager() { return fake_user_manager_; }
  NiceMock<MockProfileHelper>* profile_helper() { return &mock_profile_helper; }

 protected:
  const AccountId account_id_ =
      AccountId::FromUserEmailGaiaId("test-user@testdomain.com", "1234567890");
  NiceMock<MockProfileHelper> mock_profile_helper;

 private:
  // Setting up a fake user manager seems excessive for a unit test
  // as opposed to mocking, but it seems you cannot create a
  // `user_manager::User` without it (or a real impl).
  ash::FakeChromeUserManager* fake_user_manager_ =
      new ash::FakeChromeUserManager();
  // Tears down the FakeUserManager singleton on destruction.
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_ =
      std::make_unique<user_manager::ScopedUserManager>(
          base::WrapUnique(fake_user_manager_));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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

void InstalledLoaderUnitTest::RunHostPermissionsMetricsTest(
    HostPermissionsMetricsTestParams params) {
  const Extension* extension =
      AddExtension(params.requested_host_permissions, params.manifest_location);

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
#if BUILDFLAG(IS_CHROMEOS_ASH)

void InstalledLoaderUnitTest::MockAndRunProfileCanUseNonComponentExtensionsTest(
    const user_manager::User* fake_user,
    bool expected_return) {
  ON_CALL(testing::Const(*profile_helper()),
          GetUserByProfile(testing::An<const Profile*>()))
      .WillByDefault(testing::Return(fake_user));

  InstalledLoader loader(service());
  // testing_profile() defaults to a regular profile.
  EXPECT_EQ(expected_return, loader.ProfileCanUseNonComponentExtensions(
                                 (testing_profile()), profile_helper()));
  testing::Mock::VerifyAndClear(profile_helper());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
  params.requested_host_permissions = {"activeTab"};
  params.expected_access_level = HostPermissionsAccess::kOnActiveTabOnly;
  params.request_scope = HostPermissionsMetricsTestParams::RequestScope::kNone;

  RunHostPermissionsMetricsTest(params);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(InstalledLoaderUnitTest,
       Browser_ProfileCanUseNonComponentExtensions_RegularProfile) {
  InstalledLoader loader(service());
  // testing_profile() defaults to a regular profile.
  EXPECT_TRUE(loader.ProfileCanUseNonComponentExtensions(testing_profile()));
}

TEST_F(InstalledLoaderUnitTest,
       Browser_ProfileCannotUseNonComponentExtensions_NoProfile) {
  InstalledLoader loader(service());
  EXPECT_FALSE(loader.ProfileCanUseNonComponentExtensions(nullptr));
}

TEST_F(InstalledLoaderUnitTest,
       Browser_ProfileCannotUseNonComponentExtensions_GuestProfile) {
  testing_profile()->SetGuestSession(true);
  InstalledLoader loader(service());
  EXPECT_FALSE(loader.ProfileCanUseNonComponentExtensions(testing_profile()));
}

TEST_F(InstalledLoaderUnitTest,
       Browser_ProfileCannotUseNonComponentExtensions_IncognitoProfile) {
  TestingProfile* incognito_test_profile =
      TestingProfile::Builder().BuildIncognito(testing_profile());
  ASSERT_TRUE(incognito_test_profile);
  InstalledLoader loader(service());
  EXPECT_FALSE(
      loader.ProfileCanUseNonComponentExtensions(incognito_test_profile));
}

TEST_F(InstalledLoaderUnitTest,
       Browser_ProfileCannotUseNonComponentExtensions_OTRProfile) {
  TestingProfile* otr_test_profile =
      TestingProfile::Builder().BuildOffTheRecord(
          testing_profile(), Profile::OTRProfileID::CreateUniqueForTesting());
  ASSERT_TRUE(otr_test_profile);
  InstalledLoader loader(service());
  EXPECT_FALSE(loader.ProfileCanUseNonComponentExtensions(otr_test_profile));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(InstalledLoaderUnitTest,
       ChromeOS_ProfileCanUseNonComponentExtensions_RegularProfile) {
  const user_manager::User* user = user_manager()->AddUser(account_id_);
  ASSERT_TRUE(user);

  MockAndRunProfileCanUseNonComponentExtensionsTest(user, true);
}

TEST_F(InstalledLoaderUnitTest,
       ChromeOS_ProfileCanUseNonComponentExtensions_ChildProfile) {
  const user_manager::User* user = user_manager()->AddChildUser(account_id_);
  ASSERT_TRUE(user);

  MockAndRunProfileCanUseNonComponentExtensionsTest(user, true);
}

TEST_F(InstalledLoaderUnitTest,
       ChromeOS_ProfileCanUseNonComponentExtensions_ActiveDirectoryProfile) {
  const AccountId account_id(AccountId::AdFromUserEmailObjGuid(
      "activedirectory@gmail.com", "obj-guid"));
  const user_manager::User* user =
      user_manager()->AddActiveDirectoryUser(account_id);
  ASSERT_TRUE(user);

  MockAndRunProfileCanUseNonComponentExtensionsTest(user, true);
}

TEST_F(InstalledLoaderUnitTest,
       ChromeOS_ProfileCannotUseNonComponentExtensions_GuestProfile) {
  const user_manager::User* user = user_manager()->AddGuestUser();
  ASSERT_TRUE(user);

  MockAndRunProfileCanUseNonComponentExtensionsTest(user, false);
}

// TODO(crbug.com/1383740): Test a signin, lockscreen, or lockscreen app
// profile. `FakeChromeUserManager` doesn't have one currently. Worst case can
// mock `Profile` path.
TEST_F(InstalledLoaderUnitTest,
       ChromeOS_ProfileCannotUseNonComponentExtensions_NotUserProfile) {}

TEST_F(InstalledLoaderUnitTest,
       ChromeOS_ProfileCannotUseNonComponentExtensions_KioskAppProfile) {
  const user_manager::User* user = user_manager()->AddKioskAppUser(account_id_);
  ASSERT_TRUE(user);

  MockAndRunProfileCanUseNonComponentExtensionsTest(user, false);
}

TEST_F(InstalledLoaderUnitTest,
       ChromeOS_ProfileCannotUseNonComponentExtensions_WebKioskAppProfile) {
  const user_manager::User* user =
      user_manager()->AddWebKioskAppUser(account_id_);
  ASSERT_TRUE(user);

  MockAndRunProfileCanUseNonComponentExtensionsTest(user, false);
}

TEST_F(InstalledLoaderUnitTest,
       ChromeOS_ProfileCannotUseNonComponentExtensions_ArcKioskAppProfile) {
  const user_manager::User* user =
      user_manager()->AddArcKioskAppUser(account_id_);
  ASSERT_TRUE(user);

  MockAndRunProfileCanUseNonComponentExtensionsTest(user, false);
}

TEST_F(InstalledLoaderUnitTest,
       ChromeOS_ProfileCannotUseNonComponentExtensions_PublicProfile) {
  const user_manager::User* user =
      user_manager()->AddPublicAccountUser(account_id_);
  ASSERT_TRUE(user);

  MockAndRunProfileCanUseNonComponentExtensionsTest(user, false);
}

TEST_F(InstalledLoaderUnitTest,
       ChromeOS_ProfileCannotUseNonComponentExtensions_NoUserInProfile) {
  MockAndRunProfileCanUseNonComponentExtensionsTest(nullptr, false);
}

TEST_F(
    InstalledLoaderUnitTest,
    ChromeOS_ProfileCannotUseNonComponentExtensions_NoProfileHelperProvided) {
  InstalledLoader loader(service());
  EXPECT_FALSE(
      loader.ProfileCanUseNonComponentExtensions(testing_profile(), nullptr));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace extensions
