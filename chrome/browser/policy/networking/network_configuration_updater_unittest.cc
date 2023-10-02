// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater.h"
#include "chromeos/ash/components/network/policy_certificate_provider.h"
#include "chromeos/components/onc/certificate_scope.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::Ne;
using testing::Return;
using testing::StrictMock;

namespace policy {

namespace {

class MockPolicyProvidedCertsObserver
    : public ash::PolicyCertificateProvider::Observer {
 public:
  MockPolicyProvidedCertsObserver() = default;

  MockPolicyProvidedCertsObserver(const MockPolicyProvidedCertsObserver&) =
      delete;
  MockPolicyProvidedCertsObserver& operator=(
      const MockPolicyProvidedCertsObserver&) = delete;

  MOCK_METHOD0(OnPolicyProvidedCertsChanged, void());
};

// Note: HexSSID 737369642D6E6F6E65 maps to "ssid-none".
// HexSSID 7465737431323334 maps to "test1234"
const char kFakeONC[] = R"(
    { "NetworkConfigurations": [
        { "GUID": "{485d6076-dd44-6b6d-69787465725f5040}",
          "Type": "WiFi",
          "Name": "My WiFi Network",
          "WiFi": {
            "HexSSID": "737369642D6E6F6E65",
            "HiddenSSID": false,
            "Security": "None" }
        },
        { "GUID": "{guid-for-wifi-with-device-exp}",
          "Type": "WiFi",
          "Name": "My WiFi with device placeholder expansions",
          "WiFi": {
            "EAP": {
              "Outer": "EAP-TLS",
              "Identity": "${DEVICE_SERIAL_NUMBER}-${DEVICE_ASSET_ID}"
            },
            "HexSSID": "7465737431323334",
            "HiddenSSID": false,
            "Security": "WPA-EAP",
            "SSID": "test1234",
          }
        }
      ],
      "GlobalNetworkConfiguration": {
        "AllowOnlyPolicyNetworksToAutoconnect": true,
      },
      "Certificates": [
        { "GUID": "{f998f760-272b-6939-4c2beffe428697ac}",
          "PKCS12": "YWJj",
          "Type": "Client" },
        { "GUID": "{d443ad0d-ea16-4301-9089-588115e2f5c4}",
          "TrustBits": [
             "Web"
          ],
          "Type": "Authority",
          "X509": "-----BEGIN CERTIFICATE-----\n
    MIIC8zCCAdugAwIBAgIJALF9qhLor0+aMA0GCSqGSIb3DQEBBQUAMBcxFTATBgNV\n
    BAMMDFRlc3QgUm9vdCBDQTAeFw0xNDA4MTQwMzA1MjlaFw0yNDA4MTEwMzA1Mjla\n
    MBcxFTATBgNVBAMMDFRlc3QgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEP\n
    ADCCAQoCggEBALZJQeNCAVGofzx6cdP7zZE1F4QajvY2x9FwHfqG8267dm/oMi43\n
    /TiSPWjkin1CMxRGG9wE9pFuVEDECgn97C1i4l7huiycwbFgTNrH+CJcgiBlQh5W\n
    d3VP65AsSupXDiKNbJWsEerM1+72cA0J3aY1YV3Jdm2w8h6/MIbYd1I2lZcO0UbF\n
    7YE9G7DyYZU8wUA4719dumGf7yucn4WJdHBj1XboNX7OAeHzERGQHA31/Y3OEGyt\n
    fFUaIW/XLfR4FeovOL2RnjwdB0b1Q8GCi68SU2UZimlpZgay2gv6KgChKhWESfEB\n
    v5swBtAVoB+dUZFH4VNf717swmF5whSfxOMCAwEAAaNCMEAwDwYDVR0TAQH/BAUw\n
    AwEB/zAdBgNVHQ4EFgQUvPcw0TzA8nn675/JbFyT84poq4MwDgYDVR0PAQH/BAQD\n
    AgEGMA0GCSqGSIb3DQEBBQUAA4IBAQBXByn7f+j/sObYWGrDkKE4HLTzaLHs6Ikj\n
    JNeo8iHDYOSkSVwAv9/HgniAKxj3rd3QYl6nsMzwqrTOcBJZZWd2BQAYmv/EKhfj\n
    8VXYvlxe68rLU4cQ1QkyNqdeQfRT2n5WYNJ+TpqlCF9ddennMMsi6e8ZSYOlI6H4\n
    YEzlNtU5eBjxXr/OqgtTgSx4qQpr2xMQIRR/G3A9iRpAigYsXVAZYvnHRYnyPWYF\n
    PX11W1UegEJyoZp8bQp09u6mIWw6mPt3gl/ya1bm3ZuOUPDGrv3qpgUHqSYGVrOy\n
    2bI3oCE+eQYfuVG+9LFJTZC1M+UOx15bQMVqBNFDepRqpE9h/ILg\n
    -----END CERTIFICATE-----" },
        { "GUID": "{dac8e282-8ff3-4bb9-a20f-b5ef22b2f83b}",
          "Type": "Authority",
          "X509": "-----BEGIN CERTIFICATE-----\n
    MIIDvzCCAqegAwIBAgIBAzANBgkqhkiG9w0BAQsFADBjMQswCQYDVQQGEwJVUzET\n
    MBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNTW91bnRhaW4gVmlldzEQMA4G\n
    A1UECgwHVGVzdCBDQTEVMBMGA1UEAwwMVGVzdCBSb290IENBMB4XDTE3MDYwNTE3\n
    MTA0NloXDTI3MDYwMzE3MTA0NlowYDELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNh\n
    bGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxEDAOBgNVBAoMB1Rlc3Qg\n
    Q0ExEjAQBgNVBAMMCTEyNy4wLjAuMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCC\n
    AQoCggEBALS/0pcz5RNbd2W9cxp1KJtHWea3MOhGM21YW9ofCv/k5C3yHfiJ6GQu\n
    9sPN16OO1/fN59gOEMPnVtL85ebTTuL/gk0YY4ewo97a7wo3e6y1t0PO8gc53xTp\n
    w6RBPn5oRzSbe2HEGOYTzrO0puC6A+7k6+eq9G2+l1uqBpdQAdB4uNaSsOTiuUOI\n
    ta4UZH1ScNQFHAkl1eJPyaiC20Exw75EbwvU/b/B7tlivzuPtQDI0d9dShOtceRL\n
    X9HZckyD2JNAv2zNL2YOBNa5QygkySX9WXD+PfKpCk7Cm8TenldeXRYl5ni2REkp\n
    nfa/dPuF1g3xZVjyK9aPEEnIAC2I4i0CAwEAAaOBgDB+MAwGA1UdEwEB/wQCMAAw\n
    HQYDVR0OBBYEFODc4C8HiHQ6n9Mwo3GK+dal5aZTMB8GA1UdIwQYMBaAFJsmC4qY\n
    qbsduR8c4xpAM+2OF4irMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjAP\n
    BgNVHREECDAGhwR/AAABMA0GCSqGSIb3DQEBCwUAA4IBAQB6FEQuUDRcC5jkX3aZ\n
    uuTeZEqMVL7JXgvgFqzXsPb8zIdmxr/tEDfwXx2qDf2Dpxts7Fq4vqUwimK4qV3K\n
    7heLnWV2+FBvV1eeSfZ7AQj+SURkdlyo42r41+t13QUf+Z0ftR9266LSWLKrukeI\n
    Mxk73hOkm/u8enhTd00dy/FN9dOFBFHseVMspWNxIkdRILgOmiyfQNRgxNYdOf0e\n
    EfELR8Hn6WjZ8wAbvO4p7RTrzu1c/RZ0M+NLkID56Brbl70GC2h5681LPwAOaZ7/\n
    mWQ5kekSyJjmLfF12b+h9RVAt5MrXZgk2vNujssgGf4nbWh4KZyQ6qrs778ZdDLm\n
    yfUn\n
    -----END CERTIFICATE-----" }
      ],
      "Type": "UnencryptedConfiguration"
    })";

const char kFakeONCWithExtensionScopedCert[] = R"(
    { "Certificates": [
        { "GUID": "{extension-scoped-certificate}",
          "TrustBits": [
             "Web"
          ],
          "Scope": {
            "Type": "Extension",
            "Id": "ngjobkbdodapjbbncmagbccommkggmnj"
          },
          "Type": "Authority",
          "X509": "-----BEGIN CERTIFICATE-----\n
    MIIC8zCCAdugAwIBAgIJALF9qhLor0+aMA0GCSqGSIb3DQEBBQUAMBcxFTATBgNV\n
    BAMMDFRlc3QgUm9vdCBDQTAeFw0xNDA4MTQwMzA1MjlaFw0yNDA4MTEwMzA1Mjla\n
    MBcxFTATBgNVBAMMDFRlc3QgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEP\n
    ADCCAQoCggEBALZJQeNCAVGofzx6cdP7zZE1F4QajvY2x9FwHfqG8267dm/oMi43\n
    /TiSPWjkin1CMxRGG9wE9pFuVEDECgn97C1i4l7huiycwbFgTNrH+CJcgiBlQh5W\n
    d3VP65AsSupXDiKNbJWsEerM1+72cA0J3aY1YV3Jdm2w8h6/MIbYd1I2lZcO0UbF\n
    7YE9G7DyYZU8wUA4719dumGf7yucn4WJdHBj1XboNX7OAeHzERGQHA31/Y3OEGyt\n
    fFUaIW/XLfR4FeovOL2RnjwdB0b1Q8GCi68SU2UZimlpZgay2gv6KgChKhWESfEB\n
    v5swBtAVoB+dUZFH4VNf717swmF5whSfxOMCAwEAAaNCMEAwDwYDVR0TAQH/BAUw\n
    AwEB/zAdBgNVHQ4EFgQUvPcw0TzA8nn675/JbFyT84poq4MwDgYDVR0PAQH/BAQD\n
    AgEGMA0GCSqGSIb3DQEBBQUAA4IBAQBXByn7f+j/sObYWGrDkKE4HLTzaLHs6Ikj\n
    JNeo8iHDYOSkSVwAv9/HgniAKxj3rd3QYl6nsMzwqrTOcBJZZWd2BQAYmv/EKhfj\n
    8VXYvlxe68rLU4cQ1QkyNqdeQfRT2n5WYNJ+TpqlCF9ddennMMsi6e8ZSYOlI6H4\n
    YEzlNtU5eBjxXr/OqgtTgSx4qQpr2xMQIRR/G3A9iRpAigYsXVAZYvnHRYnyPWYF\n
    PX11W1UegEJyoZp8bQp09u6mIWw6mPt3gl/ya1bm3ZuOUPDGrv3qpgUHqSYGVrOy\n
    2bI3oCE+eQYfuVG+9LFJTZC1M+UOx15bQMVqBNFDepRqpE9h/ILg\n
    -----END CERTIFICATE-----" },
      ],
      "Type": "UnencryptedConfiguration"
    })";

const char kExtensionIdWithScopedCert[] = "ngjobkbdodapjbbncmagbccommkggmnj";

}  // namespace

class NetworkConfigurationUpdaterTest : public testing::Test {
 protected:
  NetworkConfigurationUpdaterTest() {}

  void SetUp() override {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(provider_, IsFirstPolicyLoadComplete(_))
        .WillRepeatedly(Return(false));
    provider_.Init();
    PolicyServiceImpl::Providers providers;
    providers.push_back(&provider_);
    policy_service_ = std::make_unique<PolicyServiceImpl>(std::move(providers));
  }

  void TearDown() override {
    network_configuration_updater_.reset();
    provider_.Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  void MarkPolicyProviderInitialized() {
    Mock::VerifyAndClearExpectations(&provider_);
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(provider_, IsFirstPolicyLoadComplete(_))
        .WillRepeatedly(Return(true));
    provider_.SetAutoRefresh();
    provider_.RefreshPolicies(PolicyFetchReason::kTest);
    base::RunLoop().RunUntilIdle();
  }

  void UpdateProviderPolicy(const PolicyMap& policy) {
    provider_.UpdateChromePolicy(policy);
    base::RunLoop().RunUntilIdle();
  }

  NetworkConfigurationUpdater*
  CreateNetworkConfigurationUpdaterForUserPolicy() {
    network_configuration_updater_ =
        UserNetworkConfigurationUpdater::CreateForUserPolicy(
            policy_service_.get());
    return network_configuration_updater_.get();
  }

  content::BrowserTaskEnvironment task_environment_;

  StrictMock<MockConfigurationPolicyProvider> provider_;
  std::unique_ptr<PolicyServiceImpl> policy_service_;

  std::unique_ptr<NetworkConfigurationUpdater> network_configuration_updater_;

 private:
  base::Value fake_network_configs_;
};

TEST_F(NetworkConfigurationUpdaterTest, CaPolicyIsValidatedAndRepaired) {
  std::string onc_policy = chromeos::onc::test_utils::ReadTestData(
      "toplevel_ca_partially_invalid.onc");
  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(onc_policy),
             nullptr);
  UpdateProviderPolicy(policy);

  NetworkConfigurationUpdater* updater =
      CreateNetworkConfigurationUpdaterForUserPolicy();
  MarkPolicyProviderInitialized();

  const auto kDefaultScope = chromeos::onc::CertificateScope::Default();
  EXPECT_EQ(0u, updater->GetWebTrustedCertificates(kDefaultScope).size());
  EXPECT_EQ(1u, updater->GetCertificatesWithoutWebTrust(kDefaultScope).size());
  EXPECT_EQ(
      1u, updater->GetAllServerAndAuthorityCertificates(kDefaultScope).size());
}

TEST_F(NetworkConfigurationUpdaterTest,
       WebTrustedCertificatesFromPolicyInitially) {
  NetworkConfigurationUpdater* updater =
      CreateNetworkConfigurationUpdaterForUserPolicy();

  MockPolicyProvidedCertsObserver observer;
  EXPECT_CALL(observer, OnPolicyProvidedCertsChanged());
  updater->AddPolicyProvidedCertsObserver(&observer);

  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(kFakeONC),
             nullptr);
  UpdateProviderPolicy(policy);
  MarkPolicyProviderInitialized();
  base::RunLoop().RunUntilIdle();

  // Certificates with the "Web" trust flag set will be returned.
  const auto kDefaultScope = chromeos::onc::CertificateScope::Default();
  EXPECT_EQ(1u, updater->GetWebTrustedCertificates(kDefaultScope).size());
  EXPECT_EQ(1u, updater->GetCertificatesWithoutWebTrust(kDefaultScope).size());
  EXPECT_EQ(
      2u, updater->GetAllServerAndAuthorityCertificates(kDefaultScope).size());
  EXPECT_EQ(0u, updater
                    ->GetAllServerAndAuthorityCertificates(
                        chromeos::onc::CertificateScope::ForExtension(
                            kExtensionIdWithScopedCert))
                    .size());

  updater->RemovePolicyProvidedCertsObserver(&observer);
}

TEST_F(NetworkConfigurationUpdaterTest,
       WebTrustedCertificatesFromPolicyOnUpdate) {
  // Start with an empty certificate list.
  NetworkConfigurationUpdater* updater =
      CreateNetworkConfigurationUpdaterForUserPolicy();
  MockPolicyProvidedCertsObserver observer;
  EXPECT_CALL(observer, OnPolicyProvidedCertsChanged()).Times(0);
  updater->AddPolicyProvidedCertsObserver(&observer);

  MarkPolicyProviderInitialized();
  base::RunLoop().RunUntilIdle();

  // Verify that the returned certificate list is empty.
  const auto kDefaultScope = chromeos::onc::CertificateScope::Default();
  EXPECT_TRUE(updater->GetWebTrustedCertificates(kDefaultScope).empty());
  EXPECT_TRUE(updater->GetCertificatesWithoutWebTrust(kDefaultScope).empty());
  EXPECT_TRUE(
      updater->GetAllServerAndAuthorityCertificates(kDefaultScope).empty());

  // No call has been made to the policy-provided certificates observer.
  Mock::VerifyAndClearExpectations(&observer);
  EXPECT_CALL(observer, OnPolicyProvidedCertsChanged());

  // Change to ONC policy with web trust certs.
  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(kFakeONC),
             nullptr);
  UpdateProviderPolicy(policy);
  base::RunLoop().RunUntilIdle();

  // Certificates with the "Web" trust flag set will be returned and forwarded
  // to observers.
  EXPECT_EQ(1u, updater->GetWebTrustedCertificates(kDefaultScope).size());
  EXPECT_EQ(1u, updater->GetCertificatesWithoutWebTrust(kDefaultScope).size());
  EXPECT_EQ(
      2u, updater->GetAllServerAndAuthorityCertificates(kDefaultScope).size());
  EXPECT_EQ(0u, updater
                    ->GetAllServerAndAuthorityCertificates(
                        chromeos::onc::CertificateScope::ForExtension(
                            kExtensionIdWithScopedCert))
                    .size());

  updater->RemovePolicyProvidedCertsObserver(&observer);
}

TEST_F(NetworkConfigurationUpdaterTest, ExtensionScopedWebTrustedCertificate) {
  NetworkConfigurationUpdater* updater =
      CreateNetworkConfigurationUpdaterForUserPolicy();

  MockPolicyProvidedCertsObserver observer;
  EXPECT_CALL(observer, OnPolicyProvidedCertsChanged());
  updater->AddPolicyProvidedCertsObserver(&observer);

  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             base::Value(kFakeONCWithExtensionScopedCert), nullptr);
  UpdateProviderPolicy(policy);
  MarkPolicyProviderInitialized();
  base::RunLoop().RunUntilIdle();

  // Certificates with the "Web" trust flag set will be returned.
  const auto kDefaultScope = chromeos::onc::CertificateScope::Default();
  EXPECT_EQ(0u, updater->GetWebTrustedCertificates(kDefaultScope).size());
  EXPECT_EQ(0u, updater->GetCertificatesWithoutWebTrust(kDefaultScope).size());
  EXPECT_EQ(
      0u, updater->GetAllServerAndAuthorityCertificates(kDefaultScope).size());
  EXPECT_EQ(1u, updater
                    ->GetAllServerAndAuthorityCertificates(
                        chromeos::onc::CertificateScope::ForExtension(
                            kExtensionIdWithScopedCert))
                    .size());
  EXPECT_EQ(1u, updater
                    ->GetWebTrustedCertificates(
                        chromeos::onc::CertificateScope::ForExtension(
                            kExtensionIdWithScopedCert))
                    .size());

  updater->RemovePolicyProvidedCertsObserver(&observer);
}

}  // namespace policy
