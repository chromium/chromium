// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/trial_comparison_cert_verifier.h"

#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_test_utils.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/ssl/cert_logger.pb.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/socket_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using certificate_reporting_test_utils::CertificateReportingServiceTestHelper;
using certificate_reporting_test_utils::ReportExpectation;
using certificate_reporting_test_utils::RetryStatus;
using net::test::IsError;
using testing::_;
using testing::Return;
using testing::SetArgPointee;

namespace {

MATCHER_P(CertChainMatches, expected_cert, "") {
  net::CertificateList actual_certs =
      net::X509Certificate::CreateCertificateListFromBytes(
          arg.data(), arg.size(),
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

// Fake CertVerifyProc that sets the CertVerifyResult to a given value for
// all certificates that are Verify()'d
class FakeCertVerifyProc : public net::CertVerifyProc {
 public:
  FakeCertVerifyProc(const int result_error,
                     const net::CertVerifyResult& result)
      : result_error_(result_error), result_(result) {}

  void WaitForVerifyCall() {
    verify_called_.WaitForResult();
    // Ensure MultiThreadedCertVerifier OnJobCompleted task has a chance to run.
    content::RunAllTasksUntilIdle();
  }

  // CertVerifyProc implementation:
  bool SupportsAdditionalTrustAnchors() const override { return false; }

 protected:
  ~FakeCertVerifyProc() override = default;

 private:
  int VerifyInternal(net::X509Certificate* cert,
                     const std::string& hostname,
                     const std::string& ocsp_response,
                     int flags,
                     net::CRLSet* crl_set,
                     const net::CertificateList& additional_trust_anchors,
                     net::CertVerifyResult* verify_result) override;

  const int result_error_;
  const net::CertVerifyResult result_;
  net::TestClosure verify_called_;

  DISALLOW_COPY_AND_ASSIGN(FakeCertVerifyProc);
};

int FakeCertVerifyProc::VerifyInternal(
    net::X509Certificate* cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    int flags,
    net::CRLSet* crl_set,
    const net::CertificateList& additional_trust_anchors,
    net::CertVerifyResult* verify_result) {
  *verify_result = result_;
  base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::UI})
      ->PostTask(FROM_HERE, verify_called_.closure());
  return result_error_;
}

// Fake CertVerifyProc that causes a failure if it is called.
class NotCalledCertVerifyProc : public net::CertVerifyProc {
 public:
  NotCalledCertVerifyProc() = default;

  // CertVerifyProc implementation:
  bool SupportsAdditionalTrustAnchors() const override { return false; }

 protected:
  ~NotCalledCertVerifyProc() override = default;

 private:
  int VerifyInternal(net::X509Certificate* cert,
                     const std::string& hostname,
                     const std::string& ocsp_response,
                     int flags,
                     net::CRLSet* crl_set,
                     const net::CertificateList& additional_trust_anchors,
                     net::CertVerifyResult* verify_result) override;

  DISALLOW_COPY_AND_ASSIGN(NotCalledCertVerifyProc);
};

int NotCalledCertVerifyProc::VerifyInternal(
    net::X509Certificate* cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    int flags,
    net::CRLSet* crl_set,
    const net::CertificateList& additional_trust_anchors,
    net::CertVerifyResult* verify_result) {
  ADD_FAILURE() << "NotCalledCertVerifyProc was called!";
  return net::ERR_UNEXPECTED;
}

void NotCalledCallback(int error) {
  ADD_FAILURE() << "NotCalledCallback was called with error code " << error;
}

class MockCertVerifyProc : public net::CertVerifyProc {
 public:
  MockCertVerifyProc() = default;
  // CertVerifyProc implementation:
  bool SupportsAdditionalTrustAnchors() const override { return false; }
  MOCK_METHOD7(VerifyInternal,
               int(net::X509Certificate* cert,
                   const std::string& hostname,
                   const std::string& ocsp_response,
                   int flags,
                   net::CRLSet* crl_set,
                   const net::CertificateList& additional_trust_anchors,
                   net::CertVerifyResult* verify_result));

 protected:
  ~MockCertVerifyProc() override = default;

  DISALLOW_COPY_AND_ASSIGN(MockCertVerifyProc);
};

}  // namespace

class TrialComparisonCertVerifierTest : public testing::Test {
 public:
  TrialComparisonCertVerifierTest()
      // UI and IO message loops run on the same thread for the test. (Makes
      // the test logic simpler, though doesn't fully exercise the
      // ThreadCheckers.)
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

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

    system_request_context_getter_ =
        base::MakeRefCounted<net::TestURLRequestContextGetter>(
            base::CreateSingleThreadTaskRunnerWithTraits(
                {content::BrowserThread::IO}));
    TestingBrowserProcess::GetGlobal()->SetSystemRequestContext(
        system_request_context_getter_.get());
    sb_service_ =
        base::MakeRefCounted<safe_browsing::TestSafeBrowsingService>();
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        sb_service_.get());
    g_browser_process->safe_browsing_service()->Initialize();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    ASSERT_TRUE(g_browser_process->profile_manager());
    profile_ = profile_manager_->CreateTestingProfile("profile1");

    // Enable feature and SBER pref.
    TrialComparisonCertVerifier::SetFakeOfficialBuildForTesting();
    scoped_feature_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_->InitAndEnableFeature(
        features::kCertDualVerificationTrialFeature);
    safe_browsing::SetExtendedReportingPref(pref_service(), true);

    // Initialize CertificateReportingService
    ASSERT_TRUE(service());
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    if (TestingBrowserProcess::GetGlobal()->safe_browsing_service()) {
      TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();
      TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    }
    TestingBrowserProcess::GetGlobal()->SetSystemRequestContext(nullptr);
    system_request_context_getter_ = nullptr;
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  TestingProfile* profile() { return profile_; }
  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return profile_->GetTestingPrefService();
  }

  CertificateReportingServiceTestHelper* reporting_service_test_helper() {
    return reporting_service_test_helper_.get();
  }

  CertificateReportingService* service() const {
    return CertificateReportingServiceFactory::GetForBrowserContext(profile_);
  }

 protected:
  scoped_refptr<net::X509Certificate> cert_chain_1_;
  scoped_refptr<net::X509Certificate> cert_chain_2_;
  scoped_refptr<net::X509Certificate> leaf_cert_1_;
  base::HistogramTester histograms_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
  scoped_refptr<net::URLRequestContextGetter> system_request_context_getter_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;

  scoped_refptr<CertificateReportingServiceTestHelper>
      reporting_service_test_helper_;
};

TEST_F(TrialComparisonCertVerifierTest, NotOptedIn) {
  // Disable SBER pref.
  safe_browsing::SetExtendedReportingPref(pref_service(), false);

  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = cert_chain_1_;
  TrialComparisonCertVerifier verifier(
      profile(),
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, dummy_result),
      base::MakeRefCounted<NotCalledCertVerifyProc>());
  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::OK));

  // Wait for CheckTrialEligibility task to finish.
  content::RunAllTasksUntilIdle();

  // Expect no report.
  reporting_service_test_helper()->ExpectNoRequests(service());

  // Primary verifier should have ran, trial verifier should not have.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               0);
  histograms_.ExpectTotalCount("Net.CertVerifier_TrialComparisonResult", 0);
}

TEST_F(TrialComparisonCertVerifierTest, FeatureDisabled) {
  // Disable feature.
  scoped_feature_.reset();

  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = cert_chain_1_;
  TrialComparisonCertVerifier verifier(
      profile(),
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, dummy_result),
      base::MakeRefCounted<NotCalledCertVerifyProc>());
  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::OK));

  // Wait for CheckTrialEligibility task to finish.
  content::RunAllTasksUntilIdle();

  // Expect no report.
  reporting_service_test_helper()->ExpectNoRequests(service());

  // Primary verifier should have ran, trial verifier should not have.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               0);
  histograms_.ExpectTotalCount("Net.CertVerifier_TrialComparisonResult", 0);
}

TEST_F(TrialComparisonCertVerifierTest, SameResult) {
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, dummy_result);
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, dummy_result);

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::OK));

  verify_proc2->WaitForVerifyCall();

  // Expect no report.
  reporting_service_test_helper()->ExpectNoRequests(service());

  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample("Net.CertVerifier_TrialComparisonResult",
                                 TrialComparisonCertVerifier::kEqual, 1);
}

TEST_F(TrialComparisonCertVerifierTest, Incognito) {
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = cert_chain_1_;
  TrialComparisonCertVerifier verifier(
      profile()->GetOffTheRecordProfile(),  // Use an incognito Profile.
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, dummy_result),
      base::MakeRefCounted<NotCalledCertVerifyProc>());
  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::OK));

  // Wait for CheckTrialEligibility task to finish.
  content::RunAllTasksUntilIdle();

  // Primary verifier should have ran, trial verifier should not have, control
  // histogram also should not be recorded for incognito.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               0);
  histograms_.ExpectTotalCount("Net.CertVerifier_TrialComparisonResult", 0);
}

TEST_F(TrialComparisonCertVerifierTest, PrimaryVerifierErrorSecondaryOk) {
  // Primary verifier returns an error status.
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status = net::CERT_STATUS_DATE_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::ERR_CERT_DATE_INVALID,
                                               primary_result);

  net::CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, secondary_result);

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::ERR_CERT_DATE_INVALID));

  verify_proc2->WaitForVerifyCall();

  // Expect a report.
  std::vector<std::string> full_reports;
  reporting_service_test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"127.0.0.1", RetryStatus::NOT_RETRIED}}),
      &full_reports);

  ASSERT_EQ(1U, full_reports.size());

  chrome_browser_ssl::CertLoggerRequest report;
  ASSERT_TRUE(report.ParseFromString(full_reports[0]));

  ASSERT_EQ(1, report.cert_error_size());
  EXPECT_EQ(chrome_browser_ssl::CertLoggerRequest::ERR_CERT_DATE_INVALID,
            report.cert_error()[0]);
  EXPECT_EQ(0, report.cert_status_size());

  ASSERT_TRUE(report.has_features_info());
  ASSERT_TRUE(report.features_info().has_trial_verification_info());
  const chrome_browser_ssl::TrialVerificationInfo& trial_info =
      report.features_info().trial_verification_info();
  ASSERT_EQ(0, trial_info.cert_error_size());
  EXPECT_EQ(0, trial_info.cert_status_size());

  EXPECT_THAT(report.unverified_cert_chain(), CertChainMatches(leaf_cert_1_));
  EXPECT_THAT(report.cert_chain(), CertChainMatches(cert_chain_1_));
  EXPECT_THAT(trial_info.cert_chain(), CertChainMatches(cert_chain_1_));

  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kPrimaryErrorSecondaryValid, 1);
}

TEST_F(TrialComparisonCertVerifierTest, PrimaryVerifierOkSecondaryError) {
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, primary_result);

  // Trial verifier returns an error status.
  net::CertVerifyResult secondary_result;
  secondary_result.cert_status = net::CERT_STATUS_DATE_INVALID;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::ERR_CERT_DATE_INVALID,
                                               secondary_result);

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::OK));

  verify_proc2->WaitForVerifyCall();

  // Expect a report.
  std::vector<std::string> full_reports;
  reporting_service_test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"127.0.0.1", RetryStatus::NOT_RETRIED}}),
      &full_reports);

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
  EXPECT_THAT(trial_info.cert_chain(), CertChainMatches(cert_chain_1_));

  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kPrimaryValidSecondaryError, 1);
}

TEST_F(TrialComparisonCertVerifierTest, BothVerifiersDifferentErrors) {
  // Primary verifier returns an error status.
  net::CertVerifyResult primary_result;
  primary_result.cert_status = net::CERT_STATUS_VALIDITY_TOO_LONG;
  primary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::ERR_CERT_VALIDITY_TOO_LONG,
                                               primary_result);

  // Trial verifier returns a different error status.
  net::CertVerifyResult secondary_result;
  secondary_result.cert_status = net::CERT_STATUS_DATE_INVALID;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::ERR_CERT_DATE_INVALID,
                                               secondary_result);

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::ERR_CERT_VALIDITY_TOO_LONG));

  verify_proc2->WaitForVerifyCall();

  // Expect a report.
  std::vector<std::string> full_reports;
  reporting_service_test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"127.0.0.1", RetryStatus::NOT_RETRIED}}),
      &full_reports);

  ASSERT_EQ(1U, full_reports.size());

  chrome_browser_ssl::CertLoggerRequest report;
  ASSERT_TRUE(report.ParseFromString(full_reports[0]));

  ASSERT_EQ(1, report.cert_error_size());
  EXPECT_EQ(chrome_browser_ssl::CertLoggerRequest::ERR_CERT_VALIDITY_TOO_LONG,
            report.cert_error()[0]);
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
  EXPECT_THAT(trial_info.cert_chain(), CertChainMatches(cert_chain_1_));

  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kBothErrorDifferentDetails, 1);
}

TEST_F(TrialComparisonCertVerifierTest,
       BothVerifiersOkDifferentVerifiedChains) {
  // Primary verifier returns chain1 regardless of arguments.
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, primary_result);

  // Trial verifier returns a different verified cert chain.
  net::CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_2_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, secondary_result);

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::OK));

  verify_proc2->WaitForVerifyCall();

  // Expect a report.
  std::vector<std::string> full_reports;
  reporting_service_test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"127.0.0.1", RetryStatus::NOT_RETRIED}}),
      &full_reports);

  ASSERT_EQ(1U, full_reports.size());

  chrome_browser_ssl::CertLoggerRequest report;
  ASSERT_TRUE(report.ParseFromString(full_reports[0]));

  EXPECT_EQ(0, report.cert_error_size());
  EXPECT_EQ(0, report.cert_status_size());

  ASSERT_TRUE(report.has_features_info());
  ASSERT_TRUE(report.features_info().has_trial_verification_info());
  const chrome_browser_ssl::TrialVerificationInfo& trial_info =
      report.features_info().trial_verification_info();
  EXPECT_EQ(0, trial_info.cert_error_size());
  EXPECT_EQ(0, trial_info.cert_status_size());
  EXPECT_EQ(0, trial_info.verify_flags_size());

  EXPECT_THAT(report.unverified_cert_chain(), CertChainMatches(leaf_cert_1_));
  EXPECT_THAT(report.cert_chain(), CertChainMatches(cert_chain_1_));
  EXPECT_THAT(trial_info.cert_chain(), CertChainMatches(cert_chain_2_));

  // Main CertVerifier_Job_Latency should have 2 counts since the
  // primary_reverifier was used.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 2);
  // CertVerifier_Job_Latency_TrialPrimary only has 1 count since
  // primary_reverifier doesn't use the same CertVerifier.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kBothValidDifferentDetails, 1);
}

TEST_F(TrialComparisonCertVerifierTest,
       BothVerifiersOkDifferentVerifiedChainsEqualAfterReverification) {
  net::CertVerifyResult chain1_result;
  chain1_result.verified_cert = cert_chain_1_;
  net::CertVerifyResult chain2_result;
  chain2_result.verified_cert = cert_chain_2_;

  scoped_refptr<MockCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<MockCertVerifyProc>();
  // Primary verifier returns ok status and chain1 if verifying the leaf alone.
  EXPECT_CALL(*verify_proc1,
              VerifyInternal(leaf_cert_1_.get(), _, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<6>(chain1_result), Return(net::OK)));
  // Primary verifier returns ok status and chain2 if verifying chain2.
  EXPECT_CALL(*verify_proc1,
              VerifyInternal(cert_chain_2_.get(), _, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<6>(chain2_result), Return(net::OK)));

  // Trial verifier returns ok status and chain2.
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, chain2_result);

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::OK));

  verify_proc2->WaitForVerifyCall();

  // Expect no report.
  reporting_service_test_helper()->ExpectNoRequests(service());

  // Main CertVerifier_Job_Latency should have 2 counts since the
  // primary_reverifier was used.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 2);
  // CertVerifier_Job_Latency_TrialPrimary only has 1 count since
  // primary_reverifier doesn't use the same CertVerifier.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kIgnoredDifferentPathReVerifiesEquivalent,
      1);
}

TEST_F(TrialComparisonCertVerifierTest,
       DifferentVerifiedChainsIgnorableDifferenceAfterReverification) {
  base::FilePath certs_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &certs_dir));
  certs_dir = certs_dir.AppendASCII("net")
                  .AppendASCII("trial_comparison_cert_verifier_unittest")
                  .AppendASCII("target-multiple-policies");
  scoped_refptr<net::X509Certificate> cert_chain =
      CreateCertificateChainFromFile(certs_dir, "chain.pem",
                                     net::X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert_chain);
  ASSERT_EQ(2U, cert_chain->intermediate_buffers().size());

  scoped_refptr<net::X509Certificate> leaf =
      net::X509Certificate::CreateFromBuffer(
          bssl::UpRef(cert_chain->cert_buffer()), {});
  ASSERT_TRUE(leaf);

  // Chain with the same leaf and different root. This is not a valid chain, but
  // doesn't matter for the unittest since this uses mock CertVerifyProcs.
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(
      bssl::UpRef(cert_chain_1_->intermediate_buffers().back().get()));
  scoped_refptr<net::X509Certificate> different_chain =
      net::X509Certificate::CreateFromBuffer(
          bssl::UpRef(cert_chain->cert_buffer()), std::move(intermediates));
  ASSERT_TRUE(different_chain);

  net::CertVerifyResult different_chain_result;
  different_chain_result.verified_cert = different_chain;

  net::CertVerifyResult nonev_chain_result;
  nonev_chain_result.verified_cert = cert_chain;

  net::CertVerifyResult ev_chain_result;
  ev_chain_result.verified_cert = cert_chain;
  ev_chain_result.cert_status =
      net::CERT_STATUS_IS_EV | net::CERT_STATUS_REV_CHECKING_ENABLED;

  net::SHA256HashValue root_fingerprint;
  crypto::SHA256HashString(net::x509_util::CryptoBufferAsStringPiece(
                               cert_chain->intermediate_buffers().back().get()),
                           root_fingerprint.data,
                           sizeof(root_fingerprint.data));
  // Both policies in the target are EV policies, but only 1.2.6.7 is valid for
  // the root in cert_chain.
  net::ScopedTestEVPolicy scoped_ev_policy_1(
      net::EVRootCAMetadata::GetInstance(), root_fingerprint, "1.2.6.7");
  net::ScopedTestEVPolicy scoped_ev_policy_2(
      net::EVRootCAMetadata::GetInstance(), net::SHA256HashValue(), "1.2.3.4");

  scoped_refptr<MockCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<MockCertVerifyProc>();
  // Primary verifier returns ok status and different_chain if verifying leaf
  // alone.
  EXPECT_CALL(*verify_proc1, VerifyInternal(leaf.get(), _, _, _, _, _, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<6>(different_chain_result), Return(net::OK)));
  // Primary verifier returns ok status and nonev_chain_result if verifying
  // cert_chain.
  EXPECT_CALL(*verify_proc1, VerifyInternal(cert_chain.get(), _, _, _, _, _, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<6>(nonev_chain_result), Return(net::OK)));

  // Trial verifier returns ok status and ev_chain_result.
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, ev_chain_result);

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf, "test.example", 0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::OK));

  verify_proc2->WaitForVerifyCall();

  // Expect no report.
  reporting_service_test_helper()->ExpectNoRequests(service());

  // Main CertVerifier_Job_Latency should have 2 counts since the
  // primary_reverifier was used.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 2);
  // CertVerifier_Job_Latency_TrialPrimary only has 1 count since
  // primary_reverifier doesn't use the same CertVerifier.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kIgnoredDifferentPathReVerifiesEquivalent,
      1);
}

TEST_F(TrialComparisonCertVerifierTest, BothVerifiersOkDifferentCertStatus) {
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status =
      net::CERT_STATUS_IS_EV | net::CERT_STATUS_REV_CHECKING_ENABLED;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, primary_result);

  net::CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_1_;
  secondary_result.cert_status = net::CERT_STATUS_CT_COMPLIANCE_FAILED;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, secondary_result);

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);
  net::CertVerifier::Config config;
  config.enable_rev_checking = true;
  config.enable_sha1_local_anchors = true;
  verifier.SetConfig(config);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1", 0,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::OK));

  verify_proc2->WaitForVerifyCall();

  // Expect a report.
  std::vector<std::string> full_reports;
  reporting_service_test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"127.0.0.1", RetryStatus::NOT_RETRIED}}),
      &full_reports);

  ASSERT_EQ(1U, full_reports.size());

  chrome_browser_ssl::CertLoggerRequest report;
  ASSERT_TRUE(report.ParseFromString(full_reports[0]));

  EXPECT_EQ(0, report.cert_error_size());
  ASSERT_EQ(2, report.cert_status_size());
  EXPECT_EQ(chrome_browser_ssl::CertLoggerRequest::STATUS_IS_EV,
            report.cert_status()[0]);
  EXPECT_EQ(chrome_browser_ssl::CertLoggerRequest::STATUS_REV_CHECKING_ENABLED,
            report.cert_status()[1]);

  ASSERT_TRUE(report.has_features_info());
  ASSERT_TRUE(report.features_info().has_trial_verification_info());
  const chrome_browser_ssl::TrialVerificationInfo& trial_info =
      report.features_info().trial_verification_info();
  EXPECT_EQ(0, trial_info.cert_error_size());
  ASSERT_EQ(1, trial_info.cert_status_size());
  EXPECT_EQ(chrome_browser_ssl::CertLoggerRequest::STATUS_CT_COMPLIANCE_FAILED,
            trial_info.cert_status()[0]);

  EXPECT_THAT(
      trial_info.verify_flags(),
      testing::UnorderedElementsAre(chrome_browser_ssl::TrialVerificationInfo::
                                        VERIFY_REV_CHECKING_ENABLED,
                                    chrome_browser_ssl::TrialVerificationInfo::
                                        VERIFY_ENABLE_SHA1_LOCAL_ANCHORS));

  EXPECT_THAT(report.unverified_cert_chain(), CertChainMatches(leaf_cert_1_));
  EXPECT_THAT(report.cert_chain(), CertChainMatches(cert_chain_1_));
  EXPECT_THAT(trial_info.cert_chain(), CertChainMatches(cert_chain_1_));

  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kBothValidDifferentDetails, 1);
}

TEST_F(TrialComparisonCertVerifierTest, Coalescing) {
  // Primary verifier returns an error status.
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status = net::CERT_STATUS_DATE_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::ERR_CERT_DATE_INVALID,
                                               primary_result);

  // Trial verifier has ok status.
  net::CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, secondary_result);

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);

  // Start first verification request.
  net::CertVerifyResult result_1;
  std::unique_ptr<net::CertVerifier::Request> request_1;
  net::TestCompletionCallback callback_1;
  int error = verifier.Verify(params, &result_1, callback_1.callback(),
                              &request_1, net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request_1);

  // Start second verification request with same params.
  net::CertVerifyResult result_2;
  std::unique_ptr<net::CertVerifier::Request> request_2;
  net::TestCompletionCallback callback_2;
  error = verifier.Verify(params, &result_2, callback_2.callback(), &request_2,
                          net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request_2);

  // Both callbacks should be called with same error code.
  error = callback_1.WaitForResult();
  EXPECT_THAT(error, IsError(net::ERR_CERT_DATE_INVALID));
  error = callback_2.WaitForResult();
  EXPECT_THAT(error, IsError(net::ERR_CERT_DATE_INVALID));

  // Trial verifier should run.
  verify_proc2->WaitForVerifyCall();

  // Expect a single report.
  std::vector<std::string> full_reports;
  reporting_service_test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"127.0.0.1", RetryStatus::NOT_RETRIED}}),
      &full_reports);

  ASSERT_EQ(1U, full_reports.size());

  chrome_browser_ssl::CertLoggerRequest report;
  ASSERT_TRUE(report.ParseFromString(full_reports[0]));

  ASSERT_EQ(1, report.cert_error_size());
  EXPECT_EQ(chrome_browser_ssl::CertLoggerRequest::ERR_CERT_DATE_INVALID,
            report.cert_error()[0]);
  EXPECT_EQ(0, report.cert_status_size());

  ASSERT_TRUE(report.has_features_info());
  ASSERT_TRUE(report.features_info().has_trial_verification_info());
  const chrome_browser_ssl::TrialVerificationInfo& trial_info =
      report.features_info().trial_verification_info();
  ASSERT_EQ(0, trial_info.cert_error_size());
  EXPECT_EQ(0, trial_info.cert_status_size());

  EXPECT_THAT(report.unverified_cert_chain(), CertChainMatches(leaf_cert_1_));
  EXPECT_THAT(report.cert_chain(), CertChainMatches(cert_chain_1_));
  EXPECT_THAT(trial_info.cert_chain(), CertChainMatches(cert_chain_1_));

  // Only one verification should be done by primary verifier.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  // Only one verification should be done by secondary verifier.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kPrimaryErrorSecondaryValid, 1);
}

TEST_F(TrialComparisonCertVerifierTest, CancelledDuringPrimaryVerification) {
  // Primary verifier returns an error status.
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status = net::CERT_STATUS_DATE_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::ERR_CERT_DATE_INVALID,
                                               primary_result);

  // Trial verifier has ok status.
  net::CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, secondary_result);

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error =
      verifier.Verify(params, &result, base::BindRepeating(&NotCalledCallback),
                      &request, net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  // Delete the request, cancelling it.
  request.reset();

  // The callback to the main verifier does not run. However, the verification
  // still completes in the background and triggers the trial verification.

  // Trial verifier should still run.
  verify_proc2->WaitForVerifyCall();

  // Expect a report.
  std::vector<std::string> full_reports;
  reporting_service_test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"127.0.0.1", RetryStatus::NOT_RETRIED}}),
      &full_reports);

  ASSERT_EQ(1U, full_reports.size());

  chrome_browser_ssl::CertLoggerRequest report;
  ASSERT_TRUE(report.ParseFromString(full_reports[0]));

  ASSERT_EQ(1, report.cert_error_size());
  EXPECT_EQ(chrome_browser_ssl::CertLoggerRequest::ERR_CERT_DATE_INVALID,
            report.cert_error()[0]);
  EXPECT_EQ(0, report.cert_status_size());

  ASSERT_TRUE(report.has_features_info());
  ASSERT_TRUE(report.features_info().has_trial_verification_info());
  const chrome_browser_ssl::TrialVerificationInfo& trial_info =
      report.features_info().trial_verification_info();
  ASSERT_EQ(0, trial_info.cert_error_size());
  EXPECT_EQ(0, trial_info.cert_status_size());

  EXPECT_THAT(report.unverified_cert_chain(), CertChainMatches(leaf_cert_1_));
  EXPECT_THAT(report.cert_chain(), CertChainMatches(cert_chain_1_));
  EXPECT_THAT(trial_info.cert_chain(), CertChainMatches(cert_chain_1_));

  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kPrimaryErrorSecondaryValid, 1);
}

TEST_F(TrialComparisonCertVerifierTest, DeletedDuringPrimaryVerification) {
  // Primary verifier returns an error status.
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status = net::CERT_STATUS_DATE_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::ERR_CERT_DATE_INVALID,
                                               primary_result);

  auto verifier = std::make_unique<TrialComparisonCertVerifier>(
      profile(), verify_proc1, base::MakeRefCounted<NotCalledCertVerifyProc>());

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error =
      verifier->Verify(params, &result, base::BindRepeating(&NotCalledCallback),
                       &request, net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  // Delete the TrialComparisonCertVerifier.
  verifier.reset();

  // The callback to the main verifier does not run. The verification task
  // still completes in the background, but since the CertVerifier has been
  // deleted, the result is ignored.

  // Wait for any tasks to finish.
  content::RunAllTasksUntilIdle();

  // Expect no report.
  reporting_service_test_helper()->ExpectNoRequests(service());

  // Histograms should not be recorded.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 0);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               0);
  histograms_.ExpectTotalCount("Net.CertVerifier_TrialComparisonResult", 0);
}

TEST_F(TrialComparisonCertVerifierTest, DeletedBeforeTrialVerificationStarted) {
  // Primary verifier returns an error status.
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status = net::CERT_STATUS_DATE_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::ERR_CERT_DATE_INVALID,
                                               primary_result);

  // Trial verifier has ok status.
  net::CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<NotCalledCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<NotCalledCertVerifyProc>();

  auto verifier = std::make_unique<TrialComparisonCertVerifier>(
      profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier->Verify(params, &result, callback.callback(), &request,
                               net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  // Wait for primary verifier to finish.
  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::ERR_CERT_DATE_INVALID));

  // Delete the TrialComparisonCertVerifier.
  verifier.reset();

  // Trial verification has not yet started, as it was waiting on the profile
  // to determine whether or not it would be permitted.

  // Wait for any tasks to finish.
  content::RunAllTasksUntilIdle();

  // Expect no report.
  reporting_service_test_helper()->ExpectNoRequests(service());

  // The actual verification job should be completed, but neither the
  // primary nor secondary job metrics should be recorded, as the verifier
  // was deleted prior to determining whether a trial verification would be
  // run.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               0);
  histograms_.ExpectTotalCount("Net.CertVerifier_TrialComparisonResult", 0);
}

TEST_F(TrialComparisonCertVerifierTest, DeletedAfterTrialVerificationStarted) {
  // Primary verifier returns an error status.
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  primary_result.cert_status = net::CERT_STATUS_DATE_INVALID;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::ERR_CERT_DATE_INVALID,
                                               primary_result);

  // Trial verifier has ok status.
  net::CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, secondary_result);

  auto verifier = std::make_unique<TrialComparisonCertVerifier>(
      profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier->Verify(params, &result, callback.callback(), &request,
                               net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  // Wait for primary verifier to finish.
  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::ERR_CERT_DATE_INVALID));

  // Allow the lookup on the UI thread for the profile to determine trial
  // status.
  std::unique_ptr<base::RunLoop> run_loop(std::make_unique<base::RunLoop>());
  base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::UI})
      ->PostTask(FROM_HERE, run_loop->QuitClosure());
  run_loop->Run();

  // Allow recording the metrics back on the IO thread, and starting the
  // second verification, to run.
  run_loop = std::make_unique<base::RunLoop>();
  base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::IO})
      ->PostTask(FROM_HERE, run_loop->QuitClosure());
  run_loop->Run();

  // Delete the TrialComparisonCertVerifier. The trial verification is still
  // running on the task scheduler (or, depending on timing, has posted back
  // to the IO thread after the Quit event).
  verifier.reset();

  // The callback to the trial verifier does not run. The verification task
  // still completes in the background, but since the CertVerifier has been
  // deleted, the result is ignored.

  // Wait for any tasks to finish.
  content::RunAllTasksUntilIdle();

  // Expect no report.
  reporting_service_test_helper()->ExpectNoRequests(service());

  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  // Histograms for trial verifier should not be recorded.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               0);
  histograms_.ExpectTotalCount("Net.CertVerifier_TrialComparisonResult", 0);
}

TEST_F(TrialComparisonCertVerifierTest,
       PrimaryVerifierOkSecondaryErrorUmaOnly) {
  // Enable feature with uma_only flag.
  scoped_feature_.reset();
  scoped_feature_ = std::make_unique<base::test::ScopedFeatureList>();
  scoped_feature_->InitAndEnableFeatureWithParameters(
      features::kCertDualVerificationTrialFeature, {{"uma_only", "true"}});

  net::CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, primary_result);

  // Trial verifier returns an error status.
  net::CertVerifyResult secondary_result;
  secondary_result.cert_status = net::CERT_STATUS_DATE_INVALID;
  secondary_result.verified_cert = cert_chain_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::ERR_CERT_DATE_INVALID,
                                               secondary_result);

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::OK));

  verify_proc2->WaitForVerifyCall();

  // Wait for any tasks to finish.
  content::RunAllTasksUntilIdle();

  // Expect no report.
  reporting_service_test_helper()->ExpectNoRequests(service());

  // Should still have UMA logs.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kPrimaryValidSecondaryError, 1);
}

TEST_F(TrialComparisonCertVerifierTest, MacUndesiredRevocationChecking) {
  net::CertVerifyResult revoked_result;
  revoked_result.verified_cert = cert_chain_1_;
  revoked_result.cert_status = net::CERT_STATUS_REVOKED;

  net::CertVerifyResult ok_result;
  ok_result.verified_cert = cert_chain_1_;

  // Primary verifier returns an error status.
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::ERR_CERT_REVOKED,
                                               revoked_result);

  scoped_refptr<MockCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<MockCertVerifyProc>();
  // Secondary verifier returns ok status...
  EXPECT_CALL(*verify_proc2, VerifyInternal(_, _, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<6>(ok_result), Return(net::OK)));
  // ...unless it was called with REV_CHECKING_ENABLED.
  EXPECT_CALL(
      *verify_proc2,
      VerifyInternal(_, _, _, net::CertVerifyProc::VERIFY_REV_CHECKING_ENABLED,
                     _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<6>(revoked_result),
                            Return(net::ERR_CERT_REVOKED)));

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::ERR_CERT_REVOKED));

  content::RunAllTasksUntilIdle();

  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
#if defined(OS_MACOSX)
  // Expect no report.
  reporting_service_test_helper()->ExpectNoRequests(service());

  // Secondary should have been called twice
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               2);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kIgnoredMacUndesiredRevocationChecking, 1);
#else
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kPrimaryErrorSecondaryValid, 1);

  // Expect a report.
  std::vector<std::string> full_reports;
  reporting_service_test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"127.0.0.1", RetryStatus::NOT_RETRIED}}),
      &full_reports);

  ASSERT_EQ(1U, full_reports.size());
#endif
}

TEST_F(TrialComparisonCertVerifierTest, PrimaryRevokedSecondaryOk) {
  net::CertVerifyResult revoked_result;
  revoked_result.verified_cert = cert_chain_1_;
  revoked_result.cert_status = net::CERT_STATUS_REVOKED;

  net::CertVerifyResult ok_result;
  ok_result.verified_cert = cert_chain_1_;

  // Primary verifier returns an error status.
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::ERR_CERT_REVOKED,
                                               revoked_result);

  // Secondary verifier returns ok status regardless of whether
  // REV_CHECKING_ENABLED was passed.
  scoped_refptr<MockCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<MockCertVerifyProc>();
  EXPECT_CALL(*verify_proc2, VerifyInternal(_, _, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<6>(ok_result), Return(net::OK)));

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::ERR_CERT_REVOKED));

  content::RunAllTasksUntilIdle();

  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
#if defined(OS_MACOSX)
  // Secondary should have been called twice on mac due to attempting the
  // kIgnoredMacUndesiredRevocationChecking workaround.
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               2);
#else
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);

#endif
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kPrimaryErrorSecondaryValid, 1);

  // Expect a report.
  std::vector<std::string> full_reports;
  reporting_service_test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"127.0.0.1", RetryStatus::NOT_RETRIED}}),
      &full_reports);

  ASSERT_EQ(1U, full_reports.size());
}

TEST_F(TrialComparisonCertVerifierTest, MultipleEVPolicies) {
  base::FilePath certs_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &certs_dir));
  certs_dir = certs_dir.AppendASCII("net")
                  .AppendASCII("trial_comparison_cert_verifier_unittest")
                  .AppendASCII("target-multiple-policies");
  scoped_refptr<net::X509Certificate> cert_chain =
      CreateCertificateChainFromFile(certs_dir, "chain.pem",
                                     net::X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert_chain);
  ASSERT_EQ(2U, cert_chain->intermediate_buffers().size());

  net::SHA256HashValue root_fingerprint;
  crypto::SHA256HashString(net::x509_util::CryptoBufferAsStringPiece(
                               cert_chain->intermediate_buffers().back().get()),
                           root_fingerprint.data,
                           sizeof(root_fingerprint.data));

  // Both policies in the target are EV policies, but only 1.2.6.7 is valid for
  // the root in this chain.
  net::ScopedTestEVPolicy scoped_ev_policy_1(
      net::EVRootCAMetadata::GetInstance(), root_fingerprint, "1.2.6.7");
  net::ScopedTestEVPolicy scoped_ev_policy_2(
      net::EVRootCAMetadata::GetInstance(), net::SHA256HashValue(), "1.2.3.4");

  // Both verifiers return OK, but secondary returns EV status.
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, primary_result);

  net::CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain;
  secondary_result.cert_status =
      net::CERT_STATUS_IS_EV | net::CERT_STATUS_REV_CHECKING_ENABLED;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, secondary_result);

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::OK));

  verify_proc2->WaitForVerifyCall();

  // Expect no report.
  reporting_service_test_helper()->ExpectNoRequests(service());

  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kIgnoredMultipleEVPoliciesAndOneMatchesRoot,
      1);
}

TEST_F(TrialComparisonCertVerifierTest, MultipleEVPoliciesNoneValidForRoot) {
  base::FilePath certs_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &certs_dir));
  certs_dir = certs_dir.AppendASCII("net")
                  .AppendASCII("trial_comparison_cert_verifier_unittest")
                  .AppendASCII("target-multiple-policies");
  scoped_refptr<net::X509Certificate> cert_chain =
      CreateCertificateChainFromFile(certs_dir, "chain.pem",
                                     net::X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert_chain);

  // Both policies in the target are EV policies, but neither is valid for the
  // root in this chain.
  net::ScopedTestEVPolicy scoped_ev_policy_1(
      net::EVRootCAMetadata::GetInstance(), {1}, "1.2.6.7");
  net::ScopedTestEVPolicy scoped_ev_policy_2(
      net::EVRootCAMetadata::GetInstance(), {2}, "1.2.3.4");

  // Both verifiers return OK, but secondary returns EV status.
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, primary_result);

  net::CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain;
  secondary_result.cert_status =
      net::CERT_STATUS_IS_EV | net::CERT_STATUS_REV_CHECKING_ENABLED;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, secondary_result);

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::OK));

  verify_proc2->WaitForVerifyCall();

  // Expect a report.
  std::vector<std::string> full_reports;
  reporting_service_test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"127.0.0.1", RetryStatus::NOT_RETRIED}}),
      &full_reports);

  ASSERT_EQ(1U, full_reports.size());

  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kBothValidDifferentDetails, 1);
}

TEST_F(TrialComparisonCertVerifierTest, MultiplePoliciesOnlyOneIsEV) {
  base::FilePath certs_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &certs_dir));
  certs_dir = certs_dir.AppendASCII("net")
                  .AppendASCII("trial_comparison_cert_verifier_unittest")
                  .AppendASCII("target-multiple-policies");
  scoped_refptr<net::X509Certificate> cert_chain =
      CreateCertificateChainFromFile(certs_dir, "chain.pem",
                                     net::X509Certificate::FORMAT_AUTO);
  ASSERT_TRUE(cert_chain);
  ASSERT_EQ(2U, cert_chain->intermediate_buffers().size());

  net::SHA256HashValue root_fingerprint;
  crypto::SHA256HashString(net::x509_util::CryptoBufferAsStringPiece(
                               cert_chain->intermediate_buffers().back().get()),
                           root_fingerprint.data,
                           sizeof(root_fingerprint.data));

  // One policy in the target is an EV policy and is valid for the root.
  net::ScopedTestEVPolicy scoped_ev_policy_1(
      net::EVRootCAMetadata::GetInstance(), root_fingerprint, "1.2.6.7");

  // Both verifiers return OK, but secondary returns EV status.
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = cert_chain;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, primary_result);

  net::CertVerifyResult secondary_result;
  secondary_result.verified_cert = cert_chain;
  secondary_result.cert_status =
      net::CERT_STATUS_IS_EV | net::CERT_STATUS_REV_CHECKING_ENABLED;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, secondary_result);

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::OK));

  verify_proc2->WaitForVerifyCall();

  // Expect a report.
  std::vector<std::string> full_reports;
  reporting_service_test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"127.0.0.1", RetryStatus::NOT_RETRIED}}),
      &full_reports);

  ASSERT_EQ(1U, full_reports.size());

  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kBothValidDifferentDetails, 1);
}

TEST_F(TrialComparisonCertVerifierTest, LocallyTrustedLeaf) {
  // Platform verifier verifies the leaf directly.
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = leaf_cert_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc1 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::OK, primary_result);

  // Trial verifier does not support directly-trusted leaf certs.
  net::CertVerifyResult secondary_result;
  secondary_result.cert_status = net::CERT_STATUS_AUTHORITY_INVALID;
  secondary_result.verified_cert = leaf_cert_1_;
  scoped_refptr<FakeCertVerifyProc> verify_proc2 =
      base::MakeRefCounted<FakeCertVerifyProc>(net::ERR_CERT_AUTHORITY_INVALID,
                                               secondary_result);

  TrialComparisonCertVerifier verifier(profile(), verify_proc1, verify_proc2);

  net::CertVerifier::RequestParams params(leaf_cert_1_, "127.0.0.1",
                                          0 /* flags */,
                                          std::string() /* ocsp_response */);
  net::CertVerifyResult result;
  net::TestCompletionCallback callback;
  std::unique_ptr<net::CertVerifier::Request> request;
  int error = verifier.Verify(params, &result, callback.callback(), &request,
                              net::NetLogWithSource());
  ASSERT_THAT(error, IsError(net::ERR_IO_PENDING));
  EXPECT_TRUE(request);

  error = callback.WaitForResult();
  EXPECT_THAT(error, IsError(net::OK));

  verify_proc2->WaitForVerifyCall();

  // Expect no report.
  reporting_service_test_helper()->ExpectNoRequests(service());

  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
  histograms_.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                               1);
  histograms_.ExpectUniqueSample(
      "Net.CertVerifier_TrialComparisonResult",
      TrialComparisonCertVerifier::kIgnoredLocallyTrustedLeaf, 1);
}
