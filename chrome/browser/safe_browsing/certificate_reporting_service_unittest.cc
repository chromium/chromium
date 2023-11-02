// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/certificate_reporting_service.h"

#include <memory>
#include <string>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/thread_test_helper.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_test_utils.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/security_interstitials/content/certificate_error_report.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using certificate_reporting_test_utils::CertificateReportingServiceTestHelper;
using certificate_reporting_test_utils::CertificateReportingServiceObserver;
using certificate_reporting_test_utils::EventHistogramTester;
using certificate_reporting_test_utils::ReportExpectation;
using certificate_reporting_test_utils::RetryStatus;

namespace {

// Maximum number of reports kept in the certificate reporting service's retry
// queue.
const size_t kMaxReportCountInQueue = 3;

// NSS requires that serial numbers be unique even for the same issuer;
// as all fake certificates will contain the same issuer name, it's
// necessary to ensure the serial number is unique, as otherwise
// NSS will fail to parse.
base::AtomicSequenceNumber g_serial_number;

scoped_refptr<net::X509Certificate> CreateFakeCert() {
  std::unique_ptr<crypto::RSAPrivateKey> unused_key;
  std::string cert_der;
  if (!net::x509_util::CreateKeyAndSelfSignedCert(
          "CN=Error", static_cast<uint32_t>(g_serial_number.GetNext()),
          base::Time::Now() - base::Minutes(5),
          base::Time::Now() + base::Minutes(5), &unused_key, &cert_der)) {
    return nullptr;
  }
  return net::X509Certificate::CreateFromBytes(
      base::as_bytes(base::make_span(cert_der)));
}

std::string MakeReport(const std::string& hostname) {
  net::SSLInfo ssl_info;
  ssl_info.cert = ssl_info.unverified_cert = CreateFakeCert();

  CertificateErrorReport report(hostname, ssl_info);
  std::string serialized_report;
  EXPECT_TRUE(report.Serialize(&serialized_report));
  return serialized_report;
}

void CheckReport(const CertificateReportingService::Report& report,
                 const std::string& expected_hostname,
                 bool expected_is_retry_upload,
                 const base::Time& expected_creation_time) {
  CertificateErrorReport error_report;
  EXPECT_TRUE(error_report.InitializeFromString(report.serialized_report));
  EXPECT_EQ(expected_hostname, error_report.hostname());
  EXPECT_EQ(expected_is_retry_upload, error_report.is_retry_upload());
  EXPECT_EQ(expected_creation_time, report.creation_time);
}

}  // namespace

TEST(CertificateReportingServiceReportListTest, BoundedReportList) {
  EventHistogramTester event_histogram_tester;

  // Create a report list with maximum of 2 items.
  CertificateReportingService::BoundedReportList list(2 /* max_size */);
  EXPECT_EQ(0u, list.items().size());

  // Add a ten minute old report.
  list.Add(CertificateReportingService::Report(
      1, base::Time::Now() - base::Minutes(10),
      std::string("report1_ten_minutes_old")));
  EXPECT_EQ(1u, list.items().size());
  EXPECT_EQ("report1_ten_minutes_old", list.items()[0].serialized_report);

  // Add another report. Items are ordered from newest to oldest report by
  // creation time. Oldest is at the end.
  list.Add(CertificateReportingService::Report(
      2, base::Time::Now() - base::Minutes(5),
      std::string("report2_five_minutes_old")));
  EXPECT_EQ(2u, list.items().size());
  EXPECT_EQ("report2_five_minutes_old", list.items()[0].serialized_report);
  EXPECT_EQ("report1_ten_minutes_old", list.items()[1].serialized_report);

  // Adding a third report removes the oldest report (report1) from the list.
  list.Add(CertificateReportingService::Report(
      3, base::Time::Now(), std::string("report3_zero_minutes_old")));
  EXPECT_EQ(2u, list.items().size());
  EXPECT_EQ("report3_zero_minutes_old", list.items()[0].serialized_report);
  EXPECT_EQ("report2_five_minutes_old", list.items()[1].serialized_report);

  // Adding a report older than the oldest report in the list (report2) is
  // a no-op.
  list.Add(CertificateReportingService::Report(
      0, base::Time::Now() - base::Minutes(10),
      std::string("report0_ten_minutes_old")));
  EXPECT_EQ(2u, list.items().size());
  EXPECT_EQ("report3_zero_minutes_old", list.items()[0].serialized_report);
  EXPECT_EQ("report2_five_minutes_old", list.items()[1].serialized_report);

  event_histogram_tester.SetExpectedValues(0 /* submitted */, 0 /* failed */,
                                           0 /* successful */, 2 /* dropped */);
}

class CertificateReportingServiceReporterOnIOThreadTest
    : public ::testing::Test {
 public:
  void SetUp() override {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    event_histogram_tester_ = std::make_unique<EventHistogramTester>();
  }

  void TearDown() override {
    // Check histograms as the last thing. This makes sure no in-flight report
    // is missed.
    event_histogram_tester_.reset();
  }

 protected:
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory() {
    return test_shared_loader_factory_;
  }

  EventHistogramTester* event_histogram_tester() {
    return event_histogram_tester_.get();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  // Histogram tester for reporting events. This is a member instead of a local
  // so that we can check the histogram after the test teardown. At that point
  // all in flight reports should be completed or deleted because
  // of CleanUpOnIOThread().
  std::unique_ptr<EventHistogramTester> event_histogram_tester_;
};

TEST_F(CertificateReportingServiceReporterOnIOThreadTest,
       Reporter_RetriesEnabled) {
  std::unique_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock());
  const base::Time reference_time = base::Time::Now();
  clock->SetNow(reference_time);

  const GURL kFailureURL("https://www.foo.com/");

  test_url_loader_factory()->AddResponse(
      kFailureURL, network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::ERR_SSL_PROTOCOL_ERROR));

  CertificateErrorReporter* certificate_error_reporter =
      new CertificateErrorReporter(test_shared_loader_factory(), kFailureURL);

  CertificateReportingService::BoundedReportList* list =
      new CertificateReportingService::BoundedReportList(2);

  // Create a reporter with retries enabled.
  CertificateReportingService::Reporter reporter(
      std::unique_ptr<CertificateErrorReporter>(certificate_error_reporter),
      std::unique_ptr<CertificateReportingService::BoundedReportList>(list),
      clock.get(), base::Seconds(100), true /* retries_enabled */);
  EXPECT_EQ(0u, list->items().size());
  EXPECT_EQ(0u, reporter.inflight_report_count_for_testing());

  // Sending a failed report will put the report in the retry list.
  reporter.Send(MakeReport("report1"));
  EXPECT_EQ(1u, reporter.inflight_report_count_for_testing());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, reporter.inflight_report_count_for_testing());
  ASSERT_EQ(1u, list->items().size());
  CheckReport(list->items()[0], "report1", false, reference_time);

  // Sending a second failed report will also put it in the retry list.
  clock->Advance(base::Seconds(10));
  reporter.Send(MakeReport("report2"));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, list->items().size());
  CheckReport(list->items()[0], "report2", false,
              reference_time + base::Seconds(10));
  CheckReport(list->items()[1], "report1", false, reference_time);

  // Sending a third report should remove the first report from the retry list.
  clock->Advance(base::Seconds(10));
  reporter.Send(MakeReport("report3"));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, list->items().size());
  CheckReport(list->items()[0], "report3", false,
              reference_time + base::Seconds(20));
  CheckReport(list->items()[1], "report2", false,
              reference_time + base::Seconds(10));

  // Retry sending all pending reports. All should fail and get added to the
  // retry list again. Report creation time doesn't change for retried reports.
  clock->Advance(base::Seconds(10));
  reporter.SendPending();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, list->items().size());
  CheckReport(list->items()[0], "report3", true,
              reference_time + base::Seconds(20));
  CheckReport(list->items()[1], "report2", true,
              reference_time + base::Seconds(10));

  // Advance the clock to 115 seconds. This makes report3 95 seconds old, and
  // report2 105 seconds old. report2 should be dropped because it's older than
  // max age (100 seconds).
  clock->SetNow(reference_time + base::Seconds(115));
  reporter.SendPending();
  EXPECT_EQ(1u, reporter.inflight_report_count_for_testing());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, list->items().size());
  CheckReport(list->items()[0], "report3", true,
              reference_time + base::Seconds(20));

  // Send pending reports again, this time successfully. There should be no
  // pending reports left.
  const GURL kSuccessURL("https://www.bar.com/");

  test_url_loader_factory()->AddResponse(kSuccessURL.spec(), "dummy data");

  certificate_error_reporter->set_upload_url_for_testing(kSuccessURL);
  clock->Advance(base::Seconds(1));
  reporter.SendPending();
  EXPECT_EQ(1u, reporter.inflight_report_count_for_testing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, list->items().size());

  // report1 was submitted once, failed once, dropped once.
  // report2 was submitted twice, failed twice, dropped once.
  // report3 was submitted four times, failed thrice, succeeded once.
  event_histogram_tester()->SetExpectedValues(
      7 /* submitted */, 6 /* failed */, 1 /* successful */, 2 /* dropped */);
}

// Same as above, but retries are disabled.
TEST_F(CertificateReportingServiceReporterOnIOThreadTest,
       Reporter_RetriesDisabled) {
  std::unique_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock());
  base::Time reference_time = base::Time::Now();
  clock->SetNow(reference_time);

  const GURL kFailureURL("https://www.foo.com/");

  test_url_loader_factory()->AddResponse(
      kFailureURL, network::mojom::URLResponseHead::New(), std::string(),
      network::URLLoaderCompletionStatus(net::ERR_SSL_PROTOCOL_ERROR));

  CertificateErrorReporter* certificate_error_reporter =
      new CertificateErrorReporter(test_shared_loader_factory(), kFailureURL);

  CertificateReportingService::BoundedReportList* list =
      new CertificateReportingService::BoundedReportList(2);

  // Create a reporter with retries disabled.
  CertificateReportingService::Reporter reporter(
      std::unique_ptr<CertificateErrorReporter>(certificate_error_reporter),
      std::unique_ptr<CertificateReportingService::BoundedReportList>(list),
      clock.get(), base::Seconds(100), false /* retries_enabled */);
  EXPECT_EQ(0u, list->items().size());
  EXPECT_EQ(0u, reporter.inflight_report_count_for_testing());

  // Sending a failed report will not put the report in the retry list.
  reporter.Send("report1");
  EXPECT_EQ(1u, reporter.inflight_report_count_for_testing());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, reporter.inflight_report_count_for_testing());
  ASSERT_EQ(0u, list->items().size());

  // Sending a second failed report will also not put it in the retry list.
  clock->Advance(base::Seconds(10));
  reporter.Send("report2");
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, list->items().size());

  // Send pending reports. Nothing should be sent.
  clock->Advance(base::Seconds(10));
  reporter.SendPending();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, list->items().size());

  // report1 and report2 were both submitted once, failed once.
  event_histogram_tester()->SetExpectedValues(
      2 /* submitted */, 2 /* failed */, 0 /* successful */, 0 /* dropped */);
}

class CertificateReportingServiceTest : public ::testing::Test {
 public:
  CertificateReportingServiceTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        io_task_runner_(content::GetIOThreadTaskRunner({})) {}

  ~CertificateReportingServiceTest() override {}

  void SetUp() override {
    service_observer_.Clear();

    safe_browsing::SafeBrowsingService::RegisterFactory(&sb_service_factory);
    sb_service_ = sb_service_factory.CreateSafeBrowsingService();

    test_helper_ =
        base::MakeRefCounted<CertificateReportingServiceTestHelper>();

    clock_ = std::make_unique<base::SimpleTestClock>();
    service_ = std::make_unique<CertificateReportingService>(
        sb_service_.get(), test_helper_, &profile_,
        test_helper_->server_public_key(),
        test_helper_->server_public_key_version(), kMaxReportCountInQueue,
        base::Hours(24), clock_.get(),
        base::BindRepeating(
            &CertificateReportingServiceObserver::OnServiceReset,
            base::Unretained(&service_observer_)));
    service_observer_.WaitForReset();

    event_histogram_tester_ = std::make_unique<EventHistogramTester>();
  }

  void TearDown() override {
    test_helper()->ExpectNoRequests(service());

    service_->Shutdown();
    service_.reset(nullptr);
    safe_browsing::SafeBrowsingService::RegisterFactory(nullptr);

    event_histogram_tester_.reset();
  }

 protected:
  CertificateReportingService* service() { return service_.get(); }

  // Sets service enabled state and waits for a reset event.
  void SetServiceEnabledAndWait(bool enabled) {
    service_observer_.Clear();
    service()->SetEnabled(enabled);
    service_observer_.WaitForReset();
  }

  void AdvanceClock(base::TimeDelta delta) { clock_->Advance(delta); }

  void SetNow(base::Time now) { clock_->SetNow(now); }

  CertificateReportingServiceTestHelper* test_helper() {
    return test_helper_.get();
  }

  EventHistogramTester* event_histogram_tester() {
    return event_histogram_tester_.get();
  }

  void WaitForNoReports() {
    if (!service_->GetReporterForTesting()->inflight_report_count_for_testing())
      return;

    base::RunLoop run_loop;
    service_->GetReporterForTesting()
        ->SetClosureWhenNoInflightReportsForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  std::unique_ptr<CertificateReportingService> service_;
  std::unique_ptr<base::SimpleTestClock> clock_;

  scoped_refptr<safe_browsing::SafeBrowsingService> sb_service_;
  safe_browsing::TestSafeBrowsingServiceFactory sb_service_factory;
  TestingProfile profile_;

  scoped_refptr<CertificateReportingServiceTestHelper> test_helper_;
  CertificateReportingServiceObserver service_observer_;

  std::unique_ptr<EventHistogramTester> event_histogram_tester_;
};

TEST_F(CertificateReportingServiceTest, SendSuccessful) {
  // Let all reports succeed.
  test_helper()->SetFailureMode(certificate_reporting_test_utils::
                                    ReportSendingResult::REPORTS_SUCCESSFUL);

  service()->Send(MakeReport("report0"));
  service()->Send(MakeReport("report1"));
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"report0", RetryStatus::NOT_RETRIED},
                                     {"report1", RetryStatus::NOT_RETRIED}}));

  WaitForNoReports();
  // report0 and report1 were both submitted once, succeeded once.
  event_histogram_tester()->SetExpectedValues(
      2 /* submitted */, 0 /* failed */, 2 /* successful */, 0 /* dropped */);
}

TEST_F(CertificateReportingServiceTest, SendFailure) {
  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send two reports. Both should fail and get queued.
  service()->Send(MakeReport("report0"));
  service()->Send(MakeReport("report1"));
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report0", RetryStatus::NOT_RETRIED},
                                 {"report1", RetryStatus::NOT_RETRIED}}));

  // Need this to ensure the pending reports are queued.
  WaitForNoReports();

  // Send pending reports. Previously queued reports should be queued again.
  service()->SendPending();
  test_helper()->WaitForRequestsDestroyed(ReportExpectation::Failed(
      {{"report0", RetryStatus::RETRIED}, {"report1", RetryStatus::RETRIED}}));

  // Let all reports succeed.
  test_helper()->SetFailureMode(certificate_reporting_test_utils::
                                    ReportSendingResult::REPORTS_SUCCESSFUL);

  // Send a third report. This should not be queued.
  service()->Send(MakeReport("report2"));
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"report2", RetryStatus::NOT_RETRIED}}));

  // Need this to ensure the pending reports are queued.
  WaitForNoReports();

  // Send pending reports. Previously failed and queued two reports should be
  // observed.
  service()->SendPending();
  test_helper()->WaitForRequestsDestroyed(ReportExpectation::Successful(
      {{"report0", RetryStatus::RETRIED}, {"report1", RetryStatus::RETRIED}}));

  WaitForNoReports();
  // report0 and report1 were both submitted thrice, failed twice, succeeded
  // once. report2 was submitted once, succeeded once.
  event_histogram_tester()->SetExpectedValues(
      7 /* submitted */, 4 /* failed */, 3 /* successful */, 0 /* dropped */);
}

TEST_F(CertificateReportingServiceTest, Disabled_ShouldNotSend) {
  // Let all reports succeed.
  test_helper()->SetFailureMode(certificate_reporting_test_utils::
                                    ReportSendingResult::REPORTS_SUCCESSFUL);

  // Disable the service.
  SetServiceEnabledAndWait(false);

  // Send a report. Report attempt should be cancelled and no sent reports
  // should be observed.
  service()->Send(MakeReport("report0"));

  // Enable the service and send a report again. It should be sent successfully.
  SetServiceEnabledAndWait(true);

  service()->Send(MakeReport("report1"));
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"report1", RetryStatus::NOT_RETRIED}}));

  WaitForNoReports();
  // report0 was never sent. report1 was submitted once, succeeded once.
  event_histogram_tester()->SetExpectedValues(
      1 /* submitted */, 0 /* failed */, 1 /* successful */, 0 /* dropped */);
}

TEST_F(CertificateReportingServiceTest, Disabled_ShouldClearPendingReports) {
  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  service()->Send(MakeReport("report0"));
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report0", RetryStatus::NOT_RETRIED}}));

  // Need this to ensure the pending reports are queued.
  WaitForNoReports();

  // Disable the service.
  SetServiceEnabledAndWait(false);

  // Sending has no effect while disabled.
  service()->SendPending();

  // Re-enable the service and send pending reports. Pending reports should have
  // been cleared when the service was disabled, so no report should be seen.
  SetServiceEnabledAndWait(true);

  // Sending with empty queue has no effect.
  service()->SendPending();

  WaitForNoReports();
  // report0 was submitted once, failed once.
  event_histogram_tester()->SetExpectedValues(
      1 /* submitted */, 1 /* failed */, 0 /* successful */, 0 /* dropped */);
}

TEST_F(CertificateReportingServiceTest, DontSendOldReports) {
  SetNow(base::Time::Now());
  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send a report, then advance the clock and send another report.
  service()->Send(MakeReport("report0"));
  AdvanceClock(base::Hours(5));
  service()->Send(MakeReport("report1"));
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report0", RetryStatus::NOT_RETRIED},
                                 {"report1", RetryStatus::NOT_RETRIED}}));

  // Advance the clock to 20 hours, putting it 25 hours ahead of the reference
  // time. This makes the report0 older than max age (24 hours). The report1 is
  // now 20 hours old.
  AdvanceClock(base::Hours(20));

  // Need this to ensure the pending reports are queued.
  WaitForNoReports();

  // Send pending reports. report0 should be discarded since it's too old.
  // report1 should be queued again.
  service()->SendPending();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report1", RetryStatus::RETRIED}}));

  // Send a third report.
  service()->Send(MakeReport("report2"));
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report2", RetryStatus::NOT_RETRIED}}));

  // Advance the clock 5 hours. The report1 will now be 25 hours old.
  AdvanceClock(base::Hours(5));

  // Need this to ensure the pending reports are queued.
  WaitForNoReports();

  // Send pending reports. report1 should be discarded since it's too old.
  // report2 should be queued again.
  service()->SendPending();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report2", RetryStatus::RETRIED}}));

  // Advance the clock 20 hours again so that report2 is 25 hours old and is
  // older than max age (24 hours)
  AdvanceClock(base::Hours(20));

  // Need this to ensure the pending reports are queued.
  WaitForNoReports();

  // Send pending reports. report2 should be discarded since it's too old. No
  // other reports remain.
  service()->SendPending();

  WaitForNoReports();
  // report0 was submitted once, failed once, dropped once.
  // report1 was submitted twice, failed twice, dropped once.
  // report2 was submitted twice, failed twice, dropped once.
  // Need to wait for SendPending() to complete.
  event_histogram_tester()->SetExpectedValues(
      5 /* submitted */, 5 /* failed */, 0 /* successful */, 3 /* dropped */);
}

TEST_F(CertificateReportingServiceTest, DiscardOldReports) {
  SetNow(base::Time::Now());
  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send three more reports within five hours of each other. After this:
  // report0 is 0 hours after reference time (15 hours old).
  // report1 is 5 hours after reference time (10 hours old).
  // report2 is 10 hours after reference time (5 hours old).
  // report3 is 15 hours after reference time (0 hours old).
  service()->Send(MakeReport("report0"));

  AdvanceClock(base::Hours(5));
  service()->Send(MakeReport("report1"));

  AdvanceClock(base::Hours(5));
  service()->Send(MakeReport("report2"));

  AdvanceClock(base::Hours(5));
  service()->Send(MakeReport("report3"));
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report0", RetryStatus::NOT_RETRIED},
                                 {"report1", RetryStatus::NOT_RETRIED},
                                 {"report2", RetryStatus::NOT_RETRIED},
                                 {"report3", RetryStatus::NOT_RETRIED}}));

  // Need this to ensure the pending reports are queued.
  WaitForNoReports();

  // Send pending reports. Four reports were generated above, but the service
  // only queues three reports, so the very first one should be dropped since
  // it's the oldest.
  service()->SendPending();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report1", RetryStatus::RETRIED},
                                 {"report2", RetryStatus::RETRIED},
                                 {"report3", RetryStatus::RETRIED}}));

  // Let all reports succeed.
  test_helper()->SetFailureMode(certificate_reporting_test_utils::
                                    ReportSendingResult::REPORTS_SUCCESSFUL);

  // Advance the clock by 15 hours. Current time is now 30 hours after reference
  // time. The ages of reports are now as follows:
  // report1 is 25 hours old.
  // report2 is 20 hours old.
  // report3 is 15 hours old.
  AdvanceClock(base::Hours(15));

  // Need this to ensure the pending reports are queued.
  WaitForNoReports();

  // Send pending reports. Only report2 and report3 should be sent, report1
  // should be ignored because it's too old.
  service()->SendPending();
  test_helper()->WaitForRequestsDestroyed(ReportExpectation::Successful(
      {{"report2", RetryStatus::RETRIED}, {"report3", RetryStatus::RETRIED}}));

  // Do a final send. No reports should be sent.
  service()->SendPending();

  WaitForNoReports();
  // report0 was submitted once, failed once, dropped once.
  // report1 was submitted twice, failed twice, dropped once.
  // report2 was submitted thrice, failed twice, succeeded once.
  // report3 was submitted thrice, failed twice, succeeded once.
  event_histogram_tester()->SetExpectedValues(
      9 /* submitted */, 7 /* failed */, 2 /* successful */, 2 /* dropped */);
}

// A delayed report should successfully upload when it's resumed.
TEST_F(CertificateReportingServiceTest, Delayed_Resumed) {
  // Let reports hang.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_DELAY);
  // Send a report. The report upload hangs, so no error or success callbacks
  // should be called.
  service()->Send(MakeReport("report0"));

  // Resume the report upload and run the callbacks. The report should be
  // successfully sent.
  test_helper()->ResumeDelayedRequest();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Delayed({{"report0", RetryStatus::NOT_RETRIED}}));

  WaitForNoReports();
  // report0 was submitted once, succeeded once.
  event_histogram_tester()->SetExpectedValues(
      1 /* submitted */, 0 /* failed */, 1 /* successful */, 0 /* dropped */);
}

// Delayed reports should cleaned when the service is reset.
TEST_F(CertificateReportingServiceTest, Delayed_Reset) {
  // Let reports hang.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_DELAY);
  // Send a report. The report is triggered but hangs, so no error or success
  // callbacks should be called.
  service()->Send(MakeReport("report0"));

  // Disable the service. This should reset the reporting service and
  // clear all pending reports.
  SetServiceEnabledAndWait(false);

  // Resume delayed report. No report should be observed since the service
  // should have reset and all pending reports should be cleared. If any report
  // is observed, the next test_helper()->WaitForRequestsDestroyed() will fail.
  test_helper()->ResumeDelayedRequest();

  // Enable the service.
  SetServiceEnabledAndWait(true);

  // Send a report. The report is triggered but hangs, so no error or success
  // callbacks should be called. The report id is again 0 since the pending
  // report queue has been cleared above.
  service()->Send(MakeReport("report1"));

  // Resume delayed report. Two reports are successfully sent.
  test_helper()->ResumeDelayedRequest();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Delayed({{"report0", RetryStatus::NOT_RETRIED},
                                  {"report1", RetryStatus::NOT_RETRIED}}));

  WaitForNoReports();
  // report0 was submitted once, but neither failed nor succeeded because the
  // report queue was cleared. report1 was submitted once, succeeded once.
  event_histogram_tester()->SetExpectedValues(
      2 /* submitted */, 0 /* failed */, 1 /* successful */, 0 /* dropped */);
}
