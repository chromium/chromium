// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_UPLOADING_UPLOAD_JOB_H_
#define CHROME_BROWSER_ASH_POLICY_UPLOADING_UPLOAD_JOB_H_

#include <map>
#include <memory>
#include <string>

#include "url/gurl.h"

namespace policy {

class DataSegment;

// UploadJob can be used to upload screenshots and logfiles to the cloud.
// Data is uploaded via a POST request of type "multipart/form-data". The class
// relies on OAuth2AccessTokenManager to acquire an access token with a
// sufficient scope. Data segments can be added to the upload queue using
// AddDataSegment() and the upload is started by calling Start(). Calls to
// AddDataSegment() are only allowed prior to the first call to Start(). An
// Upload instance may be destroyed at any point in time, the pending operations
// are guaranteed to be canceled and the Delegate::OnSuccess() and
// Delegate::OnFailure() methods will not be invoked.
class UploadJob {
 public:
  // If the upload fails, the Delegate's OnFailure() method is invoked with
  // one of these error codes.
  enum ErrorCode {
    NETWORK_ERROR = 1,         // Network failure.
    AUTHENTICATION_ERROR = 2,  // Authentication failure.
    SERVER_ERROR = 3           // Server returned error or malformed reply.
  };

  class Delegate {
   public:
    Delegate& operator=(const Delegate&) = delete;

    // When the upload finishes successfully, the OnSuccess() method is invoked.
    virtual void OnSuccess() = 0;

    // On upload failure, the OnFailure() method is invoked with an ErrorCode
    // indicating the reason for failure.
    virtual void OnFailure(ErrorCode error_code) = 0;

   protected:
    virtual ~Delegate();
  };

  UploadJob& operator=(const UploadJob&) = delete;

  virtual ~UploadJob() {}

  // Adds one data segment to the UploadJob. A DataSegment corresponds
  // to one "Content-Disposition" in the "multipart" request. As per RFC 2388,
  // each content-disposition has a |name| field, which must be unique within a
  // given request. For file uploads the original local file name may be
  // supplied as well as in the |filename| field. If |filename| references an
  // empty string, no |filename| header will be added for this data segment.
  // This method must not be called on an UploadJob instance which is already
  // uploading.
  virtual void AddDataSegment(
      const std::string& name,
      const std::string& filename,
      const std::map<std::string, std::string>& header_entries,
      std::unique_ptr<std::string> data) = 0;

  // Initiates the data upload . This method must only be called once.
  virtual void Start() = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_UPLOADING_UPLOAD_JOB_H_
