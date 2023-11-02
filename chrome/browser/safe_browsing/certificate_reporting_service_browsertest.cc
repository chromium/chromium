// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/certificate_reporting_service.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/thread_test_helper.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_test_utils.h"
#include "chrome/browser/ssl/certificate_reporting_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/security_interstitials/content/cert_report_helper.h"
#include "components/security_interstitials/content/certificate_error_report.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/scheme_host_port.h"

using certificate_reporting_test_utils::CertificateReportingServiceTestHelper;
using certificate_reporting_test_utils::CertificateReportingServiceObserver;
using certificate_reporting_test_utils::EventHistogramTester;
using certificate_reporting_test_utils::ReportExpectation;
using certificate_reporting_test_utils::RetryStatus;

namespace safe_browsing {

// These tests check the whole mechanism to send and queue invalid certificate
// reports. Each test triggers reports by visiting broken SSL pages. The reports
// succeed, fail or hang indefinitely:
// - If a report is expected to fail or succeed, the test waits for the
//   corresponding URL request jobs to be destroyed.
// - If a report is expected to hang, the test waits for the corresponding URL
//   request job to be created. Only after resuming the hung request job the
//   test waits for the request to be destroyed.
class CertificateReportingServiceBrowserTest : public InProcessBrowserTest {
 public:
  CertificateReportingServiceBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    CertReportHelper::SetFakeOfficialBuildForTesting();

    // Setting the sending threshold to 1.0 ensures reporting is enabled.
    variations::testing::VariationParamsManager::SetVariationParams(
        "ReportCertificateErrors", "ShowAndPossiblySend",
        {{"sendingThreshold", "1.0"}});
  }

  CertificateReportingServiceBrowserTest(
      const CertificateReportingServiceBrowserTest&) = delete;
  CertificateReportingServiceBrowserTest& operator=(
      const CertificateReportingServiceBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    test_helper_ =
        base::MakeRefCounted<CertificateReportingServiceTestHelper>();

    CertificateReportingServiceFactory::GetInstance()
        ->SetReportEncryptionParamsForTesting(
            test_helper()->server_public_key(),
            test_helper()->server_public_key_version());
    CertificateReportingServiceFactory::GetInstance()
        ->SetServiceResetCallbackForTesting(base::BindRepeating(
            &CertificateReportingServiceObserver::OnServiceReset,
            base::Unretained(&service_observer_)));
    CertificateReportingServiceFactory::GetInstance()
        ->SetURLLoaderFactoryForTesting(test_helper_);

    event_histogram_tester_ = std::make_unique<EventHistogramTester>();
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    test_helper()->ExpectNoRequests(service());
    EXPECT_GE(num_expected_failed_report_, 0)
        << "Don't forget to set expected failed report count.";
    event_histogram_tester_.reset();
  }

  CertificateReportingServiceTestHelper* test_helper() {
    return test_helper_.get();
  }

  void WaitForNoReports() {
    if (!service()->GetReporterForTesting() ||
        !service()
             ->GetReporterForTesting()
             ->inflight_report_count_for_testing())
      return;

    base::RunLoop run_loop;
    service()
        ->GetReporterForTesting()
        ->SetClosureWhenNoInflightReportsForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

 protected:
  CertificateReportingServiceFactory* factory() {
    return CertificateReportingServiceFactory::GetInstance();
  }

  // Sends a report using the provided hostname. Navigates to an interstitial
  // page on this hostname and away from it to trigger a report.
  void SendReport(const std::string& hostname) {
    // Create an HTTPS URL from the hostname. This will resolve to the HTTPS
    // server and cause an SSL error.
    const GURL kCertErrorURL(
        url::SchemeHostPort("https", hostname, https_server_.port()).GetURL());

    // Navigate to the page with SSL error.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kCertErrorURL));

    // Navigate away from the interstitial to trigger report upload.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  }

  void SendPendingReports() {
    WaitForNoReports();
    service()->SendPending();
  }

  // Changes opt-in status and waits for the cert reporting service to reset.
  // Can only be used after the service is initialized. When changing the
  // value at the beginning of a test,
  // certificate_reporting_test_utils::SetCertReportingOptIn should be used
  // instead since the service is only created upon first SSL error.
  // Changing the opt-in status synchronously fires
  // CertificateReportingService::PreferenceObserver::OnPreferenceChanged which
  // will call CertificateReportingService::SetEnabled() which in turn posts
  // a task to the IO thread to reset the service. Waiting for the IO thread
  // ensures that the service is reset before returning from this method.
  void ChangeOptInAndWait(certificate_reporting_test_utils::OptIn opt_in) {
    service_observer_.Clear();
    certificate_reporting_test_utils::SetCertReportingOptIn(browser(), opt_in);
    service_observer_.WaitForReset();
  }

  // Same as ChangeOptInAndWait, but enables/disables SafeBrowsing instead.
  void ToggleSafeBrowsingAndWaitForServiceReset(bool safebrowsing_enabled) {
    service_observer_.Clear();
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                 safebrowsing_enabled);
    service_observer_.WaitForReset();
  }

  void SetExpectedHistogramCountOnTeardown(int num_expected_failed_report) {
    num_expected_failed_report_ = num_expected_failed_report;
  }

  CertificateReportingService* service() const {
    return CertificateReportingServiceFactory::GetForBrowserContext(
        browser()->profile());
  }

  EventHistogramTester* event_histogram_tester() {
    return event_histogram_tester_.get();
  }

 private:
  // Checks that the serialized reports in |received_reports| have the same
  // hostnames as |expected_hostnames|.
  void CheckReports(const std::set<std::string>& expected_hostnames,
                    const std::set<std::string>& received_reports,
                    const std::string type) {
    std::set<std::string> received_hostnames;
    for (const std::string& serialized_report : received_reports) {
      CertificateErrorReport report;
      ASSERT_TRUE(report.InitializeFromString(serialized_report));
      received_hostnames.insert(report.hostname());
    }
    EXPECT_EQ(expected_hostnames, received_hostnames) << type
                                                      << " comparison failed";
  }

  net::EmbeddedTestServer https_server_;

  int num_expected_failed_report_ = -1;

  scoped_refptr<CertificateReportingServiceTestHelper> test_helper_;

  CertificateReportingServiceObserver service_observer_;

  base::HistogramTester histogram_tester_;
  // Histogram tester for reporting events. This is a member instead of a local
  // so that we can check the histogram after the test teardown. At that point
  // all in flight reports should be completed or deleted because
  // of CleanUpOnIOThread().
  std::unique_ptr<EventHistogramTester> event_histogram_tester_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that report send attempt should be cancelled when extended
// reporting is not opted in.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       NotOptedIn_ShouldNotSendReports) {
  SetExpectedHistogramCountOnTeardown(0);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(),
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN);
  // Send a report. Test teardown checks for created and in-flight requests. If
  // a report was incorrectly sent, the test will fail.
  SendReport("no-report");

  event_histogram_tester()->SetExpectedValues(0, 0, 0, 0);
}

// Tests that report send attempts are not cancelled when extended reporting is
// opted in. Goes to an interstitial page and navigates away to force a report
// send event.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       OptedIn_ShouldSendSuccessfulReport) {
  SetExpectedHistogramCountOnTeardown(0);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);

  // Let reports uploads successfully complete.
  test_helper()->SetFailureMode(certificate_reporting_test_utils::
                                    ReportSendingResult::REPORTS_SUCCESSFUL);

  // Reporting is opted in, so the report should succeed.
  SendReport("report0");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"report0", RetryStatus::NOT_RETRIED}}));

  // report0 was successfully submitted.
  event_histogram_tester()->SetExpectedValues(
      1 /* submitted */, 0 /* failed */, 1 /* successful */, 0 /* dropped */);
}

// Tests that report send attempts are not cancelled when extended reporting is
// opted in. Goes to an interstitial page and navigate away to force a report
// send event. Repeats this three times and checks expected number of reports.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       OptedIn_ShouldQueueFailedReport) {
  SetExpectedHistogramCountOnTeardown(2);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);
  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send a failed report.
  SendReport("report0");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report0", RetryStatus::NOT_RETRIED}}));

  // Send another failed report.
  SendReport("report1");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report1", RetryStatus::NOT_RETRIED}}));

  // Let all report uploads complete successfully now.
  test_helper()->SetFailureMode(certificate_reporting_test_utils::
                                    ReportSendingResult::REPORTS_SUCCESSFUL);

  // Send another report. This time the report should be successfully sent.
  SendReport("report2");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"report2", RetryStatus::NOT_RETRIED}}));

  // Send all pending reports. The two previously failed reports should have
  // been queued, and now be sent successfully.
  SendPendingReports();
  test_helper()->WaitForRequestsDestroyed(ReportExpectation::Successful(
      {{"report0", RetryStatus::RETRIED}, {"report1", RetryStatus::RETRIED}}));

  // Try sending pending reports again. Since there is no pending report,
  // nothing should be sent this time. If any report is sent, test teardown
  // will catch it.
  SendPendingReports();

  // report0 was submitted twice, failed once, succeeded once.
  // report1 was submitted twice, failed once, succeeded once.
  // report2 was submitted once, succeeded once.
  event_histogram_tester()->SetExpectedValues(
      5 /* submitted */, 2 /* failed */, 3 /* successful */, 0 /* dropped */);
}

// Opting in then opting out of extended reporting should clear the pending
// report queue.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       OptedIn_ThenOptedOut) {
  SetExpectedHistogramCountOnTeardown(1);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);
  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send a failed report.
  SendReport("report0");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report0", RetryStatus::NOT_RETRIED}}));

  // Disable reporting. This should clear all pending reports.
  ChangeOptInAndWait(
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN);

  // Send pending reports. No reports should be observed during test teardown.
  SendPendingReports();

  // report0 was submitted once and failed once.
  event_histogram_tester()->SetExpectedValues(
      1 /* submitted */, 1 /* failed */, 0 /* successful */, 0 /* dropped */);
}

// Opting out, then in, then out of extended reporting should work as expected.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       OptedOut_ThenOptedIn_ThenOptedOut) {
  SetExpectedHistogramCountOnTeardown(1);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(),
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN);
  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send attempt should be cancelled since reporting is opted out.
  SendReport("no-report");
  test_helper()->ExpectNoRequests(service());

  // Enable reporting.
  ChangeOptInAndWait(
      certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);

  // A failed report should be observed.
  SendReport("report0");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report0", RetryStatus::NOT_RETRIED}}));

  // Disable reporting. This should reset the reporting service and
  // clear all pending reports.
  ChangeOptInAndWait(
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN);

  // Report should be cancelled since reporting is opted out.
  SendReport("report1");
  test_helper()->ExpectNoRequests(service());

  // Send pending reports. Nothing should be sent since there aren't any
  // pending reports. If any report is sent, test teardown will catch it.
  SendPendingReports();

  // report0 was submitted once and failed once.
  // report1 was never submitted.
  event_histogram_tester()->SetExpectedValues(
      1 /* submitted */, 1 /* failed */, 0 /* successful */, 0 /* dropped */);
}

// Disabling SafeBrowsing should clear pending reports queue in
// CertificateReportingService.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       DisableSafebrowsing) {
  SetExpectedHistogramCountOnTeardown(2);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);
  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send a report.
  SendReport("report0");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report0", RetryStatus::NOT_RETRIED}}));

  // Disable SafeBrowsing. This should clear all pending reports.
  ToggleSafeBrowsingAndWaitForServiceReset(false);

  // Send pending reports. No reports should be observed.
  SendPendingReports();
  test_helper()->ExpectNoRequests(service());

  // Re-enable SafeBrowsing and trigger another report which will be queued.
  ToggleSafeBrowsingAndWaitForServiceReset(true);
  SendReport("report1");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report1", RetryStatus::NOT_RETRIED}}));

  // Queued report should now be successfully sent.
  test_helper()->SetFailureMode(certificate_reporting_test_utils::
                                    ReportSendingResult::REPORTS_SUCCESSFUL);
  SendPendingReports();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"report1", RetryStatus::RETRIED}}));

  WaitForNoReports();

  // report0 was submitted once, failed once, then cleared.
  // report1 was submitted twice, failed once, succeeded once.
  event_histogram_tester()->SetExpectedValues(
      3 /* submitted */, 2 /* failed */, 1 /* successful */, 0 /* dropped */);
}

// CertificateReportingService should ignore reports older than the report TTL.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       DontSendOldReports) {
  SetExpectedHistogramCountOnTeardown(5);

  base::SimpleTestClock clock;
  base::Time reference_time = base::Time::Now();
  clock.SetNow(reference_time);
  factory()->SetClockForTesting(&clock);

  // The service should ignore reports older than 24 hours.
  factory()->SetQueuedReportTTLForTesting(base::Hours(24));

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);

  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send a failed report.
  SendReport("report0");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report0", RetryStatus::NOT_RETRIED}}));

  // Advance the clock a bit and trigger another failed report.
  clock.Advance(base::Hours(5));
  SendReport("report1");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report1", RetryStatus::NOT_RETRIED}}));

  // Advance the clock to 20 hours, putting it 25 hours ahead of the reference
  // time. This makes report0 older than 24 hours. report1 is now 20 hours.
  clock.Advance(base::Hours(20));

  // Send pending reports. report0 should be discarded since it's too old.
  // report1 should be queued again.
  SendPendingReports();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report1", RetryStatus::RETRIED}}));

  // Trigger another failed report.
  SendReport("report2");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report2", RetryStatus::NOT_RETRIED}}));

  // Advance the clock 5 hours. report1 will now be 25 hours old.
  clock.Advance(base::Hours(5));

  // Send pending reports. report1 should be discarded since it's too old.
  // report2 should be queued again.
  SendPendingReports();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report2", RetryStatus::RETRIED}}));

  // Advance the clock 20 hours again so that report2 is 25 hours old and is
  // older than max age (24 hours).
  clock.Advance(base::Hours(20));

  // Send pending reports. report2 should be discarded since it's too old. No
  // other reports remain. If any report is sent, test teardown will catch it.
  SendPendingReports();

  // Let all reports succeed and send a single report. This is to make sure
  // that any (incorrectly) pending reports are dropped before the test tear
  // down.
  test_helper()->SetFailureMode(certificate_reporting_test_utils::
                                    ReportSendingResult::REPORTS_SUCCESSFUL);
  SendReport("report3");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"report3", RetryStatus::NOT_RETRIED}}));

  WaitForNoReports();

  // report0 was submitted once, failed once, dropped once.
  // report1 was submitted twice, failed twice, dropped once.
  // report2 was submitted twice, failed twice, dropped once.
  // report3 was submitted once, successful once.
  event_histogram_tester()->SetExpectedValues(
      6 /* submitted */, 5 /* failed */, 1 /* successful */, 3 /* dropped */);
}

// CertificateReportingService should drop old reports from its pending report
// queue, if the queue is full.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       DropOldReportsFromQueue) {
  SetExpectedHistogramCountOnTeardown(7);

  base::SimpleTestClock clock;
  base::Time reference_time = base::Time::Now();
  clock.SetNow(reference_time);
  factory()->SetClockForTesting(&clock);

  // The service should queue a maximum of 3 reports and ignore reports older
  // than 24 hours.
  factory()->SetQueuedReportTTLForTesting(base::Hours(24));
  factory()->SetMaxQueuedReportCountForTesting(3);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);

  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Trigger a failed report.
  SendReport("report0");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report0", RetryStatus::NOT_RETRIED}}));

  // Trigger three more reports within five hours of each other. After this:
  // report0 is 0 hours after reference time (15 hours old).
  // report1 is 5 hours after reference time (10 hours old).
  // report2 is 10 hours after reference time (5 hours old).
  // report3 is 15 hours after reference time (0 hours old).
  clock.Advance(base::Hours(5));
  SendReport("report1");

  clock.Advance(base::Hours(5));
  SendReport("report2");

  clock.Advance(base::Hours(5));
  SendReport("report3");

  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report1", RetryStatus::NOT_RETRIED},
                                 {"report2", RetryStatus::NOT_RETRIED},
                                 {"report3", RetryStatus::NOT_RETRIED}}));

  // Send pending reports. Four reports were generated above, but the service
  // only queues three reports, so report0 should be dropped since it's the
  // oldest.
  SendPendingReports();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({{"report1", RetryStatus::RETRIED},
                                 {"report2", RetryStatus::RETRIED},
                                 {"report3", RetryStatus::RETRIED}}));

  // Let all reports succeed.
  test_helper()->SetFailureMode(certificate_reporting_test_utils::
                                    ReportSendingResult::REPORTS_SUCCESSFUL);

  // Advance the clock 15 hours. Current time is now 30 hours after reference
  // time, and the ages of reports are now as follows:
  // report1 is 25 hours old.
  // report2 is 20 hours old.
  // report3 is 15 hours old.
  clock.Advance(base::Hours(15));

  // Send pending reports. Only reports 2 and 3 should be sent, report 1
  // should be ignored because it's too old.
  SendPendingReports();
  test_helper()->WaitForRequestsDestroyed(ReportExpectation::Successful(
      {{"report2", RetryStatus::RETRIED}, {"report3", RetryStatus::RETRIED}}));

  WaitForNoReports();

  // report0 was submitted once, failed once, dropped once.
  // report1 was submitted twice, failed twice, dropped once.
  // report2 was submitted thrice, failed twice, succeeded once.
  // report3 was submitted thrice, failed twice, succeeded once.
  event_histogram_tester()->SetExpectedValues(
      9 /* submitted */, 7 /* failed */, 2 /* successful */, 2 /* dropped */);
}

IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       Delayed_Resumed) {
  SetExpectedHistogramCountOnTeardown(0);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);
  // Let all reports hang.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_DELAY);

  // Trigger a report that hangs.
  SendReport("report0");
  test_helper()->WaitForRequestsCreated(
      ReportExpectation::Delayed({{"report0", RetryStatus::NOT_RETRIED}}));

  // Resume the report upload. The report upload should successfully complete.
  // The interceptor only observes request creations and not response
  // completions, so there is nothing to observe.
  test_helper()->ResumeDelayedRequest();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Delayed({{"report0", RetryStatus::NOT_RETRIED}}));

  WaitForNoReports();

  // report0 was submitted once and succeeded once.
  event_histogram_tester()->SetExpectedValues(
      1 /* submitted */, 0 /* failed */, 1 /* successful */, 0 /* dropped */);
}

// Same as above, but the service is shut down before resuming the delayed
// request. Should not crash.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       Delayed_Resumed_ServiceShutdown) {
  SetExpectedHistogramCountOnTeardown(0);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);
  // Let all reports hang.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_DELAY);

  // Trigger a report that hangs.
  SendReport("report0");
  test_helper()->WaitForRequestsCreated(
      ReportExpectation::Delayed({{"report0", RetryStatus::NOT_RETRIED}}));

  // Shutdown the service and resume the report upload. Shouldn't crash.
  service()->Shutdown();
  test_helper()->ResumeDelayedRequest();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Delayed({{"report0", RetryStatus::NOT_RETRIED}}));

  // report0 was submitted once and never completed since the service shut down.
  event_histogram_tester()->SetExpectedValues(
      1 /* submitted */, 0 /* failed */, 0 /* successful */, 0 /* dropped */);
}

// Trigger a delayed report, then disable Safebrowsing. Certificate reporting
// service should clear its in-flight reports list.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest, Delayed_Reset) {
  SetExpectedHistogramCountOnTeardown(0);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);
  // Let all reports hang.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_DELAY);

  // Trigger a report that hangs.
  SendReport("report0");
  test_helper()->WaitForRequestsCreated(
      ReportExpectation::Delayed({{"report0", RetryStatus::NOT_RETRIED}}));

  // Disable SafeBrowsing. This should clear all pending reports.
  ToggleSafeBrowsingAndWaitForServiceReset(false);

  // In production, the request would have already went out to the network. For
  // this test, we manually resume it which will cause it to be cancelled.
  test_helper()->ResumeDelayedRequest();

  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Delayed({{"report0", RetryStatus::NOT_RETRIED}}));

  // Resume delayed report. No response should be observed since all pending
  // reports should be cleared.
  test_helper()->ResumeDelayedRequest();
  test_helper()->ExpectNoRequests(service());

  // Re-enable SafeBrowsing.
  ToggleSafeBrowsingAndWaitForServiceReset(true);

  // Trigger a report that hangs.
  SendReport("report1");
  test_helper()->WaitForRequestsCreated(
      ReportExpectation::Delayed({{"report1", RetryStatus::NOT_RETRIED}}));

  // Resume the delayed report and wait for it to complete.
  test_helper()->ResumeDelayedRequest();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Delayed({{"report1", RetryStatus::NOT_RETRIED}}));

  WaitForNoReports();

  // report0 was submitted once and delayed, then cleared.
  // report1 was submitted once and delayed, then succeeded.
  event_histogram_tester()->SetExpectedValues(
      2 /* submitted */, 0 /* failed */, 1 /* successful */, 0 /* dropped */);
}

IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       OmitsCredentials) {
  SetExpectedHistogramCountOnTeardown(0);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);
  // Make all reports succeed.
  test_helper()->SetFailureMode(certificate_reporting_test_utils::
                                    ReportSendingResult::REPORTS_SUCCESSFUL);

  // Trigger a report
  std::vector<network::ResourceRequest> full_requests;
  SendReport("report0");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({{"report0", RetryStatus::NOT_RETRIED}}),
      nullptr, &full_requests);

  ASSERT_EQ(full_requests.size(), 1u);
  EXPECT_EQ(full_requests[0].credentials_mode,
            network::mojom::CredentialsMode::kOmit);
  // report0 was successfully submitted.
  event_histogram_tester()->SetExpectedValues(
      1 /* submitted */, 0 /* failed */, 1 /* successful */, 0 /* dropped */);
}

}  // namespace safe_browsing
