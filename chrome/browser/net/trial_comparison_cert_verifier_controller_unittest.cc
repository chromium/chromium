// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/trial_comparison_cert_verifier_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_test_utils.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/cert_logger.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/net_buildflags.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "services/cert_verifier/public/mojom/trial_comparison_cert_verifier.mojom.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using certificate_reporting_test_utils::CertificateReportingServiceTestHelper;
using certificate_reporting_test_utils::ReportExpectation;
using certificate_reporting_test_utils::RetryStatus;
using net::test::IsError;
using testing::_;
using testing::Mock;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace {

MATCHER_P(CertChainMatches, expected_cert, "") {
  net::CertificateList actual_certs =
      net::X509Certificate::CreateCertificateListFromBytes(
          base::as_bytes(base::make_span(arg)),
          net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  if (actual_certs.empty()) {
    *result_listener << "failed to parse arg";
    return false;
  }
  std::vector<std::string> actual_der_certs;
  for (const auto& cert : actual_certs) {
    actual_der_certs.emplace_back(
        net::x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()));
  }

  std::vector<std::string> expected_der_certs;
  expected_der_certs.emplace_back(
      net::x509_util::CryptoBufferAsStringPiece(expected_cert->cert_buffer()));
  for (const auto& buffer : expected_cert->intermediate_buffers()) {
    expected_der_certs.emplace_back(
        net::x509_util::CryptoBufferAsStringPiece(buffer.get()));
  }

  return actual_der_certs == expected_der_certs;
}

}  // namespace

class MockTrialComparisonCertVerifierConfigClient
    : public cert_verifier::mojom::TrialComparisonCertVerifierConfigClient {
 public:
  MockTrialComparisonCertVerifierConfigClient(
      mojo::PendingReceiver<
          cert_verifier::mojom::TrialComparisonCertVerifierConfigClient>
          config_client_receiver)
      : receiver_(this, std::move(config_client_receiver)) {}

  MOCK_METHOD1(OnTrialConfigUpdated, void(bool allowed));

 private:
  mojo::Receiver<cert_verifier::mojom::TrialComparisonCertVerifierConfigClient>
      receiver_;
};

class TrialComparisonCertVerifierControllerTest
    : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    cert_chain_1_ = CreateCertificateChainFromFile(
        net::GetTestCertsDirectory(), "multi-root-chain1.pem",
        net::X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(cert_chain_1_);
    leaf_cert_1_ = net::X509Certificate::CreateFromBuffer(
        bssl::UpRef(cert_chain_1_->cert_buffer()), {});
    ASSERT_TRUE(leaf_cert_1_);
    cert_chain_2_ = CreateCertificateChainFromFile(
        net::GetTestCertsDirectory(), "multi-root-chain2.pem",
        net::X509Certificate::FORMAT_AUTO);
    ASSERT_TRUE(cert_chain_2_);

    ok_result_.verified_cert = cert_chain_1_;
    bad_result_.verified_cert = cert_chain_2_;
    bad_result_.cert_status = net::CERT_STATUS_DATE_INVALID;

    reporting_service_test_helper_ =
        base::MakeRefCounted<CertificateReportingServiceTestHelper>();
    CertificateReportingServiceFactory::GetInstance()
        ->SetReportEncryptionParamsForTesting(
            reporting_service_test_helper()->server_public_key(),
            reporting_service_test_helper()->server_public_key_version());
    CertificateReportingServiceFactory::GetInstance()
        ->SetURLLoaderFactoryForTesting(reporting_service_test_helper_);
    reporting_service_test_helper()->SetFailureMode(
        certificate_reporting_test_utils::REPORTS_SUCCESSFUL);

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    ASSERT_TRUE(g_browser_process->profile_manager());

    sb_service_ =
        base::MakeRefCounted<safe_browsing::TestSafeBrowsingService>();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        sb_service_.get());
    g_browser_process->safe_browsing_service()->Initialize();

    // SafeBrowsingService expects to be initialized before any profiles are
    // created.
    profile_ = profile_manager_->CreateTestingProfile("profile1");

    // Initialize CertificateReportingService for |profile_|.
    ASSERT_TRUE(reporting_service());
    base::RunLoop().RunUntilIdle();
  }

  void SetExtendedReportingPref(bool enabled) {
    if (GetParam()) {
      // SetEnhancedProtectionPrefForTests sets both kSafeBrowsingEnabled and
      // kSafeBrowsingEnhanced.
      safe_browsing::SetEnhancedProtectionPrefForTests(pref_service(), enabled);
    } else {
      // SetExtendedReportingPrefForTests only sets
      // kSafeBrowsingScoutReportingEnabled, so first set kSafeBrowsingEnabled.
      // Keeping this consistent between the branches makes the test conditions
      // easier to write.
      safe_browsing::SetStandardProtectionPref(pref_service(), enabled);
      safe_browsing::SetExtendedReportingPrefForTests(pref_service(), enabled);
    }
  }

  void CreateController(Profile* profile) {
    mojo::PendingRemote<
        cert_verifier::mojom::TrialComparisonCertVerifierConfigClient>
        config_client;
    auto config_client_receiver =
        config_client.InitWithNewPipeAndPassReceiver();

    trial_controller_ =
        std::make_unique<TrialComparisonCertVerifierController>(profile);
    trial_controller_->AddClient(std::move(config_client),
                                 report_client_.BindNewPipeAndPassReceiver());

    mock_config_client_ = std::make_unique<
        StrictMock<MockTrialComparisonCertVerifierConfigClient>>(
        std::move(config_client_receiver));
  }

  void CreateController() { CreateController(profile()); }

  void TearDown() override {
    // Ensure any in-flight mojo calls get run.
    base::RunLoop().RunUntilIdle();

    // Ensure mock expectations are checked.
    mock_config_client_ = nullptr;

    if (TestingBrowserProcess::GetGlobal()->safe_browsing_service()) {
      TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();
      TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    }

    TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(
        false);
  }

  TestingProfile* profile() { return profile_; }
  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return profile_->GetTestingPrefService();
  }
  TrialComparisonCertVerifierController& trial_controller() {
    return *trial_controller_;
  }
  cert_verifier::mojom::TrialComparisonCertVerifierReportClient*
  report_client() {
    return report_client_.get();
  }
  MockTrialComparisonCertVerifierConfigClient& mock_config_client() {
    return *mock_config_client_;
  }
  CertificateReportingServiceTestHelper* reporting_service_test_helper() {
    return reporting_service_test_helper_.get();
  }
  CertificateReportingService* reporting_service() const {
    return CertificateReportingServiceFactory::GetForBrowserContext(profile_);
  }

 protected:
  scoped_refptr<net::X509Certificate> cert_chain_1_;
  scoped_refptr<net::X509Certificate> cert_chain_2_;
  scoped_refptr<net::X509Certificate> leaf_cert_1_;
  net::CertVerifyResult ok_result_;
  net::CertVerifyResult bad_result_;

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_;

 private:
  scoped_refptr<CertificateReportingServiceTestHelper>
      reporting_service_test_helper_;
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<TestingProfile> profile_;

  mojo::Remote<cert_verifier::mojom::TrialComparisonCertVerifierReportClient>
      report_client_;
  std::unique_ptr<TrialComparisonCertVerifierController> trial_controller_;
  std::unique_ptr<StrictMock<MockTrialComparisonCertVerifierConfigClient>>
      mock_config_client_;
};

TEST_P(TrialComparisonCertVerifierControllerTest, NothingEnabled) {
  CreateController();

  // Trial should not be allowed.
  EXPECT_FALSE(trial_controller().IsAllowed());

  // Enable the SBER pref, shouldn't matter since it's a non-official build and
  // field trial isn't enabled.
  SetExtendedReportingPref(true);

  // Trial still not allowed, and OnTrialConfigUpdated should not be called
  // either.
  EXPECT_FALSE(trial_controller().IsAllowed());

  // Attempting to send a report should also do nothing.
  report_client()->SendTrialReport(
      "hostname", leaf_cert_1_, false, false, false, false,
      std::vector<uint8_t>(), std::vector<uint8_t>(), ok_result_, ok_result_,
      cert_verifier::mojom::CertVerifierDebugInfo::New());
  // Ensure any in-flight mojo calls get run.
  base::RunLoop().RunUntilIdle();
  // Expect no report since the trial is not allowed.
  reporting_service_test_helper()->ExpectNoRequests(reporting_service());
}

TEST_P(TrialComparisonCertVerifierControllerTest,
       OfficialBuildTrialNotEnabled) {
  TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(true);
  CreateController();

  EXPECT_FALSE(trial_controller().IsAllowed());
  SetExtendedReportingPref(true);

  // Trial still not allowed, and OnTrialConfigUpdated should not be called
  // either.
  EXPECT_FALSE(trial_controller().IsAllowed());

  // Attempting to send a report should do nothing.
  report_client()->SendTrialReport(
      "hostname", leaf_cert_1_, false, false, false, false,
      std::vector<uint8_t>(), std::vector<uint8_t>(), ok_result_, ok_result_,
      cert_verifier::mojom::CertVerifierDebugInfo::New());

  // Ensure any in-flight mojo calls get run.
  base::RunLoop().RunUntilIdle();

  // Expect no report since the trial is not allowed.
  reporting_service_test_helper()->ExpectNoRequests(reporting_service());
}

TEST_P(TrialComparisonCertVerifierControllerTest,
       NotOfficialBuildTrialEnabled) {
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
  if (base::FeatureList::IsEnabled(net::features::kChromeRootStoreUsed)) {
    // If ChromeRootStoreUsed feature is enabled by default,
    // TrialComparisonCertVerifier will not be allowed. It is not safe to
    // change the kChromeRootStoreUsed flag in unit_tests since multiple tests
    // run in the same process, and GetChromeCertVerifierServiceParams will
    // globally enforce a single configuration for the lifetime of the
    // process. Therefore just skip this test if CRS is enabled.
    GTEST_SKIP();
  }
#endif
  scoped_feature_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_->InitAndEnableFeature(
      net::features::kCertDualVerificationTrialFeature);
  CreateController();

  EXPECT_FALSE(trial_controller().IsAllowed());
#if defined(OFFICIAL_BUILD) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // In a real official build, expect the trial config to be updated.
  EXPECT_CALL(mock_config_client(), OnTrialConfigUpdated(false)).Times(1);
  EXPECT_CALL(mock_config_client(), OnTrialConfigUpdated(true)).Times(1);
#endif
  SetExtendedReportingPref(true);

#if defined(OFFICIAL_BUILD) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // In a real official build, expect the trial to be allowed now.  (Don't
  // need to test sending reports here, since that'll be tested by
  // OfficialBuildTrialEnabled.)
  EXPECT_TRUE(trial_controller().IsAllowed());
#else
  // Trial still not allowed, and OnTrialConfigUpdated should not be called
  // either.
  EXPECT_FALSE(trial_controller().IsAllowed());

  // Attempting to send a report should do nothing.
  report_client()->SendTrialReport(
      "hostname", leaf_cert_1_, false, false, false, false,
      std::vector<uint8_t>(), std::vector<uint8_t>(), ok_result_, ok_result_,
      cert_verifier::mojom::CertVerifierDebugInfo::New());

  // Ensure any in-flight mojo calls get run.
  base::RunLoop().RunUntilIdle();

  // Expect no report since the trial is not allowed.
  reporting_service_test_helper()->ExpectNoRequests(reporting_service());
#endif
}

TEST_P(TrialComparisonCertVerifierControllerTest, OfficialBuildTrialEnabled) {
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
  if (base::FeatureList::IsEnabled(net::features::kChromeRootStoreUsed)) {
    // If ChromeRootStoreUsed feature is enabled by default,
    // TrialComparisonCertVerifier will not be allowed. It is not safe to
    // change the kChromeRootStoreUsed flag in unit_tests since multiple tests
    // run in the same process, and GetChromeCertVerifierServiceParams will
    // globally enforce a single configuration for the lifetime of the
    // process. Therefore just skip this test if CRS is enabled.
    GTEST_SKIP();
  }
#endif
  TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(true);
  scoped_feature_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_->InitAndEnableFeature(
      net::features::kCertDualVerificationTrialFeature);
  CreateController();

  EXPECT_FALSE(trial_controller().IsAllowed());

  // Enable the SBER pref, which should trigger the OnTrialConfigUpdated
  // callback.
  EXPECT_CALL(mock_config_client(), OnTrialConfigUpdated(false)).Times(1);
  EXPECT_CALL(mock_config_client(), OnTrialConfigUpdated(true)).Times(1);
  SetExtendedReportingPref(true);

  // Trial should now be allowed.
  EXPECT_TRUE(trial_controller().IsAllowed());
  // Ensure any in-flight mojo calls get run.
  base::RunLoop().RunUntilIdle();
  // OnTrialConfigUpdated should have been called.
  Mock::VerifyAndClear(&mock_config_client());

  // Report should be sent.
  report_client()->SendTrialReport(
      "127.0.0.1", leaf_cert_1_, false, false, false, false,
      std::vector<uint8_t>{4, 5, 6}, std::vector<uint8_t>{7, 8, 9}, ok_result_,
      bad_result_, cert_verifier::mojom::CertVerifierDebugInfo::New());

  // Ensure any in-flight mojo calls get run.
  base::RunLoop().RunUntilIdle();

  // Expect a report.
  std::vector<std::string> full_reports;
  reporting_service_test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"127.0.0.1", RetryStatus::NOT_RETRIED}}),
      &full_reports, nullptr);

  ASSERT_EQ(1U, full_reports.size());

  chrome_browser_ssl::CertLoggerRequest report;
  ASSERT_TRUE(report.ParseFromString(full_reports[0]));

  EXPECT_EQ(0, report.cert_error_size());
  EXPECT_EQ(0, report.cert_status_size());

  ASSERT_TRUE(report.has_features_info());
  ASSERT_TRUE(report.features_info().has_trial_verification_info());
  const chrome_browser_ssl::TrialVerificationInfo& trial_info =
      report.features_info().trial_verification_info();
  ASSERT_EQ(1, trial_info.cert_error_size());
  EXPECT_EQ(chrome_browser_ssl::CertLoggerRequest::ERR_CERT_DATE_INVALID,
            trial_info.cert_error()[0]);
  EXPECT_EQ(0, trial_info.cert_status_size());

  EXPECT_THAT(report.unverified_cert_chain(), CertChainMatches(leaf_cert_1_));
  EXPECT_THAT(report.cert_chain(), CertChainMatches(cert_chain_1_));
  EXPECT_THAT(trial_info.cert_chain(), CertChainMatches(cert_chain_2_));
  ASSERT_TRUE(trial_info.has_stapled_ocsp());
  EXPECT_EQ("\x04\x05\x06", trial_info.stapled_ocsp());
  ASSERT_TRUE(trial_info.has_sct_list());
  EXPECT_EQ("\x07\x08\x09", trial_info.sct_list());

  // Disable the SBER pref again, which should trigger the OnTrialConfigUpdated
  // callback.
  EXPECT_CALL(mock_config_client(), OnTrialConfigUpdated(false)).Times(2);
  SetExtendedReportingPref(false);

  // Not allowed now.
  EXPECT_FALSE(trial_controller().IsAllowed());

  // Attempting to send a report should do nothing now.
  report_client()->SendTrialReport(
      "hostname", leaf_cert_1_, false, false, false, false,
      std::vector<uint8_t>(), std::vector<uint8_t>(), ok_result_, bad_result_,
      cert_verifier::mojom::CertVerifierDebugInfo::New());
  // Ensure any in-flight mojo calls get run.
  base::RunLoop().RunUntilIdle();
  // Expect no report since the trial is not allowed.
  reporting_service_test_helper()->ExpectNoRequests(reporting_service());
}

TEST_P(TrialComparisonCertVerifierControllerTest,
       OfficialBuildTrialEnabledTwoClients) {
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
  if (base::FeatureList::IsEnabled(net::features::kChromeRootStoreUsed)) {
    // If ChromeRootStoreUsed feature is enabled by default,
    // TrialComparisonCertVerifier will not be allowed. It is not safe to
    // change the kChromeRootStoreUsed flag in unit_tests since multiple tests
    // run in the same process, and GetChromeCertVerifierServiceParams will
    // globally enforce a single configuration for the lifetime of the
    // process. Therefore just skip this test if CRS is enabled.
    GTEST_SKIP();
  }
#endif
  TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(true);
  scoped_feature_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_->InitAndEnableFeature(
      net::features::kCertDualVerificationTrialFeature);
  CreateController();

  mojo::Remote<cert_verifier::mojom::TrialComparisonCertVerifierReportClient>
      report_client_2;

  mojo::PendingRemote<
      cert_verifier::mojom::TrialComparisonCertVerifierConfigClient>
      config_client_2;
  auto config_client_2_receiver =
      config_client_2.InitWithNewPipeAndPassReceiver();

  trial_controller().AddClient(std::move(config_client_2),
                               report_client_2.BindNewPipeAndPassReceiver());

  StrictMock<MockTrialComparisonCertVerifierConfigClient> mock_config_client_2(
      std::move(config_client_2_receiver));

  EXPECT_FALSE(trial_controller().IsAllowed());

  // Enable the SBER pref, which should trigger the OnTrialConfigUpdated
  // callback.
  EXPECT_CALL(mock_config_client(), OnTrialConfigUpdated(false)).Times(1);
  EXPECT_CALL(mock_config_client(), OnTrialConfigUpdated(true)).Times(1);

  EXPECT_CALL(mock_config_client_2, OnTrialConfigUpdated(false)).Times(1);
  EXPECT_CALL(mock_config_client_2, OnTrialConfigUpdated(true)).Times(1);
  SetExtendedReportingPref(true);

  // Trial should now be allowed.
  EXPECT_TRUE(trial_controller().IsAllowed());
  // Ensure any in-flight mojo calls get run.
  base::RunLoop().RunUntilIdle();
  // OnTrialConfigUpdated should have been called.
  Mock::VerifyAndClear(&mock_config_client());
  Mock::VerifyAndClear(&mock_config_client_2);

  // Report should be sent.
  report_client()->SendTrialReport(
      "127.0.0.1", leaf_cert_1_, false, false, false, false,
      std::vector<uint8_t>(), std::vector<uint8_t>(), ok_result_, bad_result_,
      cert_verifier::mojom::CertVerifierDebugInfo::New());
  report_client_2->SendTrialReport(
      "127.0.0.2", leaf_cert_1_, false, false, false, false,
      std::vector<uint8_t>(), std::vector<uint8_t>(), ok_result_, bad_result_,
      cert_verifier::mojom::CertVerifierDebugInfo::New());

  // Ensure any in-flight mojo calls get run.
  base::RunLoop().RunUntilIdle();

  // Expect a report.
  std::vector<std::string> full_reports;
  reporting_service_test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"127.0.0.1", RetryStatus::NOT_RETRIED},
                                     {"127.0.0.2", RetryStatus::NOT_RETRIED}}),
      &full_reports, nullptr);

  ASSERT_EQ(2U, full_reports.size());

  chrome_browser_ssl::CertLoggerRequest report;
  for (size_t i = 0; i < 2; ++i) {
    ASSERT_TRUE(report.ParseFromString(full_reports[i]));

    EXPECT_EQ(0, report.cert_error_size());
    EXPECT_EQ(0, report.cert_status_size());

    ASSERT_TRUE(report.has_features_info());
    ASSERT_TRUE(report.features_info().has_trial_verification_info());
    const chrome_browser_ssl::TrialVerificationInfo& trial_info =
        report.features_info().trial_verification_info();
    ASSERT_EQ(1, trial_info.cert_error_size());
    EXPECT_EQ(chrome_browser_ssl::CertLoggerRequest::ERR_CERT_DATE_INVALID,
              trial_info.cert_error()[0]);
    EXPECT_EQ(0, trial_info.cert_status_size());

    EXPECT_THAT(report.unverified_cert_chain(), CertChainMatches(leaf_cert_1_));
    EXPECT_THAT(report.cert_chain(), CertChainMatches(cert_chain_1_));
    EXPECT_THAT(trial_info.cert_chain(), CertChainMatches(cert_chain_2_));

    EXPECT_FALSE(trial_info.has_stapled_ocsp());
    EXPECT_FALSE(trial_info.has_sct_list());
  }

  // Disable the SBER pref again, which should trigger the OnTrialConfigUpdated
  // callback.
  EXPECT_CALL(mock_config_client(), OnTrialConfigUpdated(false)).Times(2);
  EXPECT_CALL(mock_config_client_2, OnTrialConfigUpdated(false)).Times(2);
  SetExtendedReportingPref(false);

  // Not allowed now.
  EXPECT_FALSE(trial_controller().IsAllowed());

  // Attempting to send a report should do nothing now.
  report_client()->SendTrialReport(
      "hostname", leaf_cert_1_, false, false, false, false,
      std::vector<uint8_t>(), std::vector<uint8_t>(), ok_result_, bad_result_,
      cert_verifier::mojom::CertVerifierDebugInfo::New());
  report_client_2->SendTrialReport(
      "hostname2", leaf_cert_1_, false, false, false, false,
      std::vector<uint8_t>(), std::vector<uint8_t>(), ok_result_, bad_result_,
      cert_verifier::mojom::CertVerifierDebugInfo::New());
  // Ensure any in-flight mojo calls get run.
  base::RunLoop().RunUntilIdle();
  // Expect no report since the trial is not allowed.
  reporting_service_test_helper()->ExpectNoRequests(reporting_service());
}

TEST_P(TrialComparisonCertVerifierControllerTest,
       OfficialBuildTrialEnabledUmaOnly) {
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
  if (base::FeatureList::IsEnabled(net::features::kChromeRootStoreUsed)) {
    // If ChromeRootStoreUsed feature is enabled by default,
    // TrialComparisonCertVerifier will not be allowed. It is not safe to
    // change the kChromeRootStoreUsed flag in unit_tests since multiple tests
    // run in the same process, and GetChromeCertVerifierServiceParams will
    // globally enforce a single configuration for the lifetime of the
    // process. Therefore just skip this test if CRS is enabled.
    GTEST_SKIP();
  }
#endif
  TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(true);
  scoped_feature_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_->InitAndEnableFeatureWithParameters(
      net::features::kCertDualVerificationTrialFeature, {{"uma_only", "true"}});
  CreateController();

  EXPECT_FALSE(trial_controller().IsAllowed());

  // Enable the SBER pref, which should trigger the OnTrialConfigUpdated
  // callback.
  EXPECT_CALL(mock_config_client(), OnTrialConfigUpdated(false)).Times(1);
  EXPECT_CALL(mock_config_client(), OnTrialConfigUpdated(true)).Times(1);
  SetExtendedReportingPref(true);

  // Trial should now be allowed.
  EXPECT_TRUE(trial_controller().IsAllowed());
  // Ensure any in-flight mojo calls get run.
  base::RunLoop().RunUntilIdle();
  // OnTrialConfigUpdated should have been called.
  Mock::VerifyAndClear(&mock_config_client());

  // In uma_only mode, the network service will generate a report, but the
  // trial controller will not send it to the reporting service.
  report_client()->SendTrialReport(
      "127.0.0.1", leaf_cert_1_, false, false, false, false,
      std::vector<uint8_t>(), std::vector<uint8_t>(), ok_result_, bad_result_,
      cert_verifier::mojom::CertVerifierDebugInfo::New());

  // Ensure any in-flight mojo calls get run.
  base::RunLoop().RunUntilIdle();

  // Expect no reports in uma_only mode.
  reporting_service_test_helper()->ExpectNoRequests(reporting_service());
}

TEST_P(TrialComparisonCertVerifierControllerTest,
       IncognitoOfficialBuildTrialEnabled) {
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
  if (base::FeatureList::IsEnabled(net::features::kChromeRootStoreUsed)) {
    // If ChromeRootStoreUsed feature is enabled by default,
    // TrialComparisonCertVerifier will not be allowed. It is not safe to
    // change the kChromeRootStoreUsed flag in unit_tests since multiple tests
    // run in the same process, and GetChromeCertVerifierServiceParams will
    // globally enforce a single configuration for the lifetime of the
    // process. Therefore just skip this test if CRS is enabled.
    GTEST_SKIP();
  }
#endif
  TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(true);
  scoped_feature_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_->InitAndEnableFeature(
      net::features::kCertDualVerificationTrialFeature);
  CreateController(profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  EXPECT_FALSE(trial_controller().IsAllowed());

  // Enable the SBER pref, shouldn't matter since it's an incognito profile.
  SetExtendedReportingPref(true);

  // Trial still not allowed, and OnTrialConfigUpdated should not be called
  // either.
  EXPECT_FALSE(trial_controller().IsAllowed());

  // Attempting to send a report should also do nothing.
  report_client()->SendTrialReport(
      "hostname", leaf_cert_1_, false, false, false, false,
      std::vector<uint8_t>(), std::vector<uint8_t>(), ok_result_, ok_result_,
      cert_verifier::mojom::CertVerifierDebugInfo::New());
  // Ensure any in-flight mojo calls get run.
  base::RunLoop().RunUntilIdle();
  // Expect no report since the trial is not allowed.
  reporting_service_test_helper()->ExpectNoRequests(reporting_service());
}

INSTANTIATE_TEST_SUITE_P(Impl,
                         TrialComparisonCertVerifierControllerTest,
                         testing::Bool());
