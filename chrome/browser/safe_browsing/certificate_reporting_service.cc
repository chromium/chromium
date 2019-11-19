// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/certificate_reporting_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ssl/certificate_error_report.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// URL to upload invalid certificate chain reports. An HTTP URL is used because
// a client seeing an invalid cert might not be able to make an HTTPS connection
// to report it.
const char kExtendedReportingUploadUrl[] =
    "http://safebrowsing.googleusercontent.com/safebrowsing/clientreport/"
    "chrome-certs";

// Compare function that orders Reports in reverse chronological order (i.e.
// oldest item is last).
bool ReportCompareFunc(const CertificateReportingService::Report& item1,
                       const CertificateReportingService::Report& item2) {
  return item1.creation_time > item2.creation_time;
}

// Records an UMA histogram of the net errors when certificate reports
// fail to send.
void RecordUMAOnFailure(int net_error) {
  base::UmaHistogramSparse("SSL.CertificateErrorReportFailure", -net_error);
}

void RecordUMAEvent(CertificateReportingService::ReportOutcome outcome) {
  UMA_HISTOGRAM_ENUMERATION(
      CertificateReportingService::kReportEventHistogram, outcome,
      CertificateReportingService::ReportOutcome::EVENT_COUNT);
}

}  // namespace

const char CertificateReportingService::kReportEventHistogram[] =
    "SSL.CertificateErrorReportEvent";

CertificateReportingService::BoundedReportList::BoundedReportList(
    size_t max_size)
    : max_size_(max_size) {
  CHECK(max_size <= 20)
      << "Current implementation is not efficient for a large list.";
  DCHECK(thread_checker_.CalledOnValidThread());
}

CertificateReportingService::BoundedReportList::~BoundedReportList() {}

void CertificateReportingService::BoundedReportList::Add(const Report& item) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(items_.size() <= max_size_);
  if (items_.size() == max_size_) {
    const Report& last = items_.back();
    if (item.creation_time <= last.creation_time) {
      // Report older than the oldest item in the queue, ignore.
      RecordUMAEvent(ReportOutcome::DROPPED_OR_IGNORED);
      return;
    }
    // Reached the maximum item count, remove the oldest item.
    items_.pop_back();
    RecordUMAEvent(ReportOutcome::DROPPED_OR_IGNORED);
  }
  items_.push_back(item);
  std::sort(items_.begin(), items_.end(), ReportCompareFunc);
}

void CertificateReportingService::BoundedReportList::Clear() {
  items_.clear();
}

const std::vector<CertificateReportingService::Report>&
CertificateReportingService::BoundedReportList::items() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return items_;
}

CertificateReportingService::Reporter::Reporter(
    std::unique_ptr<CertificateErrorReporter> error_reporter,
    std::unique_ptr<BoundedReportList> retry_list,
    base::Clock* clock,
    base::TimeDelta report_ttl,
    bool retries_enabled)
    : error_reporter_(std::move(error_reporter)),
      retry_list_(std::move(retry_list)),
      clock_(clock),
      report_ttl_(report_ttl),
      retries_enabled_(retries_enabled),
      current_report_id_(0) {}

CertificateReportingService::Reporter::~Reporter() {}

void CertificateReportingService::Reporter::Send(
    const std::string& serialized_report) {
  SendInternal(Report(current_report_id_++, clock_->Now(), serialized_report));
}

void CertificateReportingService::Reporter::SendPending() {
  if (!retries_enabled_) {
    return;
  }
  const base::Time now = clock_->Now();
  // Copy pending reports and clear the retry list.
  std::vector<Report> items = retry_list_->items();
  retry_list_->Clear();
  for (Report& report : items) {
    if (report.creation_time < now - report_ttl_) {
      // Report too old, ignore.
      RecordUMAEvent(ReportOutcome::DROPPED_OR_IGNORED);
      continue;
    }
    if (!report.is_retried) {
      // If this is the first retry, deserialize the report, set its retry bit
      // and serialize again.
      CertificateErrorReport error_report;
      CHECK(error_report.InitializeFromString(report.serialized_report));
      error_report.SetIsRetryUpload(true);
      CHECK(error_report.Serialize(&report.serialized_report));
    }
    report.is_retried = true;
    SendInternal(report);
  }
}

size_t
CertificateReportingService::Reporter::inflight_report_count_for_testing()
    const {
  return inflight_reports_.size();
}

CertificateReportingService::BoundedReportList*
CertificateReportingService::Reporter::GetQueueForTesting() const {
  return retry_list_.get();
}

void CertificateReportingService::Reporter::
    SetClosureWhenNoInflightReportsForTesting(const base::Closure& closure) {
  no_in_flight_reports_ = closure;
}

void CertificateReportingService::Reporter::SendInternal(
    const CertificateReportingService::Report& report) {
  inflight_reports_.insert(std::make_pair(report.report_id, report));
  RecordUMAEvent(ReportOutcome::SUBMITTED);
  error_reporter_->SendExtendedReportingReport(
      report.serialized_report,
      base::BindOnce(&CertificateReportingService::Reporter::SuccessCallback,
                     weak_factory_.GetWeakPtr(), report.report_id),
      base::BindOnce(&CertificateReportingService::Reporter::ErrorCallback,
                     weak_factory_.GetWeakPtr(), report.report_id));
}

void CertificateReportingService::Reporter::ErrorCallback(
    int report_id,
    int net_error,
    int http_response_code) {
  RecordUMAOnFailure(net_error);
  RecordUMAEvent(ReportOutcome::FAILED);
  if (retries_enabled_) {
    auto it = inflight_reports_.find(report_id);
    DCHECK(it != inflight_reports_.end());
    retry_list_->Add(it->second);
  }
  CHECK_GT(inflight_reports_.erase(report_id), 0u);
  if (inflight_reports_.empty() && no_in_flight_reports_)
    no_in_flight_reports_.Run();
}

void CertificateReportingService::Reporter::SuccessCallback(int report_id) {
  RecordUMAEvent(ReportOutcome::SUCCESSFUL);
  CHECK_GT(inflight_reports_.erase(report_id), 0u);
  if (inflight_reports_.empty() && no_in_flight_reports_)
    no_in_flight_reports_.Run();
}

CertificateReportingService::CertificateReportingService(
    safe_browsing::SafeBrowsingService* safe_browsing_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    uint8_t server_public_key[/* 32 */],
    uint32_t server_public_key_version,
    size_t max_queued_report_count,
    base::TimeDelta max_report_age,
    base::Clock* clock,
    const base::Callback<void()>& reset_callback)
    : pref_service_(*profile->GetPrefs()),
      url_loader_factory_(url_loader_factory),
      max_queued_report_count_(max_queued_report_count),
      max_report_age_(max_report_age),
      clock_(clock),
      reset_callback_(reset_callback),
      server_public_key_(server_public_key),
      server_public_key_version_(server_public_key_version) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(clock_);

  // Subscribe to SafeBrowsing preference change notifications.
  safe_browsing_state_subscription_ =
      safe_browsing_service->RegisterStateCallback(
          base::Bind(&CertificateReportingService::OnPreferenceChanged,
                     base::Unretained(this)));

  Reset(true);
  reset_callback_.Run();
}

CertificateReportingService::~CertificateReportingService() {
  DCHECK(!reporter_);
}

void CertificateReportingService::Shutdown() {
  reporter_.reset();
}

void CertificateReportingService::Send(const std::string& serialized_report) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (reporter_)
    reporter_->Send(serialized_report);
}

void CertificateReportingService::SendPending() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (reporter_)
    reporter_->SendPending();
}

void CertificateReportingService::SetEnabled(bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Reset(enabled);
  reset_callback_.Run();
}

CertificateReportingService::Reporter*
CertificateReportingService::GetReporterForTesting() const {
  return reporter_.get();
}

// static
GURL CertificateReportingService::GetReportingURLForTesting() {
  return GURL(kExtendedReportingUploadUrl);
}

void CertificateReportingService::Reset(bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!enabled) {
    reporter_.reset();
    return;
  }
  std::unique_ptr<CertificateErrorReporter> error_reporter;
  if (server_public_key_) {
    error_reporter.reset(new CertificateErrorReporter(
        url_loader_factory_, GURL(kExtendedReportingUploadUrl),
        server_public_key_, server_public_key_version_));
  } else {
    error_reporter.reset(new CertificateErrorReporter(
        url_loader_factory_, GURL(kExtendedReportingUploadUrl)));
  }
  reporter_.reset(
      new Reporter(std::move(error_reporter),
                   std::unique_ptr<BoundedReportList>(
                       new BoundedReportList(max_queued_report_count_)),
                   clock_, max_report_age_, true /* retries_enabled */));
}

void CertificateReportingService::OnPreferenceChanged() {
  safe_browsing::SafeBrowsingService* safe_browsing_service_ =
      g_browser_process->safe_browsing_service();
  const bool enabled = safe_browsing_service_ &&
                       safe_browsing_service_->enabled_by_prefs() &&
                       safe_browsing::IsExtendedReportingEnabled(pref_service_);
  SetEnabled(enabled);
}
