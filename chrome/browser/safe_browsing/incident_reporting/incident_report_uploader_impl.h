// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_REPORT_UPLOADER_IMPL_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_REPORT_UPLOADER_IMPL_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_report_uploader.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace safe_browsing {

class ClientIncidentReport;

// An uploader of incident reports. A report is issued via the UploadReport
// method. The upload can be cancelled by deleting the returned instance. The
// instance is no longer usable after the delegate is notified, and may safely
// be destroyed by the delegate.
class IncidentReportUploaderImpl : public IncidentReportUploader {
 public:
  // The id associated with the URLFetcher, for use by tests.
  static const int kTestUrlFetcherId;

  IncidentReportUploaderImpl(const IncidentReportUploaderImpl&) = delete;
  IncidentReportUploaderImpl& operator=(const IncidentReportUploaderImpl&) =
      delete;

  ~IncidentReportUploaderImpl() override;

  // Uploads a report with a caller-provided URL context. |callback| will be run
  // when the upload is complete. Returns NULL if |report| cannot be serialized
  // for transmission, in which case the delegate is not notified.
  static std::unique_ptr<IncidentReportUploader> UploadReport(
      OnResultCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      const ClientIncidentReport& report);

 private:
  FRIEND_TEST_ALL_PREFIXES(IncidentReportUploaderImplTest, Success);

  IncidentReportUploaderImpl(
      OnResultCallback callback,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      const std::string& post_data);

  // Returns the URL to which incident reports are to be sent.
  static GURL GetIncidentReportUrl();

  // Callback when SimpleURLLoader gets the response.
  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  void OnURLLoaderCompleteInternal(const std::string& response_body,
                                   int response_code,
                                   int net_error);

  // The underlying URLLoader. The instance is alive from construction through
  // OnURLLoaderComplete.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // The time at which the upload was initiated.
  base::TimeTicks time_begin_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_REPORT_UPLOADER_IMPL_H_
