// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/device_network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/network/fake_network_device_handler.h"
#include "chromeos/network/mock_managed_network_configuration_handler.h"
#include "chromeos/network/onc/certificate_scope.h"
#include "chromeos/network/onc/onc_certificate_importer.h"
#include "chromeos/network/onc/onc_parsed_certificates.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/policy_certificate_provider.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "components/account_id/account_id.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::AnyNumber;
using testing::AtLeast;
using testing::Mock;
using testing::Ne;
using testing::Return;
using testing::StrictMock;
using testing::_;

namespace policy {

namespace {

const char kFakeUserEmail[] = "fake email";
const char kFakeUsernameHash[] = "fake hash";
const char kFakeSerialNumber[] = "FakeSerial";
const char kFakeAssetId[] = "FakeAssetId";

class FakeUser : public user_manager::User {
 public:
  FakeUser() : User(AccountId::FromUserEmail(kFakeUserEmail)) {
    set_display_email(kFakeUserEmail);
    set_username_hash(kFakeUsernameHash);
  }
  ~FakeUser() override {}

  // User overrides
  user_manager::UserType GetType() const override {
    return user_manager::USER_TYPE_REGULAR;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeUser);
};

class MockPolicyProvidedCertsObserver
    : public chromeos::PolicyCertificateProvider::Observer {
 public:
  MockPolicyProvidedCertsObserver() = default;

  MOCK_METHOD0(OnPolicyProvidedCertsChanged, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPolicyProvidedCertsObserver);
};

class FakeNetworkDeviceHandler : public chromeos::FakeNetworkDeviceHandler {
 public:
  FakeNetworkDeviceHandler()
      : allow_roaming_(false), mac_addr_randomization_(false) {}

  void SetCellularAllowRoaming(bool allow_roaming) override {
    allow_roaming_ = allow_roaming;
  }

  void SetMACAddressRandomizationEnabled(bool enabled) override {
    mac_addr_randomization_ = enabled;
  }

  bool allow_roaming_;
  bool mac_addr_randomization_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeNetworkDeviceHandler);
};

class FakeCertificateImporter : public chromeos::onc::CertificateImporter {
 public:
  using OncParsedCertificates = chromeos::onc::OncParsedCertificates;

  FakeCertificateImporter() : call_count_(0) {}
  ~FakeCertificateImporter() override {}

  void SetExpectedONCClientCertificates(
      const std::vector<OncParsedCertificates::ClientCertificate>&
          expected_client_certificates) {
    expected_client_certificates_ = expected_client_certificates;
  }

  unsigned int GetAndResetImportCount() {
    unsigned int count = call_count_;
    call_count_ = 0;
    return count;
  }

  void ImportAllCertificatesUserInitiated(
      const std::vector<OncParsedCertificates::ServerOrAuthorityCertificate>&
          server_or_authority_certificates,
      const std::vector<OncParsedCertificates::ClientCertificate>&
          client_certificates,
      DoneCallback done_callback) override {
    // As policy-provided server and authority certificates are not permanently
    // imported, only ImportClientCertificaates should be called.
    // ImportAllCertificatesUserInitiated should never be called from
    // UserNetworkConfigurationUpdater.
    NOTREACHED();
  }

  void ImportClientCertificates(
      const std::vector<OncParsedCertificates::ClientCertificate>&
          client_certificates,
      DoneCallback done_callback) override {
    EXPECT_EQ(expected_client_certificates_, client_certificates);

    ++call_count_;
    std::move(done_callback).Run(true);
  }

 private:
  std::vector<OncParsedCertificates::ClientCertificate>
      expected_client_certificates_;
  net::ScopedCERTCertificateList onc_trusted_certificates_;
  unsigned int call_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeCertificateImporter);
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

std::string ValueToString(const base::Value& value) {
  std::stringstream str;
  str << value;
  return str.str();
}

// Selects only the client certificate at |client_certificate_index| from the
// certificates contained in |toplevel_onc|. Appends the selected certificate
// into |out_parsed_client_certificates|.
void SelectSingleClientCertificateFromOnc(
    base::Value* toplevel_onc,
    size_t client_certificate_index,
    std::vector<chromeos::onc::OncParsedCertificates::ClientCertificate>*
        out_parsed_client_certificates) {
  base::ListValue* certs = nullptr;
  toplevel_onc->FindKey(onc::toplevel_config::kCertificates)->GetAsList(&certs);
  ASSERT_TRUE(certs);
  ASSERT_TRUE(certs->GetList().size() > client_certificate_index);

  base::ListValue selected_certs;
  selected_certs.Append(certs->GetList()[client_certificate_index].Clone());

  chromeos::onc::OncParsedCertificates parsed_selected_certs(selected_certs);
  ASSERT_FALSE(parsed_selected_certs.has_error());
  ASSERT_EQ(1u, parsed_selected_certs.client_certificates().size());
  out_parsed_client_certificates->push_back(
      parsed_selected_certs.client_certificates().front());
}

// Matcher to match base::Value.
MATCHER_P(IsEqualTo,
          value,
          std::string(negation ? "isn't" : "is") + " equal to " +
              ValueToString(*value)) {
  return value->Equals(&arg);
}

MATCHER(IsEmpty, std::string(negation ? "isn't" : "is") + " empty.") {
  return arg.empty();
}

ACTION_P(SetCertificateList, list) {
  if (arg2)
    *arg2 = list;
  return true;
}

}  // namespace

class NetworkConfigurationUpdaterTest : public testing::Test {
 protected:
  NetworkConfigurationUpdaterTest() : certificate_importer_(NULL) {}

  void SetUp() override {
    fake_statistics_provider_.SetMachineStatistic(
        chromeos::system::kSerialNumberKeyForTest, kFakeSerialNumber);

    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(false));
    provider_.Init();
    PolicyServiceImpl::Providers providers;
    providers.push_back(&provider_);
    policy_service_ = std::make_unique<PolicyServiceImpl>(std::move(providers));

    std::unique_ptr<base::Value> fake_toplevel_onc =
        chromeos::onc::ReadDictionaryFromJson(kFakeONC);

    base::DictionaryValue* global_config = nullptr;
    fake_toplevel_onc
        ->FindKey(onc::toplevel_config::kGlobalNetworkConfiguration)
        ->GetAsDictionary(&global_config);
    fake_global_network_config_.MergeDictionary(global_config);

    base::ListValue* certs = nullptr;
    fake_toplevel_onc->FindKey(onc::toplevel_config::kCertificates)
        ->GetAsList(&certs);
    fake_certificates_ =
        std::make_unique<chromeos::onc::OncParsedCertificates>(*certs);

    certificate_importer_ = new FakeCertificateImporter;
    client_certificate_importer_owned_.reset(certificate_importer_);
  }

  base::Value* GetExpectedFakeNetworkConfigs(::onc::ONCSource source) {
    std::unique_ptr<base::Value> fake_toplevel_onc =
        chromeos::onc::ReadDictionaryFromJson(kFakeONC);
    fake_network_configs_ =
        fake_toplevel_onc->FindKey(onc::toplevel_config::kNetworkConfigurations)
            ->Clone();
    if (source == ::onc::ONC_SOURCE_DEVICE_POLICY) {
      std::string expected_identity =
          std::string(kFakeSerialNumber) + "-" + std::string(kFakeAssetId);
      SetExpectedValueInNetworkConfig(
          &fake_network_configs_, "{guid-for-wifi-with-device-exp}",
          {"WiFi", "EAP", "Identity"}, base::Value(expected_identity));
    }
    return &fake_network_configs_;
  }

  base::Value* GetExpectedFakeGlobalNetworkConfig() {
    return &fake_global_network_config_;
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
    provider_.SetAutoRefresh();
    provider_.RefreshPolicies();
    base::RunLoop().RunUntilIdle();
  }

  void UpdateProviderPolicy(const PolicyMap& policy) {
    provider_.UpdateChromePolicy(policy);
    base::RunLoop().RunUntilIdle();
  }

  UserNetworkConfigurationUpdater*
  CreateNetworkConfigurationUpdaterForUserPolicy(
      bool set_client_cert_importer) {
    UserNetworkConfigurationUpdater* updater =
        UserNetworkConfigurationUpdater::CreateForUserPolicy(
            &profile_,
            fake_user_,
            policy_service_.get(),
            &network_config_handler_).release();
    if (set_client_cert_importer) {
      EXPECT_TRUE(client_certificate_importer_owned_);
      updater->SetClientCertificateImporterForTest(
          std::move(client_certificate_importer_owned_));
    }
    network_configuration_updater_.reset(updater);
    return updater;
  }

  NetworkConfigurationUpdater*
  CreateNetworkConfigurationUpdaterForDevicePolicy() {
    auto testing_device_asset_id_getter =
        base::BindRepeating([] { return std::string(kFakeAssetId); });
    network_configuration_updater_ =
        DeviceNetworkConfigurationUpdater::CreateForDevicePolicy(
            policy_service_.get(), &network_config_handler_,
            &network_device_handler_, chromeos::CrosSettings::Get(),
            testing_device_asset_id_getter);
    return network_configuration_updater_.get();
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<chromeos::onc::OncParsedCertificates> fake_certificates_;
  StrictMock<chromeos::MockManagedNetworkConfigurationHandler>
      network_config_handler_;
  FakeNetworkDeviceHandler network_device_handler_;
  chromeos::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  chromeos::ScopedTestDeviceSettingsService scoped_device_settings_service_;
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;

  // Ownership of client_certificate_importer_owned_ is passed to the
  // NetworkConfigurationUpdater. When that happens, |certificate_importer_|
  // continues to point to that instance but
  // |client_certificate_importer_owned_| is released.
  FakeCertificateImporter* certificate_importer_;
  std::unique_ptr<chromeos::onc::CertificateImporter>
      client_certificate_importer_owned_;

  StrictMock<MockConfigurationPolicyProvider> provider_;
  std::unique_ptr<PolicyServiceImpl> policy_service_;
  FakeUser fake_user_;

  TestingProfile profile_;

  std::unique_ptr<NetworkConfigurationUpdater> network_configuration_updater_;

 private:
  void SetExpectedValueInNetworkConfig(
      base::Value* network_configs,
      base::StringPiece guid,
      std::initializer_list<base::StringPiece> path,
      base::Value value) {
    for (base::Value& network_config : network_configs->GetList()) {
      const base::Value* guid_value =
          network_config.FindKey(::onc::network_config::kGUID);
      if (!guid_value || guid_value->GetString() != guid)
        continue;
      network_config.SetPath(path, std::move(value));
      break;
    }
  }

  base::Value fake_network_configs_;
  base::DictionaryValue fake_global_network_config_;
};

TEST_F(NetworkConfigurationUpdaterTest, CellularAllowRoaming) {
  // Ignore network config updates.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _, _)).Times(AtLeast(1));

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kSignedDataRoamingEnabled, false);
  EXPECT_FALSE(network_device_handler_.allow_roaming_);

  CreateNetworkConfigurationUpdaterForDevicePolicy();
  MarkPolicyProviderInitialized();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kSignedDataRoamingEnabled, true);
  EXPECT_TRUE(network_device_handler_.allow_roaming_);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kSignedDataRoamingEnabled, false);
  EXPECT_FALSE(network_device_handler_.allow_roaming_);
}

TEST_F(NetworkConfigurationUpdaterTest, PolicyIsValidatedAndRepaired) {
  std::unique_ptr<base::DictionaryValue> onc_repaired =
      chromeos::onc::test_utils::ReadTestDictionary(
          "repaired_toplevel_partially_invalid.onc");

  base::ListValue* network_configs_repaired = NULL;
  onc_repaired->GetListWithoutPathExpansion(
      onc::toplevel_config::kNetworkConfigurations, &network_configs_repaired);
  ASSERT_TRUE(network_configs_repaired);

  base::DictionaryValue* global_config_repaired = NULL;
  onc_repaired->GetDictionaryWithoutPathExpansion(
      onc::toplevel_config::kGlobalNetworkConfiguration,
      &global_config_repaired);
  ASSERT_TRUE(global_config_repaired);

  std::string onc_policy =
      chromeos::onc::test_utils::ReadTestData("toplevel_partially_invalid.onc");
  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(onc_policy), nullptr);
  UpdateProviderPolicy(policy);

  EXPECT_CALL(network_config_handler_,
              SetPolicy(onc::ONC_SOURCE_USER_POLICY,
                        _,
                        IsEqualTo(network_configs_repaired),
                        IsEqualTo(global_config_repaired)));

  std::vector<chromeos::onc::OncParsedCertificates::ClientCertificate>
      expected_client_certificates;
  ASSERT_NO_FATAL_FAILURE(SelectSingleClientCertificateFromOnc(
      onc_repaired.get(), 1 /* client_certificate_index */,
      &expected_client_certificates));
  certificate_importer_->SetExpectedONCClientCertificates(
      expected_client_certificates);

  CreateNetworkConfigurationUpdaterForUserPolicy(
      true /* set certificate importer */);
  MarkPolicyProviderInitialized();
  EXPECT_EQ(1u, certificate_importer_->GetAndResetImportCount());
}

TEST_F(NetworkConfigurationUpdaterTest,
       WebTrustedCertificatesFromPolicyInitially) {
  // Ignore network configuration changes.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _, _))
      .Times(AnyNumber());

  UserNetworkConfigurationUpdater* updater =
      CreateNetworkConfigurationUpdaterForUserPolicy(
          false /* set certificate importer */);

  MockPolicyProvidedCertsObserver observer;
  EXPECT_CALL(observer, OnPolicyProvidedCertsChanged());
  updater->AddPolicyProvidedCertsObserver(&observer);

  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(kFakeONC), nullptr);
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
  // Ignore network configuration changes.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _, _))
      .Times(AnyNumber());

  // Start with an empty certificate list.
  UserNetworkConfigurationUpdater* updater =
      CreateNetworkConfigurationUpdaterForUserPolicy(
          false /* set certificate importer */);
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
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(kFakeONC), nullptr);
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
  // Ignore network configuration changes.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _, _))
      .Times(AnyNumber());

  NetworkConfigurationUpdater* updater =
      CreateNetworkConfigurationUpdaterForDevicePolicy();

  MockPolicyProvidedCertsObserver observer;
  EXPECT_CALL(observer, OnPolicyProvidedCertsChanged());
  updater->AddPolicyProvidedCertsObserver(&observer);

  PolicyMap policy;
  policy.Set(key::kDeviceOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(kFakeONCWithExtensionScopedCert),
             nullptr);
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

TEST_F(NetworkConfigurationUpdaterTest,
       DontImportCertificateBeforeCertificateImporterSet) {
  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(kFakeONC), nullptr);
  UpdateProviderPolicy(policy);

  ::onc::ONCSource source = onc::ONC_SOURCE_USER_POLICY;
  EXPECT_CALL(network_config_handler_,
              SetPolicy(source, kFakeUsernameHash,
                        IsEqualTo(GetExpectedFakeNetworkConfigs(source)),
                        IsEqualTo(GetExpectedFakeGlobalNetworkConfig())));

  UserNetworkConfigurationUpdater* updater =
      CreateNetworkConfigurationUpdaterForUserPolicy(
          false /* do not set certificate importer */);
  MarkPolicyProviderInitialized();

  Mock::VerifyAndClearExpectations(&network_config_handler_);
  EXPECT_EQ(0u, certificate_importer_->GetAndResetImportCount());

  certificate_importer_->SetExpectedONCClientCertificates(
      fake_certificates_->client_certificates());

  ASSERT_TRUE(client_certificate_importer_owned_);
  updater->SetClientCertificateImporterForTest(
      std::move(client_certificate_importer_owned_));
  EXPECT_EQ(1u, certificate_importer_->GetAndResetImportCount());
}

TEST_F(NetworkConfigurationUpdaterTest, ReplaceDeviceOncPlaceholders) {
  PolicyMap policy;
  policy.Set(key::kDeviceOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(kFakeONC), nullptr);
  UpdateProviderPolicy(policy);

  ::onc::ONCSource source = onc::ONC_SOURCE_DEVICE_POLICY;
  EXPECT_CALL(network_config_handler_,
              SetPolicy(source, std::string(),
                        IsEqualTo(GetExpectedFakeNetworkConfigs(source)),
                        IsEqualTo(GetExpectedFakeGlobalNetworkConfig())));

  CreateNetworkConfigurationUpdaterForDevicePolicy();
  MarkPolicyProviderInitialized();
}

TEST(UserNetworkConfigurationStaticsTest, TestHasWebTrustedCertsNo) {
  const char kONCWithoutWebTrustedCert[] = R"(
    { "Certificates": [
        { "GUID": "{d443ad0d-ea16-4301-9089-588115e2f5c4}",
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
    -----END CERTIFICATE-----" }
      ],
      "Type": "UnencryptedConfiguration"
    })";
  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(kONCWithoutWebTrustedCert), nullptr);
  EXPECT_FALSE(
      UserNetworkConfigurationUpdater::PolicyHasWebTrustedAuthorityCertificate(
          policy));
}

TEST(UserNetworkConfigurationStaticsTest, TestHasWebTrustedCertsYes) {
  const char kONCWithWebTrustedCert[] = R"(
    { "Certificates": [
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
    -----END CERTIFICATE-----" }
      ],
      "Type": "UnencryptedConfiguration"
    })";
  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(kONCWithWebTrustedCert), nullptr);
  EXPECT_TRUE(
      UserNetworkConfigurationUpdater::PolicyHasWebTrustedAuthorityCertificate(
          policy));
}

TEST(UserNetworkConfigurationStaticsTest, TestHasWebTrustedCertsNoPolicy) {
  PolicyMap policy;
  EXPECT_FALSE(
      UserNetworkConfigurationUpdater::PolicyHasWebTrustedAuthorityCertificate(
          policy));
}

TEST(UserNetworkConfigurationStaticsTest, TestHasWebTrustedCertsInvalidPolicy) {
  const char kInvalidONC[] = "not even valid json";
  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(kInvalidONC), nullptr);
  EXPECT_FALSE(
      UserNetworkConfigurationUpdater::PolicyHasWebTrustedAuthorityCertificate(
          policy));
}

class NetworkConfigurationUpdaterTestWithParam
    : public NetworkConfigurationUpdaterTest,
      public testing::WithParamInterface<const char*> {
 protected:
  // Returns the currently tested ONC source.
  onc::ONCSource CurrentONCSource() {
    if (GetParam() == key::kOpenNetworkConfiguration)
      return onc::ONC_SOURCE_USER_POLICY;
    DCHECK(GetParam() == key::kDeviceOpenNetworkConfiguration);
    return onc::ONC_SOURCE_DEVICE_POLICY;
  }

  // Returns the expected username hash to push policies to
  // ManagedNetworkConfigurationHandler.
  std::string ExpectedUsernameHash() {
    if (GetParam() == key::kOpenNetworkConfiguration)
      return kFakeUsernameHash;
    return std::string();
  }

  size_t ExpectedImportCertificatesCallCount() {
    if (GetParam() == key::kOpenNetworkConfiguration)
      return 1u;
    return 0u;
  }

  void CreateNetworkConfigurationUpdater() {
    if (GetParam() == key::kOpenNetworkConfiguration) {
      CreateNetworkConfigurationUpdaterForUserPolicy(
          true /* set certificate importer */);
    } else {
      CreateNetworkConfigurationUpdaterForDevicePolicy();
    }
  }
};

TEST_P(NetworkConfigurationUpdaterTestWithParam, InitialUpdates) {
  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(kFakeONC),
             nullptr);
  UpdateProviderPolicy(policy);

  EXPECT_CALL(
      network_config_handler_,
      SetPolicy(CurrentONCSource(), ExpectedUsernameHash(),
                IsEqualTo(GetExpectedFakeNetworkConfigs(CurrentONCSource())),
                IsEqualTo(GetExpectedFakeGlobalNetworkConfig())));
  certificate_importer_->SetExpectedONCClientCertificates(
      fake_certificates_->client_certificates());

  CreateNetworkConfigurationUpdater();
  MarkPolicyProviderInitialized();
  EXPECT_EQ(ExpectedImportCertificatesCallCount(),
            certificate_importer_->GetAndResetImportCount());
}

TEST_P(NetworkConfigurationUpdaterTestWithParam,
       PolicyNotSetBeforePolicyProviderInitialized) {
  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(kFakeONC),
             nullptr);
  UpdateProviderPolicy(policy);

  CreateNetworkConfigurationUpdater();

  Mock::VerifyAndClearExpectations(&network_config_handler_);
  EXPECT_EQ(0u, certificate_importer_->GetAndResetImportCount());

  EXPECT_CALL(
      network_config_handler_,
      SetPolicy(CurrentONCSource(), ExpectedUsernameHash(),
                IsEqualTo(GetExpectedFakeNetworkConfigs(CurrentONCSource())),
                IsEqualTo(GetExpectedFakeGlobalNetworkConfig())));
  certificate_importer_->SetExpectedONCClientCertificates(
      fake_certificates_->client_certificates());

  MarkPolicyProviderInitialized();
  EXPECT_EQ(ExpectedImportCertificatesCallCount(),
            certificate_importer_->GetAndResetImportCount());
}

TEST_P(NetworkConfigurationUpdaterTestWithParam,
       PolicyAppliedImmediatelyIfProvidersInitialized) {
  MarkPolicyProviderInitialized();

  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(kFakeONC),
             nullptr);
  UpdateProviderPolicy(policy);

  EXPECT_CALL(
      network_config_handler_,
      SetPolicy(CurrentONCSource(), ExpectedUsernameHash(),
                IsEqualTo(GetExpectedFakeNetworkConfigs(CurrentONCSource())),
                IsEqualTo(GetExpectedFakeGlobalNetworkConfig())));
  certificate_importer_->SetExpectedONCClientCertificates(
      fake_certificates_->client_certificates());

  CreateNetworkConfigurationUpdater();

  EXPECT_EQ(ExpectedImportCertificatesCallCount(),
            certificate_importer_->GetAndResetImportCount());
}

TEST_P(NetworkConfigurationUpdaterTestWithParam, PolicyChange) {
  // Ignore the initial updates.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _, _)).Times(AtLeast(1));

  CreateNetworkConfigurationUpdater();
  MarkPolicyProviderInitialized();

  Mock::VerifyAndClearExpectations(&network_config_handler_);
  // The certificate importer is only called if the certificates changes. An
  // empty policy does not count.
  EXPECT_EQ(0u, certificate_importer_->GetAndResetImportCount());

  // The Updater should update if policy changes.
  EXPECT_CALL(
      network_config_handler_,
      SetPolicy(CurrentONCSource(), _,
                IsEqualTo(GetExpectedFakeNetworkConfigs(CurrentONCSource())),
                IsEqualTo(GetExpectedFakeGlobalNetworkConfig())));
  certificate_importer_->SetExpectedONCClientCertificates(
      fake_certificates_->client_certificates());

  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(kFakeONC),
             nullptr);
  UpdateProviderPolicy(policy);
  Mock::VerifyAndClearExpectations(&network_config_handler_);
  EXPECT_EQ(ExpectedImportCertificatesCallCount(),
            certificate_importer_->GetAndResetImportCount());

  // Another update is expected if the policy goes away.
  EXPECT_CALL(network_config_handler_,
              SetPolicy(CurrentONCSource(), _, IsEmpty(), IsEmpty()));
  certificate_importer_->SetExpectedONCClientCertificates({});

  policy.Erase(GetParam());
  UpdateProviderPolicy(policy);
  EXPECT_EQ(ExpectedImportCertificatesCallCount(),
            certificate_importer_->GetAndResetImportCount());
}

INSTANTIATE_TEST_SUITE_P(NetworkConfigurationUpdaterTestWithParamInstance,
                         NetworkConfigurationUpdaterTestWithParam,
                         testing::Values(key::kDeviceOpenNetworkConfiguration,
                                         key::kOpenNetworkConfiguration));

}  // namespace policy
