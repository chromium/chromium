// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/policy/networking/device_network_configuration_updater_ash.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater_ash.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/network/fake_network_device_handler.h"
#include "chromeos/ash/components/network/mock_managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/onc/onc_certificate_importer.h"
#include "chromeos/ash/components/network/policy_certificate_provider.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/components/onc/certificate_scope.h"
#include "chromeos/components/onc/onc_parsed_certificates.h"
#include "chromeos/components/onc/onc_test_utils.h"
#include "chromeos/components/onc/onc_utils.h"
#include "components/account_id/account_id.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
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

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Eq;
using testing::Mock;
using testing::Ne;
using testing::Return;
using testing::StrictMock;

namespace policy {

namespace {

const char kFakeUserEmail[] = "fakeuser@fakedomain.com";
const char kFakeSerialNumber[] = "FakeSerial";
const char kFakeAssetId[] = "FakeAssetId";

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

class FakeNetworkDeviceHandler : public ash::FakeNetworkDeviceHandler {
 public:
  FakeNetworkDeviceHandler() = default;

  FakeNetworkDeviceHandler(const FakeNetworkDeviceHandler&) = delete;
  FakeNetworkDeviceHandler& operator=(const FakeNetworkDeviceHandler&) = delete;

  void SetCellularPolicyAllowRoaming(bool policy_allow_roaming) override {
    policy_allow_roaming_ = policy_allow_roaming;
  }

  void SetMACAddressRandomizationEnabled(bool enabled) override {
    mac_addr_randomization_ = enabled;
  }

  bool policy_allow_roaming_ = true;
  bool mac_addr_randomization_ = false;
};

class FakeCertificateImporter : public ash::onc::CertificateImporter {
 public:
  using OncParsedCertificates = chromeos::onc::OncParsedCertificates;

  FakeCertificateImporter() : call_count_(0) {}

  FakeCertificateImporter(const FakeCertificateImporter&) = delete;
  FakeCertificateImporter& operator=(const FakeCertificateImporter&) = delete;

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
    NOTREACHED_IN_MIGRATION();
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

template <typename T>
std::string ValueToString(const T& value) {
  std::stringstream str;
  str << value;
  return str.str();
}

// Selects only the client certificate at |client_certificate_index| from the
// certificates contained in |toplevel_onc|. Appends the selected certificate
// into |out_parsed_client_certificates|.
void SelectSingleClientCertificateFromOnc(
    base::Value::Dict& toplevel_onc,
    size_t client_certificate_index,
    std::vector<chromeos::onc::OncParsedCertificates::ClientCertificate>*
        out_parsed_client_certificates) {
  const base::Value::List* certs =
      toplevel_onc.FindList(onc::toplevel_config::kCertificates);
  ASSERT_TRUE(certs);
  ASSERT_TRUE(certs->size() > client_certificate_index);

  base::Value::List selected_certs;
  selected_certs.Append((*certs)[client_certificate_index].Clone());

  chromeos::onc::OncParsedCertificates parsed_selected_certs(selected_certs);
  ASSERT_FALSE(parsed_selected_certs.has_error());
  ASSERT_EQ(1u, parsed_selected_certs.client_certificates().size());
  out_parsed_client_certificates->push_back(
      parsed_selected_certs.client_certificates().front());
}

// Matcher to match `base::Value` with a compatible type (string, `base::Value`,
// `base::Value::Dict`, `base::Value::List` etc.). See the `==` operator
// overrides in `base/values.h` and the definition of `ValueToString()` for
// the restrictions on allowed types for `value`.
MATCHER_P(IsEqualTo,
          value,
          std::string(negation ? "isn't" : "is") + " equal to " +
              ValueToString(*value)) {
  return *value == arg;
}

MATCHER(IsListEmpty, std::string(negation ? "isn't" : "is") + " empty.") {
  return arg.empty();
}

MATCHER(IsDictEmpty, std::string(negation ? "isn't" : "is") + " empty.") {
  return arg.empty();
}

ACTION_P(SetCertificateList, list) {
  if (arg2)
    *arg2 = list;
  return true;
}

}  // namespace

class NetworkConfigurationUpdaterAshTest : public testing::Test {
 protected:
  NetworkConfigurationUpdaterAshTest() : certificate_importer_(nullptr) {}

  void SetUp() override {
    fake_user_ = static_cast<ash::FakeChromeUserManager*>(
                     user_manager::UserManager::Get())
                     ->AddUser(AccountId::FromUserEmail(kFakeUserEmail));

    ash::UserSessionManager::GetInstance()->set_start_session_type_for_testing(
        ash::UserSessionManager::StartSessionType::kPrimary);

    fake_statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                                  kFakeSerialNumber);

    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(provider_, IsFirstPolicyLoadComplete(_))
        .WillRepeatedly(Return(false));
    provider_.Init();
    PolicyServiceImpl::Providers providers;
    providers.push_back(&provider_);
    policy_service_ = std::make_unique<PolicyServiceImpl>(std::move(providers));

    std::optional<base::Value::Dict> fake_toplevel_onc =
        chromeos::onc::ReadDictionaryFromJson(kFakeONC);
    ASSERT_TRUE(fake_toplevel_onc.has_value());

    base::Value::Dict* global_config = fake_toplevel_onc->FindDict(
        onc::toplevel_config::kGlobalNetworkConfiguration);
    fake_global_network_config_.Merge(global_config->Clone());

    base::Value::List* certs =
        fake_toplevel_onc->FindList(onc::toplevel_config::kCertificates);
    ASSERT_TRUE(certs);

    fake_certificates_ =
        std::make_unique<chromeos::onc::OncParsedCertificates>(*certs);

    certificate_importer_ = new FakeCertificateImporter;
    client_certificate_importer_owned_.reset(certificate_importer_);

    EXPECT_CALL(network_config_handler_, SetProfileWideVariableExpansions(_, _))
        .Times(AnyNumber());
  }

  base::Value::List* GetExpectedFakeNetworkConfigs(::onc::ONCSource source) {
    std::optional<base::Value::Dict> fake_toplevel_onc =
        chromeos::onc::ReadDictionaryFromJson(kFakeONC);
    if (!fake_toplevel_onc.has_value()) {
      return nullptr;
    }
    fake_network_configs_ =
        fake_toplevel_onc
            ->FindList(onc::toplevel_config::kNetworkConfigurations)
            ->Clone();
    return &fake_network_configs_;
  }

  base::Value::Dict* GetExpectedFakeGlobalNetworkConfig() {
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

  UserNetworkConfigurationUpdaterAsh*
  CreateNetworkConfigurationUpdaterForUserPolicy(
      bool set_client_cert_importer) {
    UserNetworkConfigurationUpdaterAsh* updater =
        UserNetworkConfigurationUpdaterAsh::CreateForUserPolicy(
            &profile_, *fake_user_, policy_service_.get(),
            &network_config_handler_)
            .release();
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
        DeviceNetworkConfigurationUpdaterAsh::CreateForDevicePolicy(
            policy_service_.get(), &network_config_handler_,
            &network_device_handler_, ash::CrosSettings::Get(),
            testing_device_asset_id_getter);
    return network_configuration_updater_.get();
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<chromeos::onc::OncParsedCertificates> fake_certificates_;
  StrictMock<ash::MockManagedNetworkConfigurationHandler>
      network_config_handler_;
  FakeNetworkDeviceHandler network_device_handler_;
  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  ash::ScopedTestDeviceSettingsService scoped_device_settings_service_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;

  // Ownership of client_certificate_importer_owned_ is passed to the
  // NetworkConfigurationUpdater. When that happens, |certificate_importer_|
  // continues to point to that instance but
  // |client_certificate_importer_owned_| is released.
  raw_ptr<FakeCertificateImporter, DanglingUntriaged> certificate_importer_;
  std::unique_ptr<ash::onc::CertificateImporter>
      client_certificate_importer_owned_;

  TestingProfile profile_;

  StrictMock<MockConfigurationPolicyProvider> provider_;
  std::unique_ptr<PolicyServiceImpl> policy_service_;
  raw_ptr<const user_manager::User> fake_user_;

  std::unique_ptr<NetworkConfigurationUpdater> network_configuration_updater_;

 private:
  base::Value::List fake_network_configs_;
  base::Value::Dict fake_global_network_config_;
  ash::ScopedFakeSessionManagerClient scoped_session_manager_client_;
};

TEST_F(NetworkConfigurationUpdaterAshTest, CellularRoamingDefaults) {
  // Ignore network config updates.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _, _)).Times(AtLeast(1));

  CreateNetworkConfigurationUpdaterForDevicePolicy();
  MarkPolicyProviderInitialized();
  EXPECT_TRUE(network_device_handler_.policy_allow_roaming_);
}

TEST_F(NetworkConfigurationUpdaterAshTest, CellularPolicyAllowRoamingManaged) {
  // Ignore network config updates.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _, _)).Times(AtLeast(1));

  // Perform this test as though this "device" is enterprise managed.
  scoped_stub_install_attributes_.Get()->SetCloudManaged(
      policy::PolicyBuilder::kFakeDomain, policy::PolicyBuilder::kFakeDeviceId);
  EXPECT_TRUE(ash::InstallAttributes::Get()->IsEnterpriseManaged());

  CreateNetworkConfigurationUpdaterForDevicePolicy();
  MarkPolicyProviderInitialized();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kSignedDataRoamingEnabled, true);
  EXPECT_TRUE(network_device_handler_.policy_allow_roaming_);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kSignedDataRoamingEnabled, false);
  EXPECT_FALSE(network_device_handler_.policy_allow_roaming_);
}

TEST_F(NetworkConfigurationUpdaterAshTest,
       CellularPolicyAllowRoamingUnmanaged) {
  // Ignore network config updates.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _, _)).Times(AtLeast(1));

  // Perform this test as though this "device" is unmanaged.
  scoped_stub_install_attributes_.Get()->SetConsumerOwned();
  EXPECT_FALSE(ash::InstallAttributes::Get()->IsEnterpriseManaged());

  CreateNetworkConfigurationUpdaterForDevicePolicy();
  MarkPolicyProviderInitialized();
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kSignedDataRoamingEnabled, true);
  EXPECT_TRUE(network_device_handler_.policy_allow_roaming_);

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ash::kSignedDataRoamingEnabled, false);
  EXPECT_TRUE(network_device_handler_.policy_allow_roaming_);
}

TEST_F(NetworkConfigurationUpdaterAshTest, PolicyIsValidatedAndRepaired) {
  base::Value::Dict onc_repaired =
      chromeos::onc::test_utils::ReadTestDictionary(
          "repaired_toplevel_partially_invalid.onc");

  base::Value::List* network_configs_repaired =
      onc_repaired.FindList(onc::toplevel_config::kNetworkConfigurations);
  ASSERT_TRUE(network_configs_repaired);

  base::Value::Dict* global_config_repaired =
      onc_repaired.FindDict(onc::toplevel_config::kGlobalNetworkConfiguration);
  ASSERT_TRUE(global_config_repaired);

  std::string onc_policy =
      chromeos::onc::test_utils::ReadTestData("toplevel_partially_invalid.onc");
  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(onc_policy),
             nullptr);
  UpdateProviderPolicy(policy);

  EXPECT_CALL(network_config_handler_,
              SetPolicy(onc::ONC_SOURCE_USER_POLICY, _,
                        IsEqualTo(network_configs_repaired),
                        IsEqualTo(global_config_repaired)));

  std::vector<chromeos::onc::OncParsedCertificates::ClientCertificate>
      expected_client_certificates;
  ASSERT_NO_FATAL_FAILURE(SelectSingleClientCertificateFromOnc(
      onc_repaired, 1 /* client_certificate_index */,
      &expected_client_certificates));
  certificate_importer_->SetExpectedONCClientCertificates(
      expected_client_certificates);

  CreateNetworkConfigurationUpdaterForUserPolicy(
      true /* set certificate importer */);
  MarkPolicyProviderInitialized();
  EXPECT_EQ(1u, certificate_importer_->GetAndResetImportCount());
}

TEST_F(NetworkConfigurationUpdaterAshTest,
       DontImportCertificateBeforeCertificateImporterSet) {
  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(kFakeONC),
             nullptr);
  UpdateProviderPolicy(policy);

  ::onc::ONCSource source = onc::ONC_SOURCE_USER_POLICY;
  EXPECT_CALL(network_config_handler_,
              SetPolicy(source, fake_user_->username_hash(),
                        IsEqualTo(GetExpectedFakeNetworkConfigs(source)),
                        IsEqualTo(GetExpectedFakeGlobalNetworkConfig())));

  UserNetworkConfigurationUpdaterAsh* updater =
      CreateNetworkConfigurationUpdaterForUserPolicy(
          false /* do not set certificate importer */);
  MarkPolicyProviderInitialized();

  Mock::VerifyAndClearExpectations(&network_config_handler_);
  EXPECT_CALL(network_config_handler_, SetProfileWideVariableExpansions(_, _))
      .Times(AnyNumber());
  EXPECT_EQ(0u, certificate_importer_->GetAndResetImportCount());

  certificate_importer_->SetExpectedONCClientCertificates(
      fake_certificates_->client_certificates());

  ASSERT_TRUE(client_certificate_importer_owned_);
  updater->SetClientCertificateImporterForTest(
      std::move(client_certificate_importer_owned_));
  EXPECT_EQ(1u, certificate_importer_->GetAndResetImportCount());
}

TEST_F(NetworkConfigurationUpdaterAshTest, SetDeviceVariableExpansions) {
  Mock::VerifyAndClearExpectations(&network_config_handler_);
  const base::flat_map<std::string, std::string> kExpectedExpansions = {
      {"DEVICE_ASSET_ID", kFakeAssetId},
      {"DEVICE_SERIAL_NUMBER", kFakeSerialNumber}};
  PolicyMap policy;
  policy.Set(key::kDeviceOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, base::Value(kFakeONC),
             nullptr);
  UpdateProviderPolicy(policy);

  ::onc::ONCSource source = onc::ONC_SOURCE_DEVICE_POLICY;
  EXPECT_CALL(network_config_handler_,
              SetPolicy(source, /*userhash=*/std::string(),
                        IsEqualTo(GetExpectedFakeNetworkConfigs(source)),
                        IsEqualTo(GetExpectedFakeGlobalNetworkConfig())));
  EXPECT_CALL(network_config_handler_,
              SetProfileWideVariableExpansions(/*userhash=*/std::string(),
                                               Eq(kExpectedExpansions)));

  CreateNetworkConfigurationUpdaterForDevicePolicy();
  MarkPolicyProviderInitialized();
}

TEST_F(NetworkConfigurationUpdaterAshTest, SetUserVariableExpansions) {
  Mock::VerifyAndClearExpectations(&network_config_handler_);
  const base::flat_map<std::string, std::string> kExpectedExpansions = {
      {"LOGIN_EMAIL", kFakeUserEmail},
      {"LOGIN_ID", "fakeuser"},  // The prefix of kFakeUserEmail before @.
  };
  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(kFakeONC),
             nullptr);
  UpdateProviderPolicy(policy);

  ::onc::ONCSource source = onc::ONC_SOURCE_USER_POLICY;
  EXPECT_CALL(network_config_handler_,
              SetPolicy(source, fake_user_->username_hash(),
                        IsEqualTo(GetExpectedFakeNetworkConfigs(source)),
                        IsEqualTo(GetExpectedFakeGlobalNetworkConfig())));
  EXPECT_CALL(
      network_config_handler_,
      SetProfileWideVariableExpansions(/*userhash=*/fake_user_->username_hash(),
                                       Eq(kExpectedExpansions)));

  CreateNetworkConfigurationUpdaterForUserPolicy(
      /*set_client_cert_importer=*/false);
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
             base::Value(kONCWithoutWebTrustedCert), nullptr);
  EXPECT_FALSE(UserNetworkConfigurationUpdaterAsh::
                   PolicyHasWebTrustedAuthorityCertificate(policy));
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
             base::Value(kONCWithWebTrustedCert), nullptr);
  EXPECT_TRUE(UserNetworkConfigurationUpdaterAsh::
                  PolicyHasWebTrustedAuthorityCertificate(policy));
}

TEST(UserNetworkConfigurationStaticsTest, TestHasWebTrustedCertsNoPolicy) {
  PolicyMap policy;
  EXPECT_FALSE(UserNetworkConfigurationUpdaterAsh::
                   PolicyHasWebTrustedAuthorityCertificate(policy));
}

TEST(UserNetworkConfigurationStaticsTest, TestHasWebTrustedCertsInvalidPolicy) {
  const char kInvalidONC[] = "not even valid json";
  PolicyMap policy;
  policy.Set(key::kOpenNetworkConfiguration, POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(kInvalidONC),
             nullptr);
  EXPECT_FALSE(UserNetworkConfigurationUpdaterAsh::
                   PolicyHasWebTrustedAuthorityCertificate(policy));
}

class NetworkConfigurationUpdaterAshTestWithParam
    : public NetworkConfigurationUpdaterAshTest,
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
      return fake_user_->username_hash();
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

TEST_P(NetworkConfigurationUpdaterAshTestWithParam, InitialUpdates) {
  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value(kFakeONC), nullptr);
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

TEST_P(NetworkConfigurationUpdaterAshTestWithParam,
       PolicyNotSetBeforePolicyProviderInitialized) {
  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value(kFakeONC), nullptr);
  UpdateProviderPolicy(policy);

  CreateNetworkConfigurationUpdater();

  Mock::VerifyAndClearExpectations(&network_config_handler_);
  EXPECT_CALL(network_config_handler_, SetProfileWideVariableExpansions(_, _))
      .Times(AnyNumber());
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

TEST_P(NetworkConfigurationUpdaterAshTestWithParam,
       PolicyAppliedImmediatelyIfProvidersInitialized) {
  MarkPolicyProviderInitialized();

  PolicyMap policy;
  policy.Set(GetParam(), POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             POLICY_SOURCE_CLOUD, base::Value(kFakeONC), nullptr);
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

TEST_P(NetworkConfigurationUpdaterAshTestWithParam, PolicyChange) {
  // Ignore the initial updates.
  EXPECT_CALL(network_config_handler_, SetPolicy(_, _, _, _)).Times(AtLeast(1));

  CreateNetworkConfigurationUpdater();
  MarkPolicyProviderInitialized();

  Mock::VerifyAndClearExpectations(&network_config_handler_);
  EXPECT_CALL(network_config_handler_, SetProfileWideVariableExpansions(_, _))
      .Times(AnyNumber());
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
             POLICY_SOURCE_CLOUD, base::Value(kFakeONC), nullptr);
  UpdateProviderPolicy(policy);
  Mock::VerifyAndClearExpectations(&network_config_handler_);
  EXPECT_CALL(network_config_handler_, SetProfileWideVariableExpansions(_, _))
      .Times(AnyNumber());
  EXPECT_EQ(ExpectedImportCertificatesCallCount(),
            certificate_importer_->GetAndResetImportCount());

  // Another update is expected if the policy goes away.
  EXPECT_CALL(network_config_handler_,
              SetPolicy(CurrentONCSource(), _, IsListEmpty(), IsDictEmpty()));
  certificate_importer_->SetExpectedONCClientCertificates({});

  policy.Erase(GetParam());
  UpdateProviderPolicy(policy);
  EXPECT_EQ(ExpectedImportCertificatesCallCount(),
            certificate_importer_->GetAndResetImportCount());
}

INSTANTIATE_TEST_SUITE_P(NetworkConfigurationUpdaterAshTestWithParamInstance,
                         NetworkConfigurationUpdaterAshTestWithParam,
                         testing::Values(key::kDeviceOpenNetworkConfiguration,
                                         key::kOpenNetworkConfiguration));

}  // namespace policy
