// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pki_metadata_component_installer.h"

#include "services/network/public/cpp/network_service_buildflags.h"

#if BUILDFLAG(IS_CT_SUPPORTED)

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/scoped_feature_list.h"
#include "components/certificate_transparency/certificate_transparency_config.pb.h"
#include "components/certificate_transparency/ct_features.h"
#include "components/certificate_transparency/ct_known_logs.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/ct_log_info.mojom.h"
#include "services/network/sct_auditing/sct_auditing_cache.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace component_updater {

namespace {
// An arbitrary, DER-encoded subjectpublickeyinfo encoded as BASE64.
const char* kLogSPKIBase64 =
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
const char* kLogIdBase64 = "ASNFZ4mrze8QERITFBUWFxgZGhscHR4f";
constexpr uint64_t kLogMMDSeconds = 42;
const char* kLogURL = "https://futuregadgetlab.jp";
const char* kLogName = "FutureGadgetLog2022";
const char* kLogOperatorName = "Future Gadget Lab";
const char* kLogOperatorEmail = "kurisu@dmail.com";
constexpr base::TimeDelta kCurrentOperatorStart = base::Days(3);
const char* kPreviousOperator1Name = "SERN";
constexpr base::TimeDelta kPreviousOperator1Start = base::Days(2);
const char* kPreviousOperator2Name = "DURPA";
constexpr base::TimeDelta kPreviousOperator2Start = base::Days(1);
const char* kGoogleLogName = "GoogleLog2022";
const char* kGoogleLogOperatorName = "Google";
constexpr base::TimeDelta kGoogleLogDisqualificationDate = base::Days(2);

// BASE64 encoded fake leaf hashes.
const std::string kPopularSCT1 = "EBESExQVFhcYGRobHB0eHwEjRWeJq83v";
const std::string kPopularSCT2 = "oKGio6SlpqeoqaqrrK2urwEjRWeJq83v";

constexpr uint64_t kMaxSupportedCompatibilityVersion = 2;
}  // namespace

// TODO(crbug.com/1306559): add a test for disabling ct enforcement once
// crbug.com/1306559 is fixed.
class PKIMetadataComponentInstallerTest : public testing::Test {
 public:
  void SetUp() override {
    ct_config_.set_disable_ct_enforcement(false);
    ct_config_.mutable_log_list()->set_compatibility_version(
        kMaxSupportedCompatibilityVersion);
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

  void WriteCtConfigToFile() {
    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
    base::FilePath file_path = component_install_dir_.GetPath().Append(
        FILE_PATH_LITERAL("ct_config.pb"));
    std::string data;
    ASSERT_TRUE(ct_config_.SerializeToString(&data));
    ASSERT_TRUE(base::WriteFile(file_path, data));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      certificate_transparency::features::
          kCertificateTransparencyComponentUpdater};
  content::BrowserTaskEnvironment task_environment_;
  component_updater::MockComponentUpdateService mock_component_update_;
  base::ScopedTempDir component_install_dir_;

  chrome_browser_certificate_transparency::CTConfig ct_config_;
  std::unique_ptr<component_updater::ComponentInstallerPolicy> policy_ =
      std::make_unique<
          component_updater::PKIMetadataComponentInstallerPolicy>();
};

TEST_F(PKIMetadataComponentInstallerTest, TestProtoBytesConversion) {
  std::vector<std::vector<uint8_t>> test_bytes = {
      {0xec, 0x72, 0x29, 0x69, 0xcb, 0x64, 0x20, 0x0a, 0xb6, 0x63, 0x8f,
       0x68, 0xac, 0x53, 0x8e, 0x40, 0xab, 0xab, 0x5b, 0x19, 0xa6, 0x48,
       0x56, 0x61, 0x04, 0x2a, 0x10, 0x61, 0xc4, 0x61, 0x27, 0x76}};

  std::string bytes_as_string(&test_bytes[0][0], &test_bytes[0][0] + 32);
  std::vector<std::string> repeated_bytes = {bytes_as_string};

  EXPECT_EQ(PKIMetadataComponentInstallerPolicy::BytesArrayFromProtoBytes(
                google::protobuf::RepeatedPtrField<std::string>(
                    repeated_bytes.begin(), repeated_bytes.end())),
            test_bytes);
}

// Tests that the installation is verified iff the component install directory
// exists.
TEST_F(PKIMetadataComponentInstallerTest, VerifyInstallation) {
  WriteCtConfigToFile();
  base::FilePath path = component_install_dir_.GetPath();
  EXPECT_TRUE(policy_->VerifyInstallation(base::Value(), path));
  ASSERT_TRUE(component_install_dir_.Delete());
  EXPECT_FALSE(policy_->VerifyInstallation(base::Value(), path));
}

// Tests that the PKI Metadata component is registered if the feature is
// enabled.
TEST_F(PKIMetadataComponentInstallerTest, RegisterComponent) {
  EXPECT_CALL(mock_component_update_, RegisterComponent)
      .Times(1)
      .WillOnce(testing::Return(true));
  component_updater::MaybeRegisterPKIMetadataComponent(&mock_component_update_);
  task_environment_.RunUntilIdle();
}

// Tests that installing the PKI Metadata component updates the network service.
TEST_F(PKIMetadataComponentInstallerTest, InstallComponent) {
  // Initialize the network service.
  content::GetNetworkService();
  task_environment_.RunUntilIdle();

  WriteCtConfigToFile();
  policy_->ComponentReady(base::Version("1.2.3.4"),
                          component_install_dir_.GetPath(), base::Value());
  task_environment_.RunUntilIdle();

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
  EXPECT_FALSE(logs.at(0)->operated_by_google);
  EXPECT_FALSE(logs.at(0)->disqualified_at);
  EXPECT_EQ(logs.at(0)->mmd, base::Seconds(kLogMMDSeconds));
  EXPECT_EQ(logs.at(0)->current_operator, kLogOperatorName);

  EXPECT_EQ(logs.at(1)->id, expected_log_id);
  EXPECT_EQ(logs.at(1)->public_key, expected_public_key);
  EXPECT_EQ(logs.at(1)->name, kGoogleLogName);
  EXPECT_TRUE(logs.at(1)->operated_by_google);
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
  EXPECT_FALSE(cache->IsPopularSCT(std::vector<const uint8_t>{1, 2, 3, 4}));
}

// Tests that installing the PKI Metadata component bails out if the proto is
// invalid.
TEST_F(PKIMetadataComponentInstallerTest, InstallComponentInvalidProto) {
  // Initialize the network service.
  content::GetNetworkService();
  task_environment_.RunUntilIdle();

  // Write an invalid ct_config.pb.
  ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
  base::FilePath file_path = component_install_dir_.GetPath().Append(
      FILE_PATH_LITERAL("ct_config.pb"));
  ASSERT_TRUE(base::WriteFile(file_path, "mismatch"));

  policy_->ComponentReady(base::Version("1.2.3.4"),
                          component_install_dir_.GetPath(), base::Value());
  task_environment_.RunUntilIdle();

  network::NetworkService* network_service =
      network::NetworkService::GetNetworkServiceForTesting();
  ASSERT_TRUE(network_service);

  // The logs should not have been updated.
  const std::vector<network::mojom::CTLogInfoPtr>& logs =
      network_service->log_list();
  EXPECT_EQ(logs.size(), 0u);
}

// Tests that installing the PKI Metadata component bails out if the CT
// compatibility version exceeds the value supported.
TEST_F(PKIMetadataComponentInstallerTest, InstallComponentIncompatibleVersion) {
  // Initialize the network service.
  content::GetNetworkService();
  task_environment_.RunUntilIdle();

  // Change the version to an unsupported value.
  ct_config_.mutable_log_list()->set_compatibility_version(
      kMaxSupportedCompatibilityVersion + 1);
  WriteCtConfigToFile();

  policy_->ComponentReady(base::Version("1.2.3.4"),
                          component_install_dir_.GetPath(), base::Value());
  task_environment_.RunUntilIdle();

  network::NetworkService* network_service =
      network::NetworkService::GetNetworkServiceForTesting();
  ASSERT_TRUE(network_service);

  // The logs should not have been updated.
  const std::vector<network::mojom::CTLogInfoPtr>& logs =
      network_service->log_list();
  EXPECT_EQ(logs.size(), 0u);
}

class PKIMetadataComponentInstallerDisabledTest
    : public PKIMetadataComponentInstallerTest {
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        certificate_transparency::features::
            kCertificateTransparencyComponentUpdater);
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the PKI Metadata component does not get registered if the feature
// is disabled.
TEST_F(PKIMetadataComponentInstallerDisabledTest,
       DoNotRegisterIfFeatureDisabled) {
  EXPECT_CALL(mock_component_update_, RegisterComponent).Times(0);
  component_updater::MaybeRegisterPKIMetadataComponent(&mock_component_update_);
  task_environment_.RunUntilIdle();
}

}  // namespace component_updater

#endif  // BUILDFLAG(IS_CT_SUPPORTED)
