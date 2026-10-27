// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pki_metadata_component_installer.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/component_updater/pki_metadata_component_installer_policy.h"
#include "components/certificate_transparency/certificate_transparency_config.pb.h"
#include "components/certificate_transparency/ct_known_logs.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/features.h"
#include "net/base/hash_value.h"
#include "net/cert/cert_verify_proc.h"
#include "net/net_buildflags.h"
#include "services/cert_verifier/cert_verifier_service_factory.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/ct_log_info.mojom.h"
#include "services/network/sct_auditing/sct_auditing_cache.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

#if BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
#include "chrome/browser/net/key_pinning.pb.h"
#include "net/cert/key_pinning_state.h"
#include "net/cert/key_pinning_state_chrome_test_util.h"
#include "net/http/transport_security_state.h"
#endif

namespace component_updater {

namespace {
// An arbitrary, DER-encoded subjectpublickeyinfo encoded as BASE64.
constexpr char kLogSPKIBase64[] =
    "MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEA1+5tMt7sGC0MTw/AloiaTsFbEpW3s"
    "g3GCAFY6wRP5Izt+mV/Q9xHb450LppfptaYh94nIkVIhtTkSQ4b2GRxOAcaSkpTsN+PUPaO2D"
    "Jd/5M7MFxXeHGYqXIbdD+rgSsU5rcBspFbJkRv+Q34bqNeiwKT+zcYqfAEH3cYvDGF+FIxXrZ"
    "YqUAmTQJZtzHfBP/2ZWfkyAHwPJqnz3aWS0MmTLY3JUF/BnSt381U6ZtI+qvmV+aWoDg2X2kx"
    "lSR2WsEGPYrO+/7dHmNn3XLqWj+aWe7qoK3wRcxsq081ONsMGKGwm6yY35I+prob1WA4avtli"
    "gMjNT0t7SJZoTkHSvgtDuo8Kfb5HrSyoMwIpIyI85XBoL2zM+BTJcCP9gyUnkiq6H9bWv1S20"
    "ujMYhssH+UkAvpgSYBbgq8g1ta5cpOb2sm7qqEG7V7Aw1AhXkVAuuuzjQK29feSaZOtrOueo2"
    "6tGd154DY2+AzwcrBDSfNw7XZqrFpgl9saW22L+P683b0wV2rlkFmc5O4LlIyk+oCuNoubS/t"
    "Iyrq1REXsoc/O0HVCXQXKP1/g6mduco4wA57lH1BSJrSet5Rc8NyR5g7zR8FPzXvav+eErLwd"
    "RsVdo4HNxlBlrc50CqkbsNFg2hdU1uCbbzRHKAF5Ih/NGdFkQZ9N+pPbTcpA8z5mWyjo6cCAw"
    "EAAQ==";
constexpr char kLogIdBase64[] = "KHWaS8pa+aGJCk5BUfi+NfHcNTRSlVLLQ8/A3d3QN3w=";
constexpr uint64_t kLogMMDSeconds = 42;
constexpr char kLogURL[] = "https://futuregadgetlab.jp";
constexpr char kLogName[] = "FutureGadgetLog2022";
constexpr char kLogOperatorName[] = "Future Gadget Lab";
constexpr char kLogOperatorEmail[] = "kurisu@dmail.com";
constexpr base::TimeDelta kCurrentOperatorStart = base::Days(3);
constexpr char kPreviousOperator1Name[] = "SERN";
constexpr base::TimeDelta kPreviousOperator1Start = base::Days(2);
constexpr char kPreviousOperator2Name[] = "DURPA";
constexpr base::TimeDelta kPreviousOperator2Start = base::Days(1);
constexpr char kGoogleLogName[] = "GoogleLog2022";
constexpr char kGoogleLogOperatorName[] = "Google";
constexpr base::TimeDelta kGoogleLogDisqualificationDate = base::Days(2);

// BASE64 encoded fake leaf hashes.
constexpr char kPopularSCT1[] = "EBESExQVFhcYGRobHB0eHwEjRWeJq83v";
constexpr char kPopularSCT2[] = "oKGio6SlpqeoqaqrrK2urwEjRWeJq83v";

constexpr uint64_t kMaxSupportedCTCompatibilityVersion = 4;

class CTLogListWaiter : public PKIMetadataComponentInstallerService::Observer {
 public:
  CTLogListWaiter() {
    PKIMetadataComponentInstallerService::GetInstance()->AddObserver(this);
  }
  ~CTLogListWaiter() override {
    PKIMetadataComponentInstallerService::GetInstance()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  void OnCTLogListProcessed() override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
};

#if BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
class KeyPinsListWaiter
    : public PKIMetadataComponentInstallerService::Observer {
 public:
  KeyPinsListWaiter() {
    PKIMetadataComponentInstallerService::GetInstance()->AddObserver(this);
  }
  ~KeyPinsListWaiter() override {
    PKIMetadataComponentInstallerService::GetInstance()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  void OnKeyPinsListProcessed() override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
};
#endif

}  // namespace

class PKIMetadataComponentInstallerTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::
                                  kCertificateTransparencyAskBeforeEnabling},
        /*disabled_features=*/{});
    ct_config_.set_disable_ct_enforcement(false);
    ct_config_.mutable_log_list()->set_compatibility_version(
        kMaxSupportedCTCompatibilityVersion);
    ct_config_.mutable_log_list()->mutable_timestamp()->set_seconds(
        (certificate_transparency::GetLogListTimestamp()
             .InMillisecondsSinceUnixEpoch() /
         1000) +
        1);
    {
      auto* log_operator = ct_config_.mutable_log_list()->add_operators();
      log_operator->add_email(kLogOperatorEmail);
      log_operator->set_name(kLogOperatorName);
    }
    {
      auto* log_operator = ct_config_.mutable_log_list()->add_operators();
      log_operator->add_email(kLogOperatorEmail);
      log_operator->set_name(kGoogleLogOperatorName);
    }
    {
      // Configure a qualified non-google log with an operator history.
      auto* ct_log = ct_config_.mutable_log_list()->add_logs();
      ct_log->set_description(kLogName);
      ct_log->set_key(kLogSPKIBase64);
      ct_log->set_log_id(kLogIdBase64);
      ct_log->set_mmd_secs(kLogMMDSeconds);
      ct_log->set_url(kLogURL);

      auto* operator_change = ct_log->add_operator_history();
      operator_change->set_name(kLogOperatorName);
      operator_change->mutable_operator_start()->set_seconds(
          kCurrentOperatorStart.InSeconds());
      operator_change = ct_log->add_operator_history();
      operator_change->set_name(kPreviousOperator1Name);
      operator_change->mutable_operator_start()->set_seconds(
          kPreviousOperator1Start.InSeconds());
      operator_change = ct_log->add_operator_history();
      operator_change->set_name(kPreviousOperator2Name);
      operator_change->mutable_operator_start()->set_seconds(
          kPreviousOperator2Start.InSeconds());

      auto* log_state = ct_log->add_state();
      log_state->set_current_state(chrome_browser_certificate_transparency::
                                       CTLog_CurrentState_QUALIFIED);
      log_state->mutable_state_start()->set_seconds(10);
    }
    {
      // Configure a non-qualified google log without an operator history.
      auto* ct_log = ct_config_.mutable_log_list()->add_logs();
      ct_log->set_description(kGoogleLogName);
      ct_log->set_key(kLogSPKIBase64);
      ct_log->set_log_id(kLogIdBase64);
      ct_log->set_mmd_secs(kLogMMDSeconds);
      ct_log->set_url(kLogURL);

      auto* operator_change = ct_log->add_operator_history();
      operator_change->set_name(kGoogleLogOperatorName);
      operator_change->mutable_operator_start()->set_seconds(
          kCurrentOperatorStart.InSeconds());

      auto* log_state = ct_log->add_state();
      log_state->set_current_state(
          chrome_browser_certificate_transparency::CTLog_CurrentState_RETIRED);
      log_state->mutable_state_start()->set_seconds(
          kGoogleLogDisqualificationDate.InSeconds());
      log_state = ct_log->add_state();
      log_state->set_current_state(chrome_browser_certificate_transparency::
                                       CTLog_CurrentState_QUALIFIED);
      log_state->mutable_state_start()->set_seconds(10);
    }
    {
      // Configure a log with an invalid log id.
      auto* ct_log = ct_config_.mutable_log_list()->add_logs();
      ct_log->set_log_id("not base64");
      ct_log->set_key(kLogSPKIBase64);
    }
    {
      // Configure a log with an invalid log key.
      auto* ct_log = ct_config_.mutable_log_list()->add_logs();
      ct_log->set_log_id(kLogIdBase64);
      ct_log->set_key("not base64");
    }
    // Configure some popular SCTs.
    ASSERT_TRUE(
        base::Base64Decode(kPopularSCT1, ct_config_.add_popular_scts()));
    ASSERT_TRUE(
        base::Base64Decode(kPopularSCT2, ct_config_.add_popular_scts()));
  }

  void WriteCTConfigToFile() {
    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
    base::FilePath ct_file_path = component_install_dir_.GetPath().Append(
        FILE_PATH_LITERAL("ct_config.pb"));
    std::string ct_data;
    ASSERT_TRUE(ct_config_.SerializeToString(&ct_data));
    ASSERT_TRUE(base::WriteFile(ct_file_path, ct_data));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  component_updater::MockComponentUpdateService mock_component_update_;
  base::ScopedTempDir component_install_dir_;

  chrome_browser_certificate_transparency::CTConfig ct_config_;
  std::unique_ptr<component_updater::ComponentInstallerPolicy> policy_ =
      std::make_unique<
          component_updater::PKIMetadataComponentInstallerPolicy>();
};

#if BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
class PKIMetadataComponentInstallerKeyPinningTest
    : public PKIMetadataComponentInstallerTest {
 public:
  // Constants for test pinset.
  static constexpr char kPinsetName[] = "example";
  // Hostname for the component updated pinset created by these tests. Must not
  // be pinned in the compiled-in static test pins list.
  static constexpr char kPinsetHostName[] = "not-pinned-by-default.test";
  static constexpr bool kPinsetIncludeSubdomains = true;
  static constexpr uint64_t kMaxSupportedKPCompatibilityVersion = 1;

  // SHA256 SPKI hashes.
  static constexpr net::SHA256HashValue kSpkiHash1 = {
      0xec, 0x72, 0x29, 0x69, 0xcb, 0x64, 0x20, 0x0a, 0xb6, 0x63, 0x8f,
      0x68, 0xac, 0x53, 0x8e, 0x40, 0xab, 0xab, 0x5b, 0x19, 0xa6, 0x48,
      0x56, 0x61, 0x04, 0x2a, 0x10, 0x61, 0xc4, 0x61, 0x27, 0x76};
  static constexpr net::SHA256HashValue kSpkiHash2 = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  // Hostname that is pinned in the compiled-in static test pinset.
  static constexpr char kPinnedByDefaultHostname[] = "example.test";

  void SetUp() override {
    PKIMetadataComponentInstallerTest::SetUp();

    // Configure the key pinning pins list.
    {
      pinlist_.mutable_timestamp()->set_seconds(
          (base::Time::Now() - base::Time::UnixEpoch()).InSeconds());
      pinlist_.set_compatibility_version(kMaxSupportedKPCompatibilityVersion);
      auto* host_pin = pinlist_.add_host_pins();
      host_pin->set_hostname(kPinsetHostName);
      host_pin->set_pinset_name(kPinsetName);
      host_pin->set_include_subdomains(kPinsetIncludeSubdomains);
      auto* pinset = pinlist_.add_pinsets();
      pinset->set_name(kPinsetName);
      pinset->add_static_spki_hashes_sha256(base::as_string_view(kSpkiHash1));
      pinset->add_bad_static_spki_hashes_sha256(
          base::as_string_view(kSpkiHash2));
    }
  }

  void WriteKPConfigToFile() {
    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
    base::FilePath kp_file_path = component_install_dir_.GetPath().Append(
        FILE_PATH_LITERAL("kp_pinslist.pb"));
    std::string kp_data;
    ASSERT_TRUE(pinlist_.SerializeToString(&kp_data));
    ASSERT_TRUE(base::WriteFile(kp_file_path, kp_data));
  }

  void WriteKPConfigAndUpdate() {
    WriteKPConfigToFile();

    KeyPinsListWaiter waiter;
    policy_->ComponentReady(base::Version("1.2.3.4"),
                            component_install_dir_.GetPath(),
                            base::DictValue());
    waiter.Wait();
  }

  void ExpectCertVerifierServiceUsesCompiledInPins() {
    cert_verifier::CertVerifierServiceFactoryImpl*
        cert_verifier_service_factory =
            content::GetCertVerifierServiceFactoryForTesting();
    ASSERT_TRUE(cert_verifier_service_factory);
    const net::CertVerifyProc::ImplParams& impl_params =
        cert_verifier_service_factory->get_impl_params();
    ASSERT_TRUE(impl_params.key_pinning_state);
    EXPECT_TRUE(impl_params.key_pinning_state->HasPublicKeyPins(
        kPinnedByDefaultHostname));
    EXPECT_FALSE(
        impl_params.key_pinning_state->HasPublicKeyPins(kPinsetHostName));
  }

 protected:
  net::ScopedKeyPinningStateChromeDefaults scoped_test_key_pinning_enable_;
  chrome_browser_key_pinning::PinList pinlist_;
};
#endif  // BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)

TEST_F(PKIMetadataComponentInstallerTest, TestProtoBytesConversion) {
  std::vector<net::SHA256HashValue> test_hashes = {
      {0xec, 0x72, 0x29, 0x69, 0xcb, 0x64, 0x20, 0x0a, 0xb6, 0x63, 0x8f,
       0x68, 0xac, 0x53, 0x8e, 0x40, 0xab, 0xab, 0x5b, 0x19, 0xa6, 0x48,
       0x56, 0x61, 0x04, 0x2a, 0x10, 0x61, 0xc4, 0x61, 0x27, 0x76}};
  std::vector<std::vector<uint8_t>> test_bytes = {
      base::ToVector(test_hashes[0])};

  std::string bytes_as_string(base::as_string_view(test_bytes[0]));
  std::vector<std::string> repeated_bytes = {bytes_as_string};

  EXPECT_EQ(
      PKIMetadataComponentInstallerService::BytesArrayFromProtoBytesForTesting(
          google::protobuf::RepeatedPtrField<std::string>(
              repeated_bytes.begin(), repeated_bytes.end())),
      test_bytes);
  EXPECT_EQ(PKIMetadataComponentInstallerService::
                SHA256HashValueArrayFromProtoBytesForTesting(
                    google::protobuf::RepeatedPtrField<std::string>(
                        repeated_bytes.begin(), repeated_bytes.end())),
            test_hashes);
}

// Tests that the installation is verified iff the component install directory
// exists.
TEST_F(PKIMetadataComponentInstallerTest, VerifyInstallation) {
  WriteCTConfigToFile();
  base::FilePath path = component_install_dir_.GetPath();
  EXPECT_TRUE(policy_->VerifyInstallation(base::DictValue(), path));
  ASSERT_TRUE(component_install_dir_.Delete());
  EXPECT_FALSE(policy_->VerifyInstallation(base::DictValue(), path));
}

// Tests that the PKI Metadata component is registered if the features are
// enabled.
TEST_F(PKIMetadataComponentInstallerTest, RegisterComponent) {
  EXPECT_CALL(mock_component_update_, RegisterComponent)

      .WillOnce(testing::Return(true));
  component_updater::MaybeRegisterPKIMetadataComponent(&mock_component_update_);
  task_environment_.RunUntilIdle();
}

// Tests that setting the CT enforcement kill switch successfully disables CT
// enforcement.
TEST_F(PKIMetadataComponentInstallerTest, CTEnforcementKillSwitch) {
  // Initialize the network service.
  content::GetNetworkService();
  task_environment_.RunUntilIdle();

  ct_config_.set_disable_ct_enforcement(true);
  WriteCTConfigToFile();
  CTLogListWaiter waiter;
  policy_->ComponentReady(base::Version("1.2.3.4"),
                          component_install_dir_.GetPath(), base::DictValue());
  waiter.Wait();

  network::NetworkService* network_service =
      network::NetworkService::GetNetworkServiceForTesting();
  ASSERT_TRUE(network_service);

  // Logs should not have been updated.
  const std::vector<network::mojom::CTLogInfoPtr>& logs =
      network_service->log_list();
  EXPECT_EQ(logs.size(), 0u);
  EXPECT_FALSE(network_service->is_ct_enforcement_enabled_for_testing());
}

#if BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)
TEST_F(PKIMetadataComponentInstallerKeyPinningTest, VerifyInstallation) {
  WriteKPConfigToFile();
  base::FilePath path = component_install_dir_.GetPath();
  EXPECT_TRUE(policy_->VerifyInstallation(base::DictValue(), path));
  ASSERT_TRUE(component_install_dir_.Delete());
  EXPECT_FALSE(policy_->VerifyInstallation(base::DictValue(), path));
}

// Tests that installing the component updates the key pinning configuration in
// the network service.
TEST_F(PKIMetadataComponentInstallerKeyPinningTest,
       InstallComponentUpdatesPinningConfig) {
  // Initialize the network service and CertVerifierServiceFactory.
  content::GetNetworkService();
  content::GetCertVerifierServiceFactory();
  task_environment_.RunUntilIdle();

  // Check initial CertVerifierServiceFactory key_pinning_state. It should be
  // initialized to the compiled-in static unittest data.
  ExpectCertVerifierServiceUsesCompiledInPins();

  WriteKPConfigAndUpdate();

  {
    network::NetworkService* network_service =
        network::NetworkService::GetNetworkServiceForTesting();
    ASSERT_TRUE(network_service);
    EXPECT_TRUE(network_service->pins_list_updated());
    const std::vector<net::TransportSecurityState::PinSet>& pinsets =
        network_service->pinsets();
    const std::vector<net::TransportSecurityState::PinSetInfo>& host_pins =
        network_service->host_pins();
    EXPECT_EQ(pinsets.size(), 1u);
    EXPECT_EQ(host_pins.size(), 1u);

    EXPECT_EQ(pinsets.at(0).name(), kPinsetName);
    EXPECT_THAT(pinsets.at(0).static_spki_hashes(),
                ::testing::ElementsAre(kSpkiHash1));
    EXPECT_THAT(pinsets.at(0).bad_static_spki_hashes(),
                ::testing::ElementsAre(kSpkiHash2));

    EXPECT_EQ(host_pins.at(0).hostname_, kPinsetHostName);
    EXPECT_EQ(host_pins.at(0).pinset_name_, kPinsetName);
    EXPECT_EQ(host_pins.at(0).include_subdomains_, kPinsetIncludeSubdomains);
  }

  {
    cert_verifier::CertVerifierServiceFactoryImpl*
        cert_verifier_service_factory =
            content::GetCertVerifierServiceFactoryForTesting();
    ASSERT_TRUE(cert_verifier_service_factory);
    const net::CertVerifyProc::ImplParams& impl_params =
        cert_verifier_service_factory->get_impl_params();

    ASSERT_TRUE(impl_params.key_pinning_state);
    EXPECT_TRUE(
        impl_params.key_pinning_state->HasPublicKeyPins(kPinsetHostName));
    EXPECT_TRUE(impl_params.key_pinning_state->HasPublicKeyPins(
        std::string("subdomain.") + kPinsetHostName));
    EXPECT_EQ(net::KeyPinningState::PKPStatus::OK,
              impl_params.key_pinning_state->CheckPublicKeyPins(
                  kPinsetHostName,
                  /*is_issued_by_known_root=*/true, {kSpkiHash1}));
    EXPECT_EQ(net::KeyPinningState::PKPStatus::VIOLATED,
              impl_params.key_pinning_state->CheckPublicKeyPins(
                  kPinsetHostName,
                  /*is_issued_by_known_root=*/true, {kSpkiHash1, kSpkiHash2}));

    EXPECT_FALSE(impl_params.key_pinning_state->HasPublicKeyPins(
        kPinnedByDefaultHostname));
  }
}

// Tests that installing the PKI Metadata component bails out if the KP proto is
// invalid.
TEST_F(PKIMetadataComponentInstallerKeyPinningTest,
       InstallComponentInvalidKPProto) {
  // Initialize the network service and CertVerifierServiceFactory.
  content::GetNetworkService();
  content::GetCertVerifierServiceFactory();
  task_environment_.RunUntilIdle();

  // Write an invalid kp_pinslist.pb.
  ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
  base::FilePath file_path = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("kp_pinslist.pb"));
  ASSERT_TRUE(base::WriteFile(file_path, "mismatch"));

  KeyPinsListWaiter waiter;
  policy_->ComponentReady(base::Version("1.2.3.4"),
                          component_install_dir_.GetPath(), base::DictValue());
  waiter.Wait();

  network::NetworkService* network_service =
      network::NetworkService::GetNetworkServiceForTesting();
  ASSERT_TRUE(network_service);

  // The pins list should not have been updated.
  EXPECT_FALSE(network_service->pins_list_updated());
  const std::vector<net::TransportSecurityState::PinSet>& pinsets =
      network_service->pinsets();
  const std::vector<net::TransportSecurityState::PinSetInfo>& host_pins =
      network_service->host_pins();
  EXPECT_EQ(pinsets.size(), 0u);
  EXPECT_EQ(host_pins.size(), 0u);

  ExpectCertVerifierServiceUsesCompiledInPins();
}

// Tests that installing the PKI Metadata component does not update the pinning
// list if its compatibility version exceeds the value supported.
TEST_F(PKIMetadataComponentInstallerKeyPinningTest,
       InstallComponentIncompatibleKPVersion) {
  // Initialize the network service and CertVerifierServiceFactory.
  content::GetNetworkService();
  content::GetCertVerifierServiceFactory();
  task_environment_.RunUntilIdle();

  // Change the version to an unsupported value.
  pinlist_.set_compatibility_version(kMaxSupportedKPCompatibilityVersion + 1);

  WriteKPConfigAndUpdate();

  network::NetworkService* network_service =
      network::NetworkService::GetNetworkServiceForTesting();
  ASSERT_TRUE(network_service);

  // The pin list should not have been updated.
  EXPECT_FALSE(network_service->pins_list_updated());
  const std::vector<net::TransportSecurityState::PinSet>& pinsets =
      network_service->pinsets();
  const std::vector<net::TransportSecurityState::PinSetInfo>& host_pins =
      network_service->host_pins();
  EXPECT_EQ(pinsets.size(), 0u);
  EXPECT_EQ(host_pins.size(), 0u);

  ExpectCertVerifierServiceUsesCompiledInPins();
}

// Tests that installing the PKI Metadata component does not update the pinning
// list if the built in list is newer.
TEST_F(PKIMetadataComponentInstallerKeyPinningTest,
       InstallComponentKPListOlderThanBuiltIn) {
  // Initialize the network service and CertVerifierServiceFactory.
  content::GetNetworkService();
  content::GetCertVerifierServiceFactory();
  task_environment_.RunUntilIdle();

  // Change the timestamp so it's older than the built in list.
  pinlist_.mutable_timestamp()->set_seconds(
      (net::TransportSecurityState::GetBuiltInPinsListTimestamp()
           .InMillisecondsSinceUnixEpoch() /
       1000) -
      1);

  WriteKPConfigAndUpdate();

  network::NetworkService* network_service =
      network::NetworkService::GetNetworkServiceForTesting();
  ASSERT_TRUE(network_service);

  // The pin list should not have been updated.
  EXPECT_FALSE(network_service->pins_list_updated());
  EXPECT_EQ(network_service->pinsets().size(), 0u);
  EXPECT_EQ(network_service->host_pins().size(), 0u);

  ExpectCertVerifierServiceUsesCompiledInPins();
}
#endif  // BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED)

#if BUILDFLAG(IS_CT_SUPPORTED)
// Tests that installing the PKI Metadata component updates the CT configuration
// in the network service.
TEST_F(PKIMetadataComponentInstallerTest, InstallComponentUpdatesCTConfig) {
  // Initialize the network service.
  content::GetNetworkService();
  task_environment_.RunUntilIdle();

  WriteCTConfigToFile();
  CTLogListWaiter waiter;
  policy_->ComponentReady(base::Version("1.2.3.4"),
                          component_install_dir_.GetPath(), base::DictValue());
  waiter.Wait();

  network::NetworkService* network_service =
      network::NetworkService::GetNetworkServiceForTesting();
  ASSERT_TRUE(network_service);
  const std::vector<network::mojom::CTLogInfoPtr>& logs =
      network_service->log_list();
  ASSERT_EQ(logs.size(), 2u);
  std::string expected_log_id;
  ASSERT_TRUE(base::Base64Decode(kLogIdBase64, &expected_log_id));
  EXPECT_EQ(logs.at(0)->id, expected_log_id);
  std::string expected_public_key;
  ASSERT_TRUE(base::Base64Decode(kLogSPKIBase64, &expected_public_key));
  EXPECT_EQ(logs.at(0)->public_key, expected_public_key);
  EXPECT_EQ(logs.at(0)->name, kLogName);
  EXPECT_FALSE(logs.at(0)->disqualified_at);
  EXPECT_EQ(logs.at(0)->mmd, base::Seconds(kLogMMDSeconds));
  EXPECT_EQ(logs.at(0)->current_operator, kLogOperatorName);

  EXPECT_EQ(logs.at(1)->id, expected_log_id);
  EXPECT_EQ(logs.at(1)->public_key, expected_public_key);
  EXPECT_EQ(logs.at(1)->name, kGoogleLogName);
  EXPECT_EQ(*logs.at(1)->disqualified_at,
            base::Time::UnixEpoch() + kGoogleLogDisqualificationDate);
  EXPECT_EQ(logs.at(1)->mmd, base::Seconds(kLogMMDSeconds));
  EXPECT_EQ(logs.at(1)->current_operator, kGoogleLogOperatorName);

  // Previous operators should be sorted in the opposite order as in the proto.
  std::vector<network::mojom::PreviousOperatorEntryPtr>& previous_operators =
      logs.at(0)->previous_operators;
  ASSERT_EQ(previous_operators.size(), 2u);
  EXPECT_EQ(previous_operators.at(0)->name, kPreviousOperator2Name);
  EXPECT_EQ(previous_operators.at(0)->end_time,
            base::Time::UnixEpoch() + kPreviousOperator1Start);
  EXPECT_EQ(previous_operators.at(1)->name, kPreviousOperator1Name);
  EXPECT_EQ(previous_operators.at(1)->end_time,
            base::Time::UnixEpoch() + kCurrentOperatorStart);

  network::SCTAuditingCache* cache = network_service->sct_auditing_cache();
  EXPECT_TRUE(cache->IsPopularSCT(*base::Base64Decode(kPopularSCT1)));
  EXPECT_TRUE(cache->IsPopularSCT(*base::Base64Decode(kPopularSCT2)));
  EXPECT_FALSE(cache->IsPopularSCT(std::vector<uint8_t>{1, 2, 3, 4}));

  EXPECT_TRUE(network_service->is_ct_enforcement_enabled_for_testing());

  cert_verifier::CertVerifierServiceFactoryImpl* cert_verifier_service_factory =
      content::GetCertVerifierServiceFactoryForTesting();
  ASSERT_TRUE(cert_verifier_service_factory);
  const net::CertVerifyProc::ImplParams& impl_params =
      cert_verifier_service_factory->get_impl_params();
  ASSERT_EQ(impl_params.ct_logs.size(), 2u);
  EXPECT_EQ(impl_params.ct_logs[0]->key_id(), expected_log_id);
  EXPECT_EQ(impl_params.ct_logs[0]->description(), kLogName);
  EXPECT_EQ(impl_params.ct_logs[1]->key_id(), expected_log_id);
  EXPECT_EQ(impl_params.ct_logs[1]->description(), kGoogleLogName);
}

// Tests that installing the PKI Metadata component bails out if the CT proto is
// invalid.
TEST_F(PKIMetadataComponentInstallerTest, InstallComponentInvalidCTProto) {
  // Initialize the network service and cert verifier service factory.
  content::GetNetworkService();
  content::GetCertVerifierServiceFactory();

  // Write an invalid ct_config.pb.
  ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
  base::FilePath file_path = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("ct_config.pb"));
  ASSERT_TRUE(base::WriteFile(file_path, "mismatch"));

  CTLogListWaiter waiter;
  policy_->ComponentReady(base::Version("1.2.3.4"),
                          component_install_dir_.GetPath(), base::DictValue());
  waiter.Wait();

  network::NetworkService* network_service =
      network::NetworkService::GetNetworkServiceForTesting();
  ASSERT_TRUE(network_service);

  // The logs should not have been updated.
  const std::vector<network::mojom::CTLogInfoPtr>& logs =
      network_service->log_list();
  EXPECT_EQ(logs.size(), 0u);
  EXPECT_TRUE(network_service->is_ct_enforcement_enabled_for_testing());

  cert_verifier::CertVerifierServiceFactoryImpl* cert_verifier_service_factory =
      content::GetCertVerifierServiceFactoryForTesting();
  ASSERT_TRUE(cert_verifier_service_factory);
  const net::CertVerifyProc::ImplParams& impl_params =
      cert_verifier_service_factory->get_impl_params();
  EXPECT_EQ(impl_params.ct_logs.size(), 0u);
}

// Tests that installing the PKI Metadata component does not update the CT log
// list if its compatibility version exceeds the value supported.
TEST_F(PKIMetadataComponentInstallerTest,
       InstallComponentIncompatibleCTVersion) {
  // Initialize the network service and cert verifier service factory.
  content::GetNetworkService();
  content::GetCertVerifierServiceFactory();
  task_environment_.RunUntilIdle();

  // Change the version to an unsupported values.
  ct_config_.mutable_log_list()->set_compatibility_version(
      kMaxSupportedCTCompatibilityVersion + 1);
  WriteCTConfigToFile();

  CTLogListWaiter waiter;
  policy_->ComponentReady(base::Version("1.2.3.4"),
                          component_install_dir_.GetPath(), base::DictValue());
  waiter.Wait();

  network::NetworkService* network_service =
      network::NetworkService::GetNetworkServiceForTesting();
  ASSERT_TRUE(network_service);

  // The logs should not have been updated.
  const std::vector<network::mojom::CTLogInfoPtr>& logs =
      network_service->log_list();
  EXPECT_EQ(logs.size(), 0u);
  EXPECT_TRUE(network_service->is_ct_enforcement_enabled_for_testing());

  cert_verifier::CertVerifierServiceFactoryImpl* cert_verifier_service_factory =
      content::GetCertVerifierServiceFactoryForTesting();
  ASSERT_TRUE(cert_verifier_service_factory);
  const net::CertVerifyProc::ImplParams& impl_params =
      cert_verifier_service_factory->get_impl_params();
  EXPECT_EQ(impl_params.ct_logs.size(), 0u);
}

// Tests that installing the PKI Metadata component does not update the CT log
// list if the log list it includes is older than the built in one.
TEST_F(PKIMetadataComponentInstallerTest,
       InstallComponentCTListOlderThanBuiltIn) {
  // Initialize the network service and cert verifier service factory.
  content::GetNetworkService();
  content::GetCertVerifierServiceFactory();
  task_environment_.RunUntilIdle();

  // Change the timestamp so it is older than the built in list.
  ct_config_.mutable_log_list()->mutable_timestamp()->set_seconds(
      (certificate_transparency::GetLogListTimestamp()
           .InMillisecondsSinceUnixEpoch() /
       1000) -
      1);
  WriteCTConfigToFile();

  CTLogListWaiter waiter;
  policy_->ComponentReady(base::Version("1.2.3.4"),
                          component_install_dir_.GetPath(), base::DictValue());
  waiter.Wait();

  network::NetworkService* network_service =
      network::NetworkService::GetNetworkServiceForTesting();
  ASSERT_TRUE(network_service);

  // The logs should not have been updated.
  EXPECT_EQ(network_service->log_list().size(), 0u);
  EXPECT_TRUE(network_service->is_ct_enforcement_enabled_for_testing());

  cert_verifier::CertVerifierServiceFactoryImpl* cert_verifier_service_factory =
      content::GetCertVerifierServiceFactoryForTesting();
  ASSERT_TRUE(cert_verifier_service_factory);
  EXPECT_EQ(cert_verifier_service_factory->get_impl_params().ct_logs.size(),
            0u);
}

// Tests that calling |ReconfigureAfterNetworkRestart| is a no-op if the
// component has not been installed.
TEST_F(PKIMetadataComponentInstallerTest, ReconfigureWhenNotInstalled) {
  // Initialize the network service and cert verifier service factory.
  content::GetNetworkService();
  content::GetCertVerifierServiceFactory();
  task_environment_.RunUntilIdle();

  PKIMetadataComponentInstallerService::GetInstance()
      ->ReconfigureAfterNetworkRestart();

  // The logs should not have been updated.
  network::NetworkService* network_service =
      network::NetworkService::GetNetworkServiceForTesting();
  ASSERT_TRUE(network_service);
  const std::vector<network::mojom::CTLogInfoPtr>& logs =
      network_service->log_list();
  EXPECT_EQ(logs.size(), 0u);

  cert_verifier::CertVerifierServiceFactoryImpl* cert_verifier_service_factory =
      content::GetCertVerifierServiceFactoryForTesting();
  ASSERT_TRUE(cert_verifier_service_factory);
  const net::CertVerifyProc::ImplParams& impl_params =
      cert_verifier_service_factory->get_impl_params();
  EXPECT_EQ(impl_params.ct_logs.size(), 0u);
}
#endif  // BUILDFLAG(IS_CT_SUPPORTED)

class PKIMetadataComponentInstallerDisabledTest
    : public PKIMetadataComponentInstallerTest {
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            features::kCertificateTransparencyAskBeforeEnabling});
  }
};

// Tests that the PKI Metadata component does not get registered if both the CT
// component updater and KP component updater features are disabled.
TEST_F(PKIMetadataComponentInstallerDisabledTest,
       MaybeDoNotRegisterIfFeatureDisabled) {
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  // If Chrome Root Store is supported on this build config, PKI metadata
  // component will always be registered even if the other feature flags are
  // disabled.
  EXPECT_CALL(mock_component_update_, RegisterComponent)

      .WillOnce(testing::Return(true));
#else
  EXPECT_CALL(mock_component_update_, RegisterComponent).Times(0);
#endif
  component_updater::MaybeRegisterPKIMetadataComponent(&mock_component_update_);
  task_environment_.RunUntilIdle();
}

}  // namespace component_updater
