// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FAKE_DEEP_SCANNING_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FAKE_DEEP_SCANNING_DIALOG_DELEGATE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"
#include "chrome/browser/safe_browsing/download_protection/binary_upload_service.h"
#include "components/safe_browsing/proto/webprotect.pb.h"

namespace content {
class WebContents;
}

namespace safe_browsing {

// A derivative of DeepScanningDialogDelegate that overrides calls to the
// real binary upload service and re-implements them locally.
class FakeDeepScanningDialogDelegate : public DeepScanningDialogDelegate {
 public:
  // Callback that determines the scan status of the file specified.  To
  // simulate a file that passes a scan return a successful response, such
  // as the value returned by SuccessfulResponse().  To simulate a file that
  // does not pass a scan return a failed response, such as the value returned
  // by MalwareResponse() or DlpResponse().
  using StatusCallback = base::RepeatingCallback<DeepScanningClientResponse(
      const base::FilePath&)>;

  FakeDeepScanningDialogDelegate(base::RepeatingClosure delete_closure,
                                 StatusCallback status_callback,
                                 std::string dm_token,
                                 content::WebContents* web_contents,
                                 Data data,
                                 CompletionCallback callback);
  ~FakeDeepScanningDialogDelegate() override;

  // Use with DeepScanningDialogDelegate::SetFactoryForTesting() to create
  // fake instances of this class.  Note that all but the last three arguments
  // will need to be bound at base::Bind() time.
  static std::unique_ptr<DeepScanningDialogDelegate> Create(
      base::RepeatingClosure delete_closure,
      StatusCallback status_callback,
      std::string dm_token,
      content::WebContents* web_contents,
      Data data,
      CompletionCallback callback);

  // Returns a deep scanning response that represents a successful scan.
  static DeepScanningClientResponse SuccessfulResponse();

  // Returns a deep scanning response with a specific malware verdict,
  static DeepScanningClientResponse MalwareResponse(
      MalwareDeepScanningVerdict::Verdict verdict);

  // Returns a deep scanning response with a specific DLP verdict,
  static DeepScanningClientResponse DlpResponse(
      DlpDeepScanningVerdict::Status status,
      const std::string& rule_name,
      DlpDeepScanningVerdict::TriggeredRule::Action action);

 private:
  // Simulates a response from the binary upload service.  the |path| argument
  // is used to call |status_callback_| to determine if the path should succeed
  // or fail.
  void Response(base::FilePath path,
                std::unique_ptr<BinaryUploadService::Request> request);

  // DeepScanningDialogDelegate overrides.
  void UploadTextForDeepScanning(
      std::unique_ptr<BinaryUploadService::Request> request) override;
  void UploadFileForDeepScanning(
      const base::FilePath& path,
      std::unique_ptr<BinaryUploadService::Request> request) override;
  bool CloseTabModalDialog() override;

  base::RepeatingClosure delete_closure_;
  StatusCallback status_callback_;
  std::string dm_token_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FAKE_DEEP_SCANNING_DIALOG_DELEGATE_H_
