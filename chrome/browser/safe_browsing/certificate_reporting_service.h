// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ssl/certificate_error_reporter.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefService;
class Profile;

namespace base {
class Clock;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace safe_browsing {
class SafeBrowsingService;
}

// This service initiates uploads of invalid certificate reports and retries any
// failed uploads. Each report is retried until it's older than a certain time
// to live (TTL). Reports older than this TTL are dropped and no more retried,
// so that the retry list doesn't grow indefinitely.
//
// Lifetime and dependencies:
//
// CertificateReportingService uses the URLLoaderFactory from SafeBrowsing
// service. SafeBrowsingService is created before CertificateReportingService,
// but is also shut down before any KeyedService is shut down.
//
// This class observes SafeBrowsing preference changes to enable/disable
// reporting. It does this by subscribing to changes in SafeBrowsing and
// extended reporting preferences.
class CertificateReportingService : public KeyedService {
 public:
  // Events for UMA. Do not rename or remove values, add new values to the end.
  // Public for testing.
  enum class ReportOutcome {
    // A report is submitted. This includes failed and successful uploads as
    // well as uploads that never return a response.
    SUBMITTED = 0,
    // A report submission failed, either because of a net error or a non-HTTP
    // 200 response from the server.
    FAILED = 1,
    // A report submission was successfully sent, receiving an HTTP 200 response
    // from the server.
    SUCCESSFUL = 2,
    // A report was dropped from the reporting queue because it was older
    // than report TTL, or it was ignored because the queue was full and the
    // report was older than the oldest report in the queue. Does not include
    // reports that were cleared because of a SafeBrowsing preference change.
    DROPPED_OR_IGNORED = 3,
    EVENT_COUNT = 4
  };

  // Represents a report to be sent.
  struct Report {
    int report_id;
    base::Time creation_time;
    std::string serialized_report;
    bool is_retried;
    Report(int report_id,
           base::Time creation_time,
           const std::string& serialized_report)
        : report_id(report_id),
          creation_time(creation_time),
          serialized_report(serialized_report),
          is_retried(false) {}
  };

  // This class contains a number of reports, sorted by the first time the
  // report was to be sent. Oldest reports are at the end of the list. The
  // number of reports are bounded by |max_size|. The implementation sorts all
  // items in the list whenever a new item is added. This should be fine for
  // small values of |max_size| (e.g. fewer than 100 items). In case this is not
  // sufficient in the future, an array based implementation should be
  // considered where the array is maintained as a heap.
  class BoundedReportList {
   public:
    explicit BoundedReportList(size_t max_size);
    ~BoundedReportList();

    void Add(const Report& report);
    void Clear();

    const std::vector<Report>& items() const;

   private:
    // Maximum number of reports in the list. If the number of reports in the
    // list is smaller than this number, a new item is immediately added to the
    // list. Otherwise, the item is compared to the items in the list and only
    // added when it's newer than the oldest item in the list.
    const size_t max_size_;

    std::vector<Report> items_;
    base::ThreadChecker thread_checker_;

    DISALLOW_COPY_AND_ASSIGN(BoundedReportList);
  };

  // Class that handles report uploads and implements the upload retry logic.
  class Reporter {
   public:
    Reporter(std::unique_ptr<CertificateErrorReporter> error_reporter_,
             std::unique_ptr<BoundedReportList> retry_list,
             base::Clock* const clock,
             base::TimeDelta report_ttl,
             bool retries_enabled);
    ~Reporter();

    // Sends a report. If the send fails, the report will be added to the retry
    // list.
    void Send(const std::string& serialized_report);

    // Sends all pending reports. Skips reports older than the |report_ttl|
    // provided in the constructor. Failed reports will be added to the retry
    // list.
    void SendPending();

    // Getter and setters for testing:
    size_t inflight_report_count_for_testing() const;
    BoundedReportList* GetQueueForTesting() const;
    // Sets a closure that is called when there are no more inflight reports.
    void SetClosureWhenNoInflightReportsForTesting(
        const base::Closure& closure);

   private:
    void SendInternal(const Report& report);
    // Called when a report upload fails either because of a net error or a
    // non-HTTP 200 response code. See
    // TransportSecurityState::ReportSenderInterface for parameters.
    void ErrorCallback(int report_id,
                       int net_error,
                       int http_response_code);
    // Called when a report upload is successful.
    void SuccessCallback(int report_id);

    std::unique_ptr<CertificateErrorReporter> error_reporter_;
    std::unique_ptr<BoundedReportList> retry_list_;
    base::Clock* const clock_;
    // Maximum age of a queued report. Reports older than this are discarded in
    // the next SendPending() call.
    const base::TimeDelta report_ttl_;
    const bool retries_enabled_;
    // Current report id, starting from zero and monotonically incrementing.
    int current_report_id_;

    std::map<int, Report> inflight_reports_;

    base::Closure no_in_flight_reports_;

    base::WeakPtrFactory<Reporter> weak_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(Reporter);
  };

  // Public for testing.
  static const char kReportEventHistogram[];

  CertificateReportingService(
      safe_browsing::SafeBrowsingService* safe_browsing_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      uint8_t server_public_key[/* 32 */],
      uint32_t server_public_key_version,
      size_t max_queued_report_count,
      base::TimeDelta max_report_age,
      base::Clock* clock,
      const base::Callback<void()>& reset_callback);

  ~CertificateReportingService() override;

  // KeyedService implementation:
  void Shutdown() override;

  // Sends a serialized report. If the report upload fails, the upload is
  // retried at a future time.
  void Send(const std::string& serialized_report);

  // Sends pending reports that are in the retry queue.
  void SendPending();

  // Enables or disables reporting. When disabled, pending report queue is
  // cleared and incoming reports are ignored. Reporting is enabled by default
  // once the service is initialized.
  void SetEnabled(bool enabled);

  // Getters for testing.
  Reporter* GetReporterForTesting() const;
  static GURL GetReportingURLForTesting();

 private:
  // Resets the reporter. Changes in SafeBrowsing or extended reporting enabled
  // states cause the reporter to be reset. If |enabled| is false, report is set
  // to null, effectively cancelling all in flight uploads and clearing the
  // pending reports queue.
  void Reset(bool enabled);

  void OnPreferenceChanged();

  const PrefService& pref_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<Reporter> reporter_;

  // Subscription for state changes. When this subscription is notified, it
  // means SafeBrowsingService is enabled/disabled or one of the preferences
  // related to it is changed.
  std::unique_ptr<base::CallbackList<void(void)>::Subscription>
      safe_browsing_state_subscription_;

  // Maximum number of reports to be queued for retry.
  const size_t max_queued_report_count_;

  // Maximum age of the reports to be queued for retry, from the time the
  // certificate error was first encountered by the user. Any report older than
  // this age is ignored and is not re-uploaded.
  const base::TimeDelta max_report_age_;

  base::Clock* const clock_;

  // Called when the service is reset. Used for testing.
  base::Callback<void()> reset_callback_;

  // Encryption parameters.
  uint8_t* server_public_key_;
  uint32_t server_public_key_version_;

  DISALLOW_COPY_AND_ASSIGN(CertificateReportingService);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_SERVICE_H_
