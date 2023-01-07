// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_REPORT_UPLOADER_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_REPORT_UPLOADER_H_

#include <memory>

#include "base/functional/callback.h"

namespace safe_browsing {

class ClientIncidentResponse;

// An abstract base class for a facility that uploads incident reports.
class IncidentReportUploader {
 public:
  // The result of a report upload. Values here are used for UMA so they must
  // not be changed.
  enum Result {
    UPLOAD_SUCCESS = 0,           // A response was received.
    UPLOAD_SUPPRESSED = 1,        // The request was suppressed.
    UPLOAD_INVALID_REQUEST = 2,   // The request was invalid.
    UPLOAD_CANCELLED = 3,         // The upload was cancelled.
    UPLOAD_REQUEST_FAILED = 4,    // Upload failed.
    UPLOAD_INVALID_RESPONSE = 5,  // The response was not recognized.
    UPLOAD_NO_DOWNLOAD = 6,       // No last download was found.
    NUM_UPLOAD_RESULTS
  };

  // A callback run by the uploader upon success or failure. The first argument
  // indicates the result of the upload, while the second contains the response
  // received, if any.
  typedef base::OnceCallback<void(Result,
                                  std::unique_ptr<ClientIncidentResponse>)>
      OnResultCallback;

  virtual ~IncidentReportUploader();

 protected:
  explicit IncidentReportUploader(OnResultCallback callback);

  // The callback by which results are returned.
  OnResultCallback callback_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_INCIDENT_REPORT_UPLOADER_H_
