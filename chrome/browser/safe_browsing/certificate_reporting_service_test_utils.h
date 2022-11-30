// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_TEST_UTILS_H_
#define CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_TEST_UTILS_H_

#include <unordered_map>

#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"

namespace certificate_reporting_test_utils {

enum RetryStatus {
  NOT_RETRIED = 0,
  RETRIED = 1,
};

typedef std::unordered_map<std::string, RetryStatus> ObservedReportMap;

// Syntactic sugar for wrapping report expectations in a more readable way.
// Passed to WaitForRequestDeletions() as input.
// Example:
// The following expects report0 and report1 to be successfully sent and their
// URL requests to be deleted:
// WaitForRequestsDestroyed(
//     ReportExpectation::Successful("report0, report1"));
struct ReportExpectation {
  ReportExpectation();
  ReportExpectation(const ReportExpectation& other);
  ~ReportExpectation();
  // Returns an expectation where all reports in |reports| succeed.
  static ReportExpectation Successful(const ObservedReportMap& reports);
  // Returns an expectation where all reports in |reports| fail.
  static ReportExpectation Failed(const ObservedReportMap& reports);
  // Returns an expectation where all reports in |reports| are delayed.
  static ReportExpectation Delayed(const ObservedReportMap& reports);
  // Total number of reports expected.
  int num_reports() const;
  ObservedReportMap successful_reports;
  ObservedReportMap failed_reports;
  ObservedReportMap delayed_reports;
};

// Failure mode of the report sending attempts.
enum ReportSendingResult {
  // Report send attempts should be successful.
  REPORTS_SUCCESSFUL,
  // Report send attempts should fail.
  REPORTS_FAIL,
  // Report send attempts should hang until explicitly resumed.
  REPORTS_DELAY,
};

// Helper class to wait for a number of events (e.g. request destroyed, report
// observed).
class RequestObserver {
 public:
  RequestObserver();
  ~RequestObserver();

  // Waits for |num_request| requests to be created or destroyed, depending on
  // whichever one this class observes.
  void Wait(unsigned int num_events_to_wait_for);

  // Called when a request created or destroyed, depending on whichever one this
  // class observes.
  void OnRequest(const network::ResourceRequest& url_request,
                 const std::string& serialized_report,
                 ReportSendingResult report_type);

  // These must be called on the UI thread.
  const ObservedReportMap& successful_reports() const;
  const ObservedReportMap& failed_reports() const;
  const ObservedReportMap& delayed_reports() const;
  const std::vector<std::string>& full_reports() const;
  const std::vector<network::ResourceRequest>& full_requests() const;
  void ClearObservedReports();

 private:
  unsigned int num_events_to_wait_for_;
  unsigned int num_received_events_;
  std::unique_ptr<base::RunLoop> run_loop_;

  ObservedReportMap successful_reports_;
  ObservedReportMap failed_reports_;
  ObservedReportMap delayed_reports_;

  std::vector<std::string> full_reports_;
  std::vector<network::ResourceRequest> full_requests_;
};

// Class to wait for the CertificateReportingService to reset.
class CertificateReportingServiceObserver {
 public:
  CertificateReportingServiceObserver();
  ~CertificateReportingServiceObserver();

  // Clears the state of the observer. Must be called before waiting each time.
  void Clear();

  // Waits for the service to reset.
  void WaitForReset();

  // Must be called when the service is reset.
  void OnServiceReset();

 private:
  bool did_reset_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Base class for CertificateReportingService tests.
class CertificateReportingServiceTestHelper
    : public network::SharedURLLoaderFactory {
 public:
  CertificateReportingServiceTestHelper();

  CertificateReportingServiceTestHelper(
      const CertificateReportingServiceTestHelper&) = delete;
  CertificateReportingServiceTestHelper& operator=(
      const CertificateReportingServiceTestHelper&) = delete;

  // Changes the behavior of report uploads to fail, succeed or hang.
  void SetFailureMode(ReportSendingResult expected_report_result);

  // Resumes delayed report request. Failure mode should be REPORTS_DELAY when
  // calling this method.
  void ResumeDelayedRequest();

  void WaitForRequestsCreated(const ReportExpectation& expectation);
  void WaitForRequestsCreated(
      const ReportExpectation& expectation,
      std::vector<std::string>* full_reports,
      std::vector<network::ResourceRequest>* full_requests);
  void WaitForRequestsDestroyed(const ReportExpectation& expectation);
  void WaitForRequestsDestroyed(
      const ReportExpectation& expectation,
      std::vector<std::string>* full_reports,
      std::vector<network::ResourceRequest>* full_requests);

  // Checks that all requests are destroyed and that there are no in-flight
  // reports in |service|.
  void ExpectNoRequests(CertificateReportingService* service);

  uint8_t* server_public_key();
  uint32_t server_public_key_version() const;

 private:
  friend class base::RefCounted<CertificateReportingServiceTestHelper>;
  ~CertificateReportingServiceTestHelper() override;

  void SendResponse(mojo::PendingRemote<network::mojom::URLLoaderClient> client,
                    bool fail);

  // network::SharedURLLoaderFactory
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override;

  ReportSendingResult expected_report_result_;

  mojo::PendingRemote<network::mojom::URLLoaderClient> delayed_client_;
  std::string delayed_report_;
  network::ResourceRequest delayed_request_;
  ReportSendingResult delayed_result_;

  RequestObserver request_created_observer_;
  RequestObserver request_destroyed_observer_;

  uint8_t server_public_key_[32];
  uint8_t server_private_key_[32];
};

// Class to test reporting events histogram for CertificateReportingService.
// We can't use a simple HistogramTester, as we need to wait until test teardown
// to check the histogram contents. This ensures that all in-flight requests
// are torn down by the time we check the histograms.
class EventHistogramTester {
 public:
  EventHistogramTester();
  ~EventHistogramTester();

  void SetExpectedValues(int submitted,
                         int failed,
                         int successful,
                         int dropped);

 private:
  int submitted_ = 0;
  int failed_ = 0;
  int successful_ = 0;
  int dropped_ = 0;
  base::HistogramTester histogram_tester_;
};

}  // namespace certificate_reporting_test_utils

#endif  // CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_TEST_UTILS_H_
