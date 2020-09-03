// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FAKE_DEEP_SCANNING_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FAKE_DEEP_SCANNING_DIALOG_DELEGATE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_dialog_delegate.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"

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
  using StatusCallback =
      base::RepeatingCallback<enterprise_connectors::ContentAnalysisResponse(
          const base::FilePath&)>;

  // Callback that determines the encryption of the file specified.  Returns
  // true if the file is considered encrypted for tests.
  using EncryptionStatusCallback =
      base::RepeatingCallback<bool(const base::FilePath&)>;

  FakeDeepScanningDialogDelegate(base::RepeatingClosure delete_closure,
                                 StatusCallback status_callback,
                                 EncryptionStatusCallback encryption_callback,
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
      EncryptionStatusCallback encryption_callback,
      std::string dm_token,
      content::WebContents* web_contents,
      Data data,
      CompletionCallback callback);

  // Sets a delay to have before returning responses. This is used by tests that
  // need to simulate response taking some time.
  static void SetResponseDelay(base::TimeDelta delay);

  // Returns a content analysis response that represents a successful scan and
  // includes the given tags.
  static enterprise_connectors::ContentAnalysisResponse SuccessfulResponse(
      const std::set<std::string>& tags);

  // Returns a content analysis response with a specific malware action.
  static enterprise_connectors::ContentAnalysisResponse MalwareResponse(
      enterprise_connectors::TriggeredRule::Action action);

  // Returns a content analysis response with a specific DLP action.
  static enterprise_connectors::ContentAnalysisResponse DlpResponse(
      enterprise_connectors::ContentAnalysisResponse::Result::Status status,
      const std::string& rule_name,
      enterprise_connectors::TriggeredRule::Action action);

  // Returns a content analysis response with specific malware and DLP actions.
  static enterprise_connectors::ContentAnalysisResponse MalwareAndDlpResponse(
      enterprise_connectors::TriggeredRule::Action malware_action,
      enterprise_connectors::ContentAnalysisResponse::Result::Status dlp_status,
      const std::string& dlp_rule_name,
      enterprise_connectors::TriggeredRule::Action dlp_action);

  // Sets the BinaryUploadService::Result to use in the next response callback.
  static void SetResponseResult(BinaryUploadService::Result result);

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
      BinaryUploadService::Result result,
      const base::FilePath& path,
      std::unique_ptr<BinaryUploadService::Request> request) override;

  static BinaryUploadService::Result result_;
  base::RepeatingClosure delete_closure_;
  StatusCallback status_callback_;
  EncryptionStatusCallback encryption_callback_;
  std::string dm_token_;

  base::WeakPtrFactory<FakeDeepScanningDialogDelegate> weakptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_FAKE_DEEP_SCANNING_DIALOG_DELEGATE_H_
