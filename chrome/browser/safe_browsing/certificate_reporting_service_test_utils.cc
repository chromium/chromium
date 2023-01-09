// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/certificate_reporting_service_test_utils.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/string_piece.h"
#include "components/encrypted_messages/encrypted_message.pb.h"
#include "components/encrypted_messages/message_encrypter.h"
#include "components/security_interstitials/content/certificate_error_report.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"

namespace {

static const char kHkdfLabel[] = "certificate report";
const uint32_t kServerPublicKeyTestVersion = 16;

std::string GetReportContents(const network::ResourceRequest& request,
                              const uint8_t* server_private_key) {
  std::string serialized_report(network::GetUploadData(request));
  encrypted_messages::EncryptedMessage encrypted_message;
  EXPECT_TRUE(encrypted_message.ParseFromString(serialized_report));
  EXPECT_EQ(kServerPublicKeyTestVersion,
            encrypted_message.server_public_key_version());
  EXPECT_EQ(
      encrypted_messages::EncryptedMessage::AEAD_ECDH_AES_128_CTR_HMAC_SHA256,
      encrypted_message.algorithm());
  std::string decrypted_report;
  // TODO(estark): kHkdfLabel needs to include the null character in the label
  // due to a matching error in the server for the case of certificate
  // reporting, the strlen + 1 can be removed once that error is fixed.
  // https://crbug.com/517746
  EXPECT_TRUE(encrypted_messages::DecryptMessageForTesting(
      server_private_key, base::StringPiece(kHkdfLabel, strlen(kHkdfLabel) + 1),
      encrypted_message, &decrypted_report));
  return decrypted_report;
}

void WaitReports(
    certificate_reporting_test_utils::RequestObserver* observer,
    const certificate_reporting_test_utils::ReportExpectation& expectation,
    std::vector<std::string>* full_reports,
    std::vector<network::ResourceRequest>* full_requests) {
  observer->Wait(expectation.num_reports());
  EXPECT_EQ(expectation.successful_reports, observer->successful_reports());
  EXPECT_EQ(expectation.failed_reports, observer->failed_reports());
  EXPECT_EQ(expectation.delayed_reports, observer->delayed_reports());
  if (full_reports)
    *full_reports = observer->full_reports();
  if (full_requests)
    *full_requests = observer->full_requests();
  observer->ClearObservedReports();
}

}  // namespace

namespace certificate_reporting_test_utils {

RequestObserver::RequestObserver()
    : num_events_to_wait_for_(0u), num_received_events_(0u) {}

RequestObserver::~RequestObserver() {}

void RequestObserver::Wait(unsigned int num_events_to_wait_for) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!run_loop_);
  ASSERT_LE(num_received_events_, num_events_to_wait_for)
      << "Observed unexpected report";

  if (num_received_events_ < num_events_to_wait_for) {
    num_events_to_wait_for_ = num_events_to_wait_for;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset(nullptr);
    EXPECT_EQ(0u, num_received_events_);
    EXPECT_EQ(0u, num_events_to_wait_for_);
  } else if (num_received_events_ == num_events_to_wait_for) {
    num_received_events_ = 0u;
    num_events_to_wait_for_ = 0u;
  }
}

void RequestObserver::OnRequest(const network::ResourceRequest& url_request,
                                const std::string& serialized_report,
                                ReportSendingResult report_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CertificateErrorReport report;
  EXPECT_TRUE(report.InitializeFromString(serialized_report));

  full_reports_.push_back(serialized_report);
  full_requests_.push_back(url_request);

  switch (report_type) {
    case REPORTS_SUCCESSFUL:
      successful_reports_[report.hostname()] =
          report.is_retry_upload() ? RETRIED : NOT_RETRIED;
      break;
    case REPORTS_FAIL:
      failed_reports_[report.hostname()] =
          report.is_retry_upload() ? RETRIED : NOT_RETRIED;
      break;
    case REPORTS_DELAY:
      delayed_reports_[report.hostname()] =
          report.is_retry_upload() ? RETRIED : NOT_RETRIED;
      break;
  }

  num_received_events_++;
  if (!run_loop_) {
    return;
  }
  ASSERT_LE(num_received_events_, num_events_to_wait_for_)
      << "Observed unexpected report";

  if (num_received_events_ == num_events_to_wait_for_) {
    num_events_to_wait_for_ = 0u;
    num_received_events_ = 0u;
    run_loop_->Quit();
  }
}

const ObservedReportMap& RequestObserver::successful_reports() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return successful_reports_;
}

const ObservedReportMap& RequestObserver::failed_reports() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return failed_reports_;
}

const ObservedReportMap& RequestObserver::delayed_reports() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return delayed_reports_;
}
const std::vector<std::string>& RequestObserver::full_reports() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return full_reports_;
}
const std::vector<network::ResourceRequest>& RequestObserver::full_requests()
    const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return full_requests_;
}

void RequestObserver::ClearObservedReports() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  successful_reports_.clear();
  failed_reports_.clear();
  delayed_reports_.clear();
  full_reports_.clear();
  full_requests_.clear();
}

ReportExpectation::ReportExpectation() {}

ReportExpectation::ReportExpectation(const ReportExpectation& other) = default;

ReportExpectation::~ReportExpectation() {}

// static
ReportExpectation ReportExpectation::Successful(
    const ObservedReportMap& reports) {
  ReportExpectation expectation;
  expectation.successful_reports = reports;
  return expectation;
}

// static
ReportExpectation ReportExpectation::Failed(const ObservedReportMap& reports) {
  ReportExpectation expectation;
  expectation.failed_reports = reports;
  return expectation;
}

// static
ReportExpectation ReportExpectation::Delayed(const ObservedReportMap& reports) {
  ReportExpectation expectation;
  expectation.delayed_reports = reports;
  return expectation;
}

int ReportExpectation::num_reports() const {
  return successful_reports.size() + failed_reports.size() +
         delayed_reports.size();
}

CertificateReportingServiceObserver::CertificateReportingServiceObserver() {}

CertificateReportingServiceObserver::~CertificateReportingServiceObserver() {}

void CertificateReportingServiceObserver::Clear() {
  did_reset_ = false;
}

void CertificateReportingServiceObserver::WaitForReset() {
  DCHECK(!run_loop_);
  if (did_reset_)
    return;
  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

void CertificateReportingServiceObserver::OnServiceReset() {
  did_reset_ = true;
  if (run_loop_)
    run_loop_->Quit();
}

CertificateReportingServiceTestHelper::CertificateReportingServiceTestHelper()
    : expected_report_result_(REPORTS_FAIL) {
  memset(server_private_key_, 1, sizeof(server_private_key_));
  X25519_public_from_private(server_public_key_, server_private_key_);
}

CertificateReportingServiceTestHelper::
    ~CertificateReportingServiceTestHelper() {}

void CertificateReportingServiceTestHelper::SetFailureMode(
    ReportSendingResult expected_report_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  expected_report_result_ = expected_report_result;
}

void CertificateReportingServiceTestHelper::ResumeDelayedRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  EXPECT_EQ(REPORTS_DELAY, expected_report_result_);
  if (delayed_client_) {
    SendResponse(std::move(delayed_client_), delayed_result_ == REPORTS_FAIL);
    request_destroyed_observer_.OnRequest(delayed_request_, delayed_report_,
                                          delayed_result_);
  }
}

uint8_t* CertificateReportingServiceTestHelper::server_public_key() {
  return server_public_key_;
}

uint32_t CertificateReportingServiceTestHelper::server_public_key_version()
    const {
  return kServerPublicKeyTestVersion;
}

void CertificateReportingServiceTestHelper::WaitForRequestsCreated(
    const ReportExpectation& expectation) {
  WaitReports(&request_created_observer_, expectation, nullptr, nullptr);
}

void CertificateReportingServiceTestHelper::WaitForRequestsCreated(
    const ReportExpectation& expectation,
    std::vector<std::string>* full_reports,
    std::vector<network::ResourceRequest>* full_requests) {
  WaitReports(&request_created_observer_, expectation, full_reports,
              full_requests);
}

void CertificateReportingServiceTestHelper::WaitForRequestsDestroyed(
    const ReportExpectation& expectation) {
  WaitReports(&request_destroyed_observer_, expectation, nullptr, nullptr);
}

void CertificateReportingServiceTestHelper::WaitForRequestsDestroyed(
    const ReportExpectation& expectation,
    std::vector<std::string>* full_reports,
    std::vector<network::ResourceRequest>* full_requests) {
  WaitReports(&request_destroyed_observer_, expectation, full_reports,
              full_requests);
}

void CertificateReportingServiceTestHelper::ExpectNoRequests(
    CertificateReportingService* service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Check that all requests have been destroyed.
  EXPECT_TRUE(request_destroyed_observer_.successful_reports().empty());
  EXPECT_TRUE(request_destroyed_observer_.failed_reports().empty());
  EXPECT_TRUE(request_destroyed_observer_.delayed_reports().empty());

  if (service->GetReporterForTesting()) {
    // Reporter can be null if reporting is disabled.
    EXPECT_EQ(
        0u,
        service->GetReporterForTesting()->inflight_report_count_for_testing());
  }
}

void CertificateReportingServiceTestHelper::SendResponse(
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    bool fail) {
  mojo::Remote<network::mojom::URLLoaderClient> client_remote(
      std::move(client));
  if (fail) {
    client_remote->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_SSL_PROTOCOL_ERROR));
    return;
  }

  auto head = network::mojom::URLResponseHead::New();
  head->headers = new net::HttpResponseHeaders(
      "HTTP/1.1 200 OK\nContent-type: text/html\n\n");
  head->mime_type = "text/html";
  client_remote->OnReceiveResponse(
      std::move(head), mojo::ScopedDataPipeConsumerHandle(), absl::nullopt);
  client_remote->OnComplete(network::URLLoaderCompletionStatus());
}

void CertificateReportingServiceTestHelper::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  const std::string serialized_report =
      GetReportContents(url_request, server_private_key_);
  request_created_observer_.OnRequest(url_request, serialized_report,
                                      expected_report_result_);

  if (expected_report_result_ == REPORTS_FAIL) {
    SendResponse(std::move(client), true);
    request_destroyed_observer_.OnRequest(url_request, serialized_report,
                                          expected_report_result_);
    return;
  }

  if (expected_report_result_ == REPORTS_DELAY) {
    DCHECK(!delayed_client_) << "Supports only one delayed request at a time";
    delayed_client_ = std::move(client);
    delayed_report_ = serialized_report;
    delayed_request_ = url_request;
    delayed_result_ = expected_report_result_;
    return;
  }

  SendResponse(std::move(client), false);
  request_destroyed_observer_.OnRequest(url_request, serialized_report,
                                        expected_report_result_);
}

void CertificateReportingServiceTestHelper::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  NOTREACHED();
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
CertificateReportingServiceTestHelper::Clone() {
  NOTREACHED();
  return nullptr;
}

EventHistogramTester::EventHistogramTester() {}

EventHistogramTester::~EventHistogramTester() {
  if (submitted_) {
    histogram_tester_.ExpectBucketCount(
        CertificateReportingService::kReportEventHistogram,
        static_cast<int>(CertificateReportingService::ReportOutcome::SUBMITTED),
        submitted_);
  }
  if (failed_) {
    histogram_tester_.ExpectBucketCount(
        CertificateReportingService::kReportEventHistogram,
        static_cast<int>(CertificateReportingService::ReportOutcome::FAILED),
        failed_);
  }
  if (successful_) {
    histogram_tester_.ExpectBucketCount(
        CertificateReportingService::kReportEventHistogram,
        static_cast<int>(
            CertificateReportingService::ReportOutcome::SUCCESSFUL),
        successful_);
  }
  if (dropped_) {
    histogram_tester_.ExpectBucketCount(
        CertificateReportingService::kReportEventHistogram,
        static_cast<int>(
            CertificateReportingService::ReportOutcome::DROPPED_OR_IGNORED),
        dropped_);
  }
  histogram_tester_.ExpectTotalCount(
      CertificateReportingService::kReportEventHistogram,
      submitted_ + failed_ + successful_ + dropped_);
}

void EventHistogramTester::SetExpectedValues(int submitted,
                                             int failed,
                                             int successful,
                                             int dropped) {
  submitted_ = submitted;
  failed_ = failed;
  successful_ = successful;
  dropped_ = dropped;
}

}  // namespace certificate_reporting_test_utils
