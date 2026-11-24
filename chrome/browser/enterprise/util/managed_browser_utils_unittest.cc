// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/managed_browser_utils.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/host_port_pair.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/with_feature_override.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/gaia_id.h"
#endif  // BUILDFLAG(IS_ANDROID)

using testing::UnorderedElementsAre;

namespace {
const char kRequestingUrl[] = "https://www.example.com";

// A GMock matcher that checks whether the
// `std::unique_ptr<net::ClientCertIdentity>` argument has the certificate
// that's equal to the expected `scoped_refptr<net::X509Certificate>`.
MATCHER_P(CertEq, expected_cert, "") {
  return arg->certificate()->EqualsExcludingChain(expected_cert.get());
}

}  // namespace

TEST(ManagedBrowserUtils, NoPolicies) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  EXPECT_FALSE(enterprise_util::IsBrowserManaged(&profile));
}

TEST(ManagedBrowserUtils, HasManagedConnector) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile::Builder builder;
  builder.OverridePolicyConnectorIsManagedForTesting(true);

  std::unique_ptr<TestingProfile> profile = builder.Build();
  EXPECT_TRUE(enterprise_util::IsBrowserManaged(profile.get()));
}

TEST(ManagedBrowserUtils, GetRequestingUrl) {
  GURL expected("https://hostname:1234");
  net::HostPortPair host_port_pair("hostname", 1234);
  EXPECT_EQ(expected, enterprise_util::GetRequestingUrl(host_port_pair));
}

#if !BUILDFLAG(IS_CHROMEOS)
class ManagedBrowserUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_provider_ =
        std::make_unique<policy::MockConfigurationPolicyProvider>();
    mock_provider_->Init();
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(
        mock_provider_.get());
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("profile_name");
  }

  void TearDown() override {
    mock_provider_->Shutdown();
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(nullptr);
  }

  TestingProfile* profile() { return profile_; }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::MockConfigurationPolicyProvider> mock_provider_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
};

TEST_F(ManagedBrowserUtilsTest, HasMachineLevelPolicies) {
  policy::PolicyMap map;
  map.Set("test-policy", policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
          base::Value("hello"), nullptr);
  mock_provider_->UpdateChromePolicy(map);

  EXPECT_TRUE(enterprise_util::IsBrowserManaged(profile()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
TEST_F(ManagedBrowserUtilsTest, WorkProfileDefaultLabel) {
  // Ensure enterprise badging can be shown.
  std::u16string work_label = u"Work";

  {
    enterprise_util::SetUserAcceptedAccountManagement(profile(), true);
    policy::ScopedManagementServiceOverrideForTesting platform_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::CLOUD);
    EXPECT_EQ(enterprise_util::GetEnterpriseLabel(profile()), work_label);
  }

  {
    enterprise_util::SetUserAcceptedAccountManagement(profile(), false);
    policy::ScopedManagementServiceOverrideForTesting platform_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::CLOUD);
    EXPECT_NE(enterprise_util::GetEnterpriseLabel(profile()), work_label);
  }

  {
    enterprise_util::SetUserAcceptedAccountManagement(profile(), true);
    policy::ScopedManagementServiceOverrideForTesting platform_management(
        policy::ManagementServiceFactory::GetForProfile(profile()),
        policy::EnterpriseManagementAuthority::NONE);
    EXPECT_NE(enterprise_util::GetEnterpriseLabel(profile()), work_label);
  }

  {
    enterprise_util::SetUserAcceptedAccountManagement(profile(), false);
    EXPECT_NE(enterprise_util::GetEnterpriseLabel(profile()), work_label);
  }
}

TEST_F(ManagedBrowserUtilsTest, DefaultLabelDisabledbyPolicy) {
  profile()->GetPrefs()->SetInteger(
      prefs::kEnterpriseProfileBadgeToolbarSettings, 1);
  enterprise_util::SetUserAcceptedAccountManagement(profile(), true);
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  // There should be no text because the policy fully disables badging.
  EXPECT_EQ(enterprise_util::GetEnterpriseLabel(profile()), std::u16string());
}

TEST_F(ManagedBrowserUtilsTest, CustomLabelDisabledbyPolicy) {
  profile()->GetPrefs()->SetString(prefs::kEnterpriseCustomLabelForProfile,
                                   "Custom Label");
  profile()->GetPrefs()->SetInteger(
      prefs::kEnterpriseProfileBadgeToolbarSettings, 1);
  enterprise_util::SetUserAcceptedAccountManagement(profile(), true);
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  // There should be no label because the policy fully disables badging.
  EXPECT_EQ(enterprise_util::GetEnterpriseLabel(profile()), std::u16string());
}

TEST_F(ManagedBrowserUtilsTest, CustomLabelTruncated) {
  profile()->GetPrefs()->SetString(prefs::kEnterpriseCustomLabelForProfile,
                                   "Custom Label Can Be Max 16 Characters");
  enterprise_util::SetUserAcceptedAccountManagement(profile(), true);
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD);

  EXPECT_EQ(enterprise_util::GetEnterpriseLabel(profile()),
            u"Custom Label Can Be Max 16 Characters");
  // The text should be truncated to 16 characters followed by ellipsis.
  EXPECT_EQ(enterprise_util::GetEnterpriseLabel(profile(), true),
            u"Custom Label Can…");
}
#endif

#if BUILDFLAG(IS_ANDROID)
class ManagedBrowserUtilsAndroidTest : public testing::Test {
 protected:
  TestingProfile* profile() { return &profile_; }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ManagedBrowserUtilsAndroidTest,
       UserAcceptedAccountManagementAndroid_DefaultsToFalse) {
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagementAndroid(
      profile(), GaiaId("gaia_id")));
}

TEST_F(ManagedBrowserUtilsAndroidTest,
       SetUserAcceptedAccountManagementAndroid) {
  const GaiaId gaia_id("gaia_id");

  enterprise_util::SetUserAcceptedAccountManagementAndroid(profile(), gaia_id,
                                                           true);
  EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagementAndroid(profile(),
                                                                    gaia_id));

  enterprise_util::SetUserAcceptedAccountManagementAndroid(profile(), gaia_id,
                                                           false);
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagementAndroid(profile(),
                                                                     gaia_id));
}

TEST_F(ManagedBrowserUtilsAndroidTest,
       SetUserAcceptedAccountManagementAndroid_IsPerAccount) {
  const GaiaId accepted_gaia_id("accepted");
  const GaiaId other_gaia_id("other");

  enterprise_util::SetUserAcceptedAccountManagementAndroid(
      profile(), accepted_gaia_id, true);

  EXPECT_TRUE(enterprise_util::UserAcceptedAccountManagementAndroid(
      profile(), accepted_gaia_id));
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagementAndroid(
      profile(), other_gaia_id));
}

TEST_F(ManagedBrowserUtilsAndroidTest,
       SetUserAcceptedAccountManagementAndroid_SharesDeviceSignalsConsentBit) {
  const GaiaId gaia_id("gaia_id");
  SigninPrefs signin_prefs(*profile()->GetPrefs());

  // Accepting management records the device signals consent.
  enterprise_util::SetUserAcceptedAccountManagementAndroid(profile(), gaia_id,
                                                           true);
  EXPECT_TRUE(signin_prefs.GetDeviceSignalsPermanentConsentReceived(gaia_id));

  // And the device signals consent is reflected as management acceptance.
  signin_prefs.SetDeviceSignalsPermanentConsentReceived(gaia_id, false);
  EXPECT_FALSE(enterprise_util::UserAcceptedAccountManagementAndroid(profile(),
                                                                     gaia_id));
}

// Tests for `ProfileCanBeManaged()` on Android. Unlike the fixture above, this
// one registers a `TestingProfileManager` (`ProfileCanBeManaged()` bails out
// early without one) and an `IdentityTestEnvironment` so a primary account can
// be set.
class ManagedBrowserUtilsProfileCanBeManagedAndroidTest
    : public base::test::WithFeatureOverride,
      public testing::Test {
 protected:
  ManagedBrowserUtilsProfileCanBeManagedAndroidTest()
      : base::test::WithFeatureOverride(
            chrome::android::kAndroidNewManagementNotice) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(
        "profile_name", IdentityTestEnvironmentProfileAdaptor::
                            GetIdentityTestEnvironmentFactories());
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
  }

  void TearDown() override {
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
  }

  TestingProfile* profile() { return profile_; }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  ProfileAttributesEntry* GetProfileAttributesEntry() {
    return profile_manager_.profile_attributes_storage()
        ->GetProfileAttributesWithPath(profile_->GetPath());
  }

  // Signs in with `ConsentLevel::kSignin` and mirrors the sign-in state onto
  // the `ProfileAttributesEntry`, which is what the legacy (profile-attribute
  // based) implementation of `ProfileCanBeManaged()` consults.
  AccountInfo SignIn() {
    AccountInfo account = identity_test_env()->MakePrimaryAccountAvailable(
        "user@example.com", signin::ConsentLevel::kSignin);
    GetProfileAttributesEntry()->SetAuthInfo(
        account.GetGaiaId(), base::UTF8ToUTF16(account.GetEmail()),
        /*is_consented_primary_account=*/false);
    return account;
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
};

TEST_P(ManagedBrowserUtilsProfileCanBeManagedAndroidTest, NullProfile) {
  EXPECT_FALSE(enterprise_util::ProfileCanBeManaged(nullptr));
}

TEST_P(ManagedBrowserUtilsProfileCanBeManagedAndroidTest, NotSignedIn) {
  EXPECT_FALSE(enterprise_util::ProfileCanBeManaged(profile()));
}

TEST_P(ManagedBrowserUtilsProfileCanBeManagedAndroidTest,
       SignedInWithoutAcceptingManagement) {
  SignIn();
  EXPECT_FALSE(enterprise_util::ProfileCanBeManaged(profile()));
}

TEST_P(ManagedBrowserUtilsProfileCanBeManagedAndroidTest,
       SignedInAndAcceptedManagementPerAccount) {
  AccountInfo account = SignIn();
  enterprise_util::SetUserAcceptedAccountManagementAndroid(
      profile(), account.GetGaiaId(), /*accepted=*/true);

  // The per-account bit is only consulted by the new implementation.
  EXPECT_EQ(enterprise_util::ProfileCanBeManaged(profile()),
            IsParamFeatureEnabled());
}

TEST_P(ManagedBrowserUtilsProfileCanBeManagedAndroidTest,
       SignedInAndAcceptedManagementOnProfileAttributes) {
  SignIn();
  GetProfileAttributesEntry()->SetUserAcceptedAccountManagement(true);

  // The profile attribute is only consulted by the legacy implementation.
  EXPECT_EQ(enterprise_util::ProfileCanBeManaged(profile()),
            !IsParamFeatureEnabled());
}

TEST_P(ManagedBrowserUtilsProfileCanBeManagedAndroidTest,
       AcceptedManagementForOtherAccount) {
  SignIn();
  enterprise_util::SetUserAcceptedAccountManagementAndroid(
      profile(), GaiaId("other_account"), /*accepted=*/true);
  EXPECT_FALSE(enterprise_util::ProfileCanBeManaged(profile()));
}

TEST_P(ManagedBrowserUtilsProfileCanBeManagedAndroidTest,
       PerAccountAcceptanceSurvivesSignOut) {
  if (!IsParamFeatureEnabled()) {
    GTEST_SKIP() << "Per-account acceptance is only used with the feature.";
  }

  AccountInfo account = SignIn();
  enterprise_util::SetUserAcceptedAccountManagementAndroid(
      profile(), account.GetGaiaId(), /*accepted=*/true);
  ASSERT_TRUE(enterprise_util::ProfileCanBeManaged(profile()));

  // Without a primary account the profile can never be managed.
  identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(enterprise_util::ProfileCanBeManaged(profile()));

  // Signing back in with the same account restores manageability since the
  // per-account bit is not cleared on sign-out.
  SignIn();
  EXPECT_TRUE(enterprise_util::ProfileCanBeManaged(profile()));
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    ManagedBrowserUtilsProfileCanBeManagedAndroidTest);
#endif  // BUILDFLAG(IS_ANDROID)

class AutoSelectCertificateTest : public testing::Test {
 protected:
  void SetUp() override {
    client_1_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1.pem");
    ASSERT_TRUE(client_1_);
    client_2_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_2.pem");
    ASSERT_TRUE(client_2_);

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    profile_ = profile_manager_->CreateTestingProfile("TestProfile");
  }

  void TearDown() override {
    HostContentSettingsMap* m =
        HostContentSettingsMapFactory::GetForProfile(profile());
    m->ClearSettingsForOneType(ContentSettingsType::AUTO_SELECT_CERTIFICATE);
  }

  net::ClientCertIdentityList GetDefaultClientCertList() const {
    return net::FakeClientCertIdentityListFromCertificateList(
        {client_1_, client_2_});
  }

  void SetPolicyValueInContentSettings(base::ListValue filters) {
    HostContentSettingsMap* m =
        HostContentSettingsMapFactory::GetForProfile(profile());

    base::DictValue root;
    root.Set("filters", std::move(filters));

    m->SetWebsiteSettingDefaultScope(
        GURL(kRequestingUrl), GURL(),
        ContentSettingsType::AUTO_SELECT_CERTIFICATE,
        base::Value(std::move(root)));
  }

  base::DictValue CreateFilterValue(const std::string& issuer,
                                    const std::string& subject) {
    EXPECT_FALSE(issuer.empty() && subject.empty());

    base::DictValue filter;
    if (!issuer.empty()) {
      filter.Set("ISSUER", base::DictValue().Set("CN", issuer));
    }

    if (!subject.empty()) {
      filter.Set("SUBJECT", base::DictValue().Set("CN", subject));
    }

    return filter;
  }

  TestingProfile* profile() { return profile_; }

  const scoped_refptr<net::X509Certificate>& client_1() { return client_1_; }
  const scoped_refptr<net::X509Certificate>& client_2() { return client_2_; }

 private:
  content::BrowserTaskEnvironment task_environment_;

  scoped_refptr<net::X509Certificate> client_1_;
  scoped_refptr<net::X509Certificate> client_2_;

  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

TEST_F(AutoSelectCertificateTest, NoPolicyAppliedReturnsNoMatch) {
  GURL requesting_url(kRequestingUrl);
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  net::ClientCertIdentityList matching_certs_list, nonmatching_certs_list;
  enterprise_util::AutoSelectCertificates(
      profile(), requesting_url, std::move(client_certs_list),
      &matching_certs_list, &nonmatching_certs_list);

  EXPECT_TRUE(matching_certs_list.empty());
  EXPECT_THAT(nonmatching_certs_list,
              UnorderedElementsAre(CertEq(client_1()), CertEq(client_2())));
}

TEST_F(AutoSelectCertificateTest,
       SingleIssuerFilterPolicySelectsFirstCertIfMatching) {
  GURL requesting_url(kRequestingUrl);
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  // client_1.pem has "B CA" as its issuer, so set up filters to select it
  base::ListValue filters;
  filters.Append(CreateFilterValue("B CA", ""));

  SetPolicyValueInContentSettings(std::move(filters));

  net::ClientCertIdentityList matching_certs_list, nonmatching_certs_list;
  enterprise_util::AutoSelectCertificates(
      profile(), requesting_url, std::move(client_certs_list),
      &matching_certs_list, &nonmatching_certs_list);

  EXPECT_THAT(matching_certs_list, UnorderedElementsAre(CertEq(client_1())));
  EXPECT_THAT(nonmatching_certs_list, UnorderedElementsAre(CertEq(client_2())));
}

TEST_F(AutoSelectCertificateTest,
       SingleIssuerFilterPolicySelectsOtherCertIfMatching) {
  GURL requesting_url(kRequestingUrl);
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  // client_2.pem has "E CA" as its issuer, so set up filters to select it
  base::ListValue filters;
  filters.Append(CreateFilterValue("E CA", ""));

  SetPolicyValueInContentSettings(std::move(filters));

  net::ClientCertIdentityList matching_certs_list, nonmatching_certs_list;

  enterprise_util::AutoSelectCertificates(
      profile(), requesting_url, std::move(client_certs_list),
      &matching_certs_list, &nonmatching_certs_list);

  EXPECT_THAT(matching_certs_list, UnorderedElementsAre(CertEq(client_2())));
  EXPECT_THAT(nonmatching_certs_list, UnorderedElementsAre(CertEq(client_1())));
}

TEST_F(AutoSelectCertificateTest,
       SingleSubjectFilterPolicySelectsFirstCertIfMatching) {
  GURL requesting_url(kRequestingUrl);
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  // client_1.pem has "Client Cert A" as its subject, so set up filters to
  // select it
  base::ListValue filters;
  filters.Append(CreateFilterValue("", "Client Cert A"));

  SetPolicyValueInContentSettings(std::move(filters));

  net::ClientCertIdentityList matching_certs_list, nonmatching_certs_list;

  enterprise_util::AutoSelectCertificates(
      profile(), requesting_url, std::move(client_certs_list),
      &matching_certs_list, &nonmatching_certs_list);

  EXPECT_THAT(matching_certs_list, UnorderedElementsAre(CertEq(client_1())));
  EXPECT_THAT(nonmatching_certs_list, UnorderedElementsAre(CertEq(client_2())));
}

TEST_F(AutoSelectCertificateTest,
       SingleSubjectFilterPolicySelectsOtherCertIfMatching) {
  GURL requesting_url(kRequestingUrl);
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  // client_2.pem has "Client Cert D" as its subject, so set up filters to
  // select it
  base::ListValue filters;
  filters.Append(CreateFilterValue("", "Client Cert D"));

  SetPolicyValueInContentSettings(std::move(filters));

  net::ClientCertIdentityList matching_certs_list, nonmatching_certs_list;

  enterprise_util::AutoSelectCertificates(
      profile(), requesting_url, std::move(client_certs_list),
      &matching_certs_list, &nonmatching_certs_list);

  EXPECT_THAT(matching_certs_list, UnorderedElementsAre(CertEq(client_2())));
  EXPECT_THAT(nonmatching_certs_list, UnorderedElementsAre(CertEq(client_1())));
}

TEST_F(AutoSelectCertificateTest, IssuerNotMatchingDoesntSelectCerts) {
  GURL requesting_url(kRequestingUrl);
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  base::ListValue filters;
  filters.Append(CreateFilterValue("Bad Issuer", "Client Cert D"));

  SetPolicyValueInContentSettings(std::move(filters));

  net::ClientCertIdentityList matching_certs_list, nonmatching_certs_list;

  enterprise_util::AutoSelectCertificates(
      profile(), requesting_url, std::move(client_certs_list),
      &matching_certs_list, &nonmatching_certs_list);

  EXPECT_TRUE(matching_certs_list.empty());
  EXPECT_THAT(nonmatching_certs_list,
              UnorderedElementsAre(CertEq(client_1()), CertEq(client_2())));
}

TEST_F(AutoSelectCertificateTest, SubjectNotMatchingDoesntSelectCerts) {
  GURL requesting_url(kRequestingUrl);
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  base::ListValue filters;
  filters.Append(CreateFilterValue("E CA", "Bad Subject"));

  SetPolicyValueInContentSettings(std::move(filters));

  net::ClientCertIdentityList matching_certs_list, nonmatching_certs_list;

  enterprise_util::AutoSelectCertificates(
      profile(), requesting_url, std::move(client_certs_list),
      &matching_certs_list, &nonmatching_certs_list);

  EXPECT_TRUE(matching_certs_list.empty());
  EXPECT_THAT(nonmatching_certs_list,
              UnorderedElementsAre(CertEq(client_1()), CertEq(client_2())));
}

TEST_F(AutoSelectCertificateTest, MatchingCertOnDifferentUrlDoesntSelectCerts) {
  GURL requesting_url("http://other.domain.example.com");
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  base::ListValue filters;
  filters.Append(CreateFilterValue("E CA", ""));

  SetPolicyValueInContentSettings(std::move(filters));

  net::ClientCertIdentityList matching_certs_list, nonmatching_certs_list;
  enterprise_util::AutoSelectCertificates(
      profile(), requesting_url, std::move(client_certs_list),
      &matching_certs_list, &nonmatching_certs_list);

  EXPECT_TRUE(matching_certs_list.empty());
  EXPECT_THAT(nonmatching_certs_list,
              UnorderedElementsAre(CertEq(client_1()), CertEq(client_2())));
}
