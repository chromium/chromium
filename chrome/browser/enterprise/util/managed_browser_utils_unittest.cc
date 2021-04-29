// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/managed_browser_utils.h"

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/values.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

namespace {
const char kRequestingUrl[] = "https://www.example.com";
}

TEST(ManagedBrowserUtils, NoPolicies) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  EXPECT_FALSE(chrome::enterprise_util::HasBrowserPoliciesApplied(&profile));
}

TEST(ManagedBrowserUtils, HasManagedConnector) {
  content::BrowserTaskEnvironment task_environment;
  TestingProfile::Builder builder;
  builder.OverridePolicyConnectorIsManagedForTesting(true);

  std::unique_ptr<TestingProfile> profile = builder.Build();
  EXPECT_TRUE(
      chrome::enterprise_util::HasBrowserPoliciesApplied(profile.get()));
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

  EXPECT_TRUE(chrome::enterprise_util::HasBrowserPoliciesApplied(&profile));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class AutoSelectCertificateTest : public testing::Test {
 protected:
  void SetUp() override {
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

  net::ClientCertIdentityList GetDefaultClientCertList() {
    EXPECT_EQ(0UL, client_certs_.size());

    client_certs_.push_back(
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1.pem"));
    client_certs_.push_back(
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_2.pem"));

    return net::FakeClientCertIdentityListFromCertificateList(client_certs_);
  }

  void SetPolicyValueInContentSettings(
      const std::vector<base::Value>& filters) {
    HostContentSettingsMap* m =
        HostContentSettingsMapFactory::GetForProfile(profile());

    std::unique_ptr<base::DictionaryValue> root =
        std::make_unique<base::DictionaryValue>();
    root->SetKey("filters", base::Value(std::move(filters)));

    m->SetWebsiteSettingDefaultScope(
        GURL(kRequestingUrl), GURL(),
        ContentSettingsType::AUTO_SELECT_CERTIFICATE, std::move(root));
  }

  base::Value CreateFilterValue(const std::string& issuer,
                                const std::string& subject) {
    EXPECT_FALSE(issuer.empty() && subject.empty());

    base::Value filter(base::Value::Type::DICTIONARY);
    if (!issuer.empty()) {
      base::Value issuer_value(base::Value::Type::DICTIONARY);
      issuer_value.SetStringKey("CN", issuer);
      filter.SetKey("ISSUER", std::move(issuer_value));
    }

    if (!subject.empty()) {
      base::Value subject_value(base::Value::Type::DICTIONARY);
      subject_value.SetStringKey("CN", subject);
      filter.SetKey("SUBJECT", std::move(subject_value));
    }

    return filter;
  }

  TestingProfile* profile() { return profile_; }

  std::vector<scoped_refptr<net::X509Certificate>>& client_certs() {
    return client_certs_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile* profile_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

  std::vector<scoped_refptr<net::X509Certificate>> client_certs_;
};

TEST_F(AutoSelectCertificateTest, NoPolicyAppliedReturnsNull) {
  GURL requesting_url(kRequestingUrl);
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  std::unique_ptr<net::ClientCertIdentity> cert =
      chrome::enterprise_util::AutoSelectCertificate(profile(), requesting_url,
                                                     client_certs_list);

  EXPECT_EQ(nullptr, cert);
}

TEST_F(AutoSelectCertificateTest,
       SingleIssuerFilterPolicySelectsFirstCertIfMatching) {
  GURL requesting_url(kRequestingUrl);
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  // client_1.pem has "B CA" as its issuer, so set up filters to select it
  std::vector<base::Value> filters;
  filters.push_back(CreateFilterValue("B CA", ""));

  SetPolicyValueInContentSettings(filters);

  std::unique_ptr<net::ClientCertIdentity> cert =
      chrome::enterprise_util::AutoSelectCertificate(profile(), requesting_url,
                                                     client_certs_list);
  EXPECT_NE(nullptr, cert);

  EXPECT_TRUE(
      cert->certificate()->EqualsIncludingChain(client_certs()[0].get()));
}

TEST_F(AutoSelectCertificateTest,
       SingleIssuerFilterPolicySelectsOtherCertIfMatching) {
  GURL requesting_url(kRequestingUrl);
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  // client_2.pem has "E CA" as its issuer, so set up filters to select it
  std::vector<base::Value> filters;
  filters.push_back(CreateFilterValue("E CA", ""));

  SetPolicyValueInContentSettings(filters);

  std::unique_ptr<net::ClientCertIdentity> cert =
      chrome::enterprise_util::AutoSelectCertificate(profile(), requesting_url,
                                                     client_certs_list);
  EXPECT_NE(nullptr, cert);

  EXPECT_TRUE(
      cert->certificate()->EqualsIncludingChain(client_certs()[1].get()));
}

TEST_F(AutoSelectCertificateTest,
       SingleSubjectFilterPolicySelectsFirstCertIfMatching) {
  GURL requesting_url(kRequestingUrl);
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  // client_1.pem has "Client Cert A" as its subject, so set up filters to
  // select it
  std::vector<base::Value> filters;
  filters.push_back(CreateFilterValue("", "Client Cert A"));

  SetPolicyValueInContentSettings(filters);

  std::unique_ptr<net::ClientCertIdentity> cert =
      chrome::enterprise_util::AutoSelectCertificate(profile(), requesting_url,
                                                     client_certs_list);
  EXPECT_NE(nullptr, cert);

  EXPECT_TRUE(
      cert->certificate()->EqualsIncludingChain(client_certs()[0].get()));
}

TEST_F(AutoSelectCertificateTest,
       SingleSubjectFilterPolicySelectsOtherCertIfMatching) {
  GURL requesting_url(kRequestingUrl);
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  // client_2.pem has "Client Cert D" as its subject, so set up filters to
  // select it
  std::vector<base::Value> filters;
  filters.push_back(CreateFilterValue("", "Client Cert D"));

  SetPolicyValueInContentSettings(filters);

  std::unique_ptr<net::ClientCertIdentity> cert =
      chrome::enterprise_util::AutoSelectCertificate(profile(), requesting_url,
                                                     client_certs_list);
  EXPECT_NE(nullptr, cert);

  EXPECT_TRUE(
      cert->certificate()->EqualsIncludingChain(client_certs()[1].get()));
}

TEST_F(AutoSelectCertificateTest, IssuerNotMatchingDoesntSelectCerts) {
  GURL requesting_url(kRequestingUrl);
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  std::vector<base::Value> filters;
  filters.push_back(CreateFilterValue("Bad Issuer", "Client Cert D"));

  SetPolicyValueInContentSettings(filters);

  std::unique_ptr<net::ClientCertIdentity> cert =
      chrome::enterprise_util::AutoSelectCertificate(profile(), requesting_url,
                                                     client_certs_list);
  EXPECT_EQ(nullptr, cert);
}

TEST_F(AutoSelectCertificateTest, SubjectNotMatchingDoesntSelectCerts) {
  GURL requesting_url(kRequestingUrl);
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  std::vector<base::Value> filters;
  filters.push_back(CreateFilterValue("E CA", "Bad Subject"));

  SetPolicyValueInContentSettings(filters);

  std::unique_ptr<net::ClientCertIdentity> cert =
      chrome::enterprise_util::AutoSelectCertificate(profile(), requesting_url,
                                                     client_certs_list);
  EXPECT_EQ(nullptr, cert);
}

TEST_F(AutoSelectCertificateTest, MatchingCertOnDifferentUrlDoesntSelectCerts) {
  GURL requesting_url("http://other.domain.example.com");
  net::ClientCertIdentityList client_certs_list = GetDefaultClientCertList();

  std::vector<base::Value> filters;
  filters.push_back(CreateFilterValue("E CA", ""));

  SetPolicyValueInContentSettings(filters);

  std::unique_ptr<net::ClientCertIdentity> cert =
      chrome::enterprise_util::AutoSelectCertificate(profile(), requesting_url,
                                                     client_certs_list);
  EXPECT_EQ(nullptr, cert);
}
