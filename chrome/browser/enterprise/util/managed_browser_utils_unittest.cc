// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/managed_browser_utils.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/host_port_pair.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/values.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
class ManagedBrowserUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_provider_ =
        std::make_unique<policy::MockConfigurationPolicyProvider>();
    mock_provider_->Init();
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(
        mock_provider_.get());
  }
  void TearDown() override {
    mock_provider_->Shutdown();
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(nullptr);
  }
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::MockConfigurationPolicyProvider> mock_provider_;
};

TEST_F(ManagedBrowserUtilsTest, HasMachineLevelPolicies) {
  TestingProfile profile;

  policy::PolicyMap map;
  map.Set("test-policy", policy::POLICY_LEVEL_MANDATORY,
          policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
          base::Value("hello"), nullptr);
  mock_provider_->UpdateChromePolicy(map);

  EXPECT_TRUE(enterprise_util::IsBrowserManaged(&profile));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

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

  void SetPolicyValueInContentSettings(base::Value::List filters) {
    HostContentSettingsMap* m =
        HostContentSettingsMapFactory::GetForProfile(profile());

    base::Value::Dict root;
    root.Set("filters", std::move(filters));

    m->SetWebsiteSettingDefaultScope(
        GURL(kRequestingUrl), GURL(),
        ContentSettingsType::AUTO_SELECT_CERTIFICATE,
        base::Value(std::move(root)));
  }

  base::Value::Dict CreateFilterValue(const std::string& issuer,
                                      const std::string& subject) {
    EXPECT_FALSE(issuer.empty() && subject.empty());

    base::Value::Dict filter;
    if (!issuer.empty()) {
      filter.Set("ISSUER", base::Value::Dict().Set("CN", issuer));
    }

    if (!subject.empty()) {
      filter.Set("SUBJECT", base::Value::Dict().Set("CN", subject));
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
  base::Value::List filters;
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
  base::Value::List filters;
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
  base::Value::List filters;
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
  base::Value::List filters;
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

  base::Value::List filters;
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

  base::Value::List filters;
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

  base::Value::List filters;
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
