// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/dns_over_https/templates_uri_resolver_impl.h"

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/device_attributes_fake.h"
#include "chrome/browser/net/secure_dns_config.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::dns_over_https {
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Key;
using ::testing::UnorderedElementsAre;

constexpr const char kGoogleDns[] = "https://dns.google/dns-query{?dns}";

constexpr char kTemplateIdentifiers[] =
    "https://dns.google.alternativeuri/"
    "${USER_EMAIL}-${USER_EMAIL_DOMAIN}-${USER_EMAIL_NAME}-${DEVICE_"
    "DIRECTORY_"
    "ID}-${DEVICE_ASSET_ID}-${DEVICE_SERIAL_NUMBER}-${DEVICE_ANNOTATED_"
    "LOCATION}/"
    "dns-query{?dns}";
constexpr char kDisplayTemplateIdentifiers[] =
    "https://dns.google.alternativeuri/"
    "${test-user@testdomain.com}-${testdomain.com}-${test-user}-${85729104-"
    "ef7a-5718d62e72ca}-${admin-provided-"
    "test-asset-ID}-${serial-number}-${admin-provided-test-location}/"
    "dns-query{?dns}";
constexpr char kDisplayTemplateIdentifiersUnaffiliated[] =
    "https://dns.google.alternativeuri/"
    "${test-user@testdomain.com}-${testdomain.com}-${test-user}-${DEVICE_"
    "DIRECTORY_ID}-${DEVICE_ASSET_ID}-${"
    "DEVICE_SERIAL_NUMBER}-${DEVICE_ANNOTATED_LOCATION}/"
    "dns-query{?dns}";
constexpr char kEffectiveTemplateIdentifiers[] =
    "https://dns.google.alternativeuri/"
    "EAE2DCB2164EB64B695BC555C4EC45D01C8F0DF73CCD3321E45E5B49F22A22DF-"
    "0641BDB5149AF8B202F8EC96D8C256774CDFE9456CB12663DDF5897AFD91BC78-"
    "F3BA0BDE2D6E8DBE626D0B9ECF7862B18256C4D1807621F9F01AF06A3F603137-"
    "542BD404A979AB36D342019A18FFE0691B7763C1D145F2D7119FC1DCBBB7B248-"
    "2601D4E337D68B6EE97B338482E45F7D3670E78075490A7F9916321AF7B4539F-"
    "D60336AF57006D9FD327FD96F4B44F9067A09FB0A4DD83FB67EC0B2C472B9734-"
    "F7E37C960A2F15DD63CE0F694F29A3A76E4BE2700918E01FA22F0692098C6B28/"
    "dns-query{?dns}";
constexpr char kEffectiveTemplateIdentifiersWithTestSalt[] =
    "https://dns.google.alternativeuri/"
    "C3C0177E97A4E05B1642A503F457AF68D40A846A797AC88648A42F3153FEA5FD-"
    "4F11690A960D3EEEBD383FDD5551318D992408FBE4FA8673BEFB04A35078387A-"
    "9B7A60CA80571F2BFB83C955416F091084C0BA5679B8FB24A4801060BA353663-"
    "F260F9A86E34A360934E4C6397993A1CECA6584920512610741E328D67DCE4D6-"
    "69D97D906733DFAD7B7AEE5B7632DDD65838EAACEF2C35682C022CE121FB7E67-"
    "F8F33F2554E3E2223391375D12FC6CC8744CE1EBD02F4E73941FEB66FFC3A4CE-"
    "752F92CB761746F2ACE4DB56D7CCD332E2737A2921B154E280E528532CDA3E1B/"
    "dns-query{?dns}";

constexpr char kTestDeviceDirectoryId[] = "85729104-ef7a-5718d62e72ca";
constexpr char kTestDeviceAssetId[] = "admin-provided-test-asset-ID";
constexpr char kTestDeviceAnnotatedLocation[] = "admin-provided-test-location";
constexpr char kTestSerialNumber[] = "serial-number";
}  // namespace

class TemplatesUriResolverImplTest : public testing::Test {
 public:
  TemplatesUriResolverImplTest() = default;

  TemplatesUriResolverImplTest(const TemplatesUriResolverImplTest&) = delete;
  TemplatesUriResolverImplTest& operator=(const TemplatesUriResolverImplTest&) =
      delete;

  void SetUp() override {
    pref_service_.registry()->RegisterStringPref(prefs::kDnsOverHttpsMode,
                                                 SecureDnsConfig::kModeOff);
    pref_service_.registry()->RegisterStringPref(prefs::kDnsOverHttpsTemplates,
                                                 "");
    pref_service_.registry()->RegisterStringPref(
        prefs::kDnsOverHttpsTemplatesWithIdentifiers, "");
    pref_service_.registry()->RegisterStringPref(prefs::kDnsOverHttpsSalt, "");

    fake_user_manager_ = new user_manager::FakeUserManager();
    fake_user_manager_->set_local_state(&pref_service_);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_));

    doh_template_uri_resolver_ = std::make_unique<TemplatesUriResolverImpl>();

    // Set up fake device attributes.
    std::unique_ptr<policy::FakeDeviceAttributes> device_attributes =
        std::make_unique<policy::FakeDeviceAttributes>();
    device_attributes->SetFakeDirectoryApiId(kTestDeviceDirectoryId);
    device_attributes->SetFakeDeviceAssetId(kTestDeviceAssetId);
    device_attributes->SetFakeDeviceAnnotatedLocation(
        kTestDeviceAnnotatedLocation);
    device_attributes->SetFakeDeviceSerialNumber(kTestSerialNumber);

    doh_template_uri_resolver_->SetDeviceAttributesForTesting(
        std::move(device_attributes));
  }

  void TearDown() override { scoped_user_manager_.reset(); }

  void SetupAffiliatedUser() {
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        "test-user@testdomain.com", "1234567890"));
    fake_user_manager_->AddUserWithAffiliation(account_id, true);
  }

  void SetupUnaffiliatedUser() {
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        "test-user@testdomain.com", "1234567890"));
    fake_user_manager_->AddUser(account_id);
  }

  PrefService* pref_service() { return &pref_service_; }
  user_manager::FakeUserManager* user_manager() { return fake_user_manager_; }

 protected:
  std::unique_ptr<TemplatesUriResolverImpl> doh_template_uri_resolver_;

 private:
  TestingPrefServiceSimple pref_service_;
  user_manager::FakeUserManager* fake_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  ScopedStubInstallAttributes test_install_attributes_{
      StubInstallAttributes::CreateCloudManaged("fake-domain", "fake-id")};
};

// Test that verifies the correct substitution of placeholders in the template
// uri.
TEST_F(TemplatesUriResolverImplTest, TemplatesWithIdentifiers) {
  SetupAffiliatedUser();
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeSecure));
  pref_service()->Set(prefs::kDnsOverHttpsSalt, base::Value("test-salt"));
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(kTemplateIdentifiers));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));
  doh_template_uri_resolver_->UpdateFromPrefs(pref_service());

  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            kDisplayTemplateIdentifiers);
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            kEffectiveTemplateIdentifiers);
  EXPECT_TRUE(doh_template_uri_resolver_->GetDohWithIdentifiersActive());

  // `prefs::kDnsOverHttpsTemplates` should apply when
  // `prefs::kDnsOverHttpsTemplatesWithIdentifiers` is cleared.
  pref_service()->ClearPref(prefs::kDnsOverHttpsTemplatesWithIdentifiers);
  doh_template_uri_resolver_->UpdateFromPrefs(pref_service());
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(), kGoogleDns);
  EXPECT_FALSE(doh_template_uri_resolver_->GetDohWithIdentifiersActive());
}

// Tests that only user indentifiers are replaced in
// `prefs::kDnsOverHttpsTemplatesWithIdentifiers` if the user is not affiliated.
TEST_F(TemplatesUriResolverImplTest, TemplatesWithIdentifiersUnaffiliated) {
  SetupUnaffiliatedUser();
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeSecure));
  pref_service()->Set(prefs::kDnsOverHttpsSalt, base::Value("test-salt"));
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(kTemplateIdentifiers));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));
  doh_template_uri_resolver_->UpdateFromPrefs(pref_service());

  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            kDisplayTemplateIdentifiersUnaffiliated);
  EXPECT_TRUE(doh_template_uri_resolver_->GetDohWithIdentifiersActive());
}

// Verifies that, when the pref sets a list of template URI separated by space,
// all template URIs are being resolved.
TEST_F(TemplatesUriResolverImplTest, MultipleTemplatesWithIdentifiers) {
  SetupAffiliatedUser();
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeSecure));
  pref_service()->Set(prefs::kDnsOverHttpsSalt, base::Value("test-salt"));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates, base::Value(kGoogleDns));
  std::string multiple_templates = base::StringPrintf(
      "%s %s %s", kTemplateIdentifiers, kGoogleDns, kTemplateIdentifiers);
  pref_service()->Set(prefs::kDnsOverHttpsTemplatesWithIdentifiers,
                      base::Value(multiple_templates));
  doh_template_uri_resolver_->UpdateFromPrefs(pref_service());

  std::string expected_multiple_templates =
      base::StringPrintf("%s %s %s", kEffectiveTemplateIdentifiers, kGoogleDns,
                         kEffectiveTemplateIdentifiers);
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            expected_multiple_templates);
  EXPECT_TRUE(doh_template_uri_resolver_->GetDohWithIdentifiersActive());
}

// Tests that reusing the existing policy DnsOverHttpsTemplates for testing
// template URI with identifiers works as intended.
TEST_F(TemplatesUriResolverImplTest, ReuseOldPolicyFeature) {
  SetupAffiliatedUser();
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      features::kDnsOverHttpsWithIdentifiersReuseOldPolicy);

  // Set the templates with identifiers in the existing pref.
  pref_service()->Set(prefs::kDnsOverHttpsMode,
                      base::Value(SecureDnsConfig::kModeSecure));
  pref_service()->Set(prefs::kDnsOverHttpsTemplates,
                      base::Value(kTemplateIdentifiers));
  // Extra precaution to ensure the new pref is not set.
  pref_service()->ClearPref(prefs::kDnsOverHttpsTemplatesWithIdentifiers);
  doh_template_uri_resolver_->UpdateFromPrefs(pref_service());

  // `features::kDnsOverHttpsWithIdentifiersReuseOldPolicy` disabled means the
  // identifiers are not replaced.
  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(), "");
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            kTemplateIdentifiers);
  EXPECT_FALSE(doh_template_uri_resolver_->GetDohWithIdentifiersActive());

  features.Reset();
  features.InitAndEnableFeature(
      features::kDnsOverHttpsWithIdentifiersReuseOldPolicy);
  doh_template_uri_resolver_->UpdateFromPrefs(pref_service());

  // `features::kDnsOverHttpsWithIdentifiersReuseOldPolicy` enabled means the
  // identifiers are replaced.
  EXPECT_EQ(doh_template_uri_resolver_->GetDisplayTemplates(),
            kDisplayTemplateIdentifiers);
  EXPECT_EQ(doh_template_uri_resolver_->GetEffectiveTemplates(),
            kEffectiveTemplateIdentifiersWithTestSalt);
  EXPECT_NE(kEffectiveTemplateIdentifiersWithTestSalt,
            kEffectiveTemplateIdentifiers);
  EXPECT_TRUE(doh_template_uri_resolver_->GetDohWithIdentifiersActive());
}

}  // namespace ash::dns_over_https
