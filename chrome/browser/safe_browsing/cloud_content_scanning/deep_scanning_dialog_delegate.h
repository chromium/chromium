// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_DIALOG_DELEGATE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/safe_browsing/core/proto/webprotect.pb.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "url/gurl.h"

class Profile;

namespace safe_browsing {

class BinaryUploadService;
class DeepScanningDialogViews;

// A tab modal dialog delegate that informs the user of a background deep
// scan happening in the given tab with an option to cancel the operation.
//
// If the UI is not enabled, then the dialog is not shown and the delegate
// proceeds as if all data (strings and files) have successfully passed the
// deep checks.  However the checks are still performed in the background and
// verdicts reported, implementing an audit-only mode.  The UI will be turned
// on once finalized.
//
// Users of this class normally call IsEnabled() first to determine if deep
// scanning is required for the given web page.  If so, the caller fills in
// the appropriate data members of |Data| and then call ShowForWebContents()
// to start the scan.
//
// Example:
//
//   contents::WebContent* contents = ...
//   Profile* profile = ...
//   safe_browsing::DeepScanningDialogDelegate::Data data;
//   if (safe_browsing::DeepScanningDialogDelegate::IsEnabled(
//           profile, contents->GetLastCommittedURL(), &data)) {
//     data.text.push_back(...);  // As needed.
//     data.paths.push_back(...);  // As needed.
//     safe_browsing::DeepScanningDialogDelegate::ShowForWebContents(
//         contents, std::move(data), base::BindOnce(...));
//   }
class DeepScanningDialogDelegate {
 public:
  // Used as an input to ShowForWebContents() to describe what data needs
  // deeper scanning.  Any members can be empty.
  struct Data {
    Data();
    Data(Data&& other);
    ~Data();

    // URL of the page that is to receive sensitive data.
    GURL url;

    // Text data to scan, such as plain text, URLs, HTML content, etc.
    std::vector<base::string16> text;

    // List of files to scan.
    std::vector<base::FilePath> paths;

    // The settings to use for the analysis of the data in this struct.
    enterprise_connectors::AnalysisSettings settings;
  };

  // Result of deep scanning.  Each Result contains the verdicts of deep scans
  // specified by one Data.
  struct Result {
    Result();
    Result(Result&& other);
    ~Result();

    // String data result.  Each element in this array is the result for the
    // corresponding Data::text element.  A value of true means the text
    // complies with all checks and is safe to be used.  A false means the
    // text does not comply with all checks and the caller should not use it.
    std::vector<bool> text_results;

    // File data result.  Each element in this array is the result for the
    // corresponding Data::paths element.  A value of true means the file
    // complies with all checks and is safe to be used.  A false means the
    // file does not comply with all checks and the caller should not use it.
    std::vector<bool> paths_results;
  };

  // File information used as an input to event report functions.
  struct FileInfo {
    FileInfo();
    FileInfo(FileInfo&& other);
    ~FileInfo();

    // Hex-encoded SHA256 hash for the given file.
    std::string sha256;

    // File size in bytes. -1 represents an unknown size.
    uint64_t size = 0;

    // File mime type.
    std::string mime_type;
  };

  // File contents used as input for |file_info_| and the BinaryUploadService.
  struct FileContents {
    FileContents();
    explicit FileContents(BinaryUploadService::Result result);
    FileContents(FileContents&&);
    FileContents& operator=(FileContents&&);

    BinaryUploadService::Result result = BinaryUploadService::Result::UNKNOWN;
    BinaryUploadService::Request::Data data;

    // Store the file size separately instead of using data.contents.size() to
    // keep track of size for large files.
    int64_t size = 0;
    std::string sha256;
  };

  // Enum to identify which message to show once scanning is complete. Ordered
  // by precedence for when multiple files have conflicting results.
  // TODO(crbug.com/1055785): Refactor this to whatever solution is chosen.
  enum class DeepScanningFinalResult {
    // Show that an issue was found and that the upload is blocked.
    FAILURE = 0,

    // Show that files were not uploaded since they were too large.
    LARGE_FILES = 1,

    // Show that files were not uploaded since they were encrypted.
    ENCRYPTED_FILES = 2,

    // Show that DLP checks failed, but that the user can proceed if they want.
    WARNING = 3,

    // Show that no issue was found and that the user may proceed.
    SUCCESS = 4,
  };

  // Callback used with ShowForWebContents() that informs caller of verdict
  // of deep scans.
  using CompletionCallback =
      base::OnceCallback<void(const Data& data, const Result& result)>;

  // A factory function used in tests to create fake DeepScanningDialogDelegate
  // instances.
  using Factory =
      base::RepeatingCallback<std::unique_ptr<DeepScanningDialogDelegate>(
          content::WebContents*,
          Data,
          CompletionCallback)>;

  DeepScanningDialogDelegate(const DeepScanningDialogDelegate&) = delete;
  DeepScanningDialogDelegate& operator=(const DeepScanningDialogDelegate&) =
      delete;
  virtual ~DeepScanningDialogDelegate();

  // Called when the user decides to bypass the verdict they obtained from DLP.
  // This will allow the upload of files marked as DLP warnings.
  void BypassWarnings();

  // Called when the user decides to cancel the file upload. This will stop the
  // upload to Chrome since the scan wasn't allowed to complete. If |warning| is
  // true, it means the user clicked Cancel after getting a warning, meaning the
  // "CancelledByUser" metrics should not be recorded.
  void Cancel(bool warning);

  // Returns true if the deep scanning feature is enabled in the upload
  // direction via enterprise policies.  If the appropriate enterprise policies
  // are not set this feature is not enabled.
  //
  // The |do_dlp_scan| and |do_malware_scan| members of |data| are filled in
  // as needed.  If either is true, the function returns true, otherwise it
  // returns false.
  static bool IsEnabled(Profile* profile,
                        GURL url,
                        Data* data,
                        enterprise_connectors::AnalysisConnector connector);

  // Entry point for starting a deep scan, with the callback being called once
  // all results are available.  When the UI is enabled, a tab-modal dialog
  // is shown while the scans proceed in the background.  When the UI is
  // disabled, the callback will immedaitely inform the callers that all data
  // has successfully passed the checks, even though the checks will proceed
  // in the background.
  //
  // Whether the UI is enabled or not, verdicts of the scan will be reported.
  static void ShowForWebContents(content::WebContents* web_contents,
                                 Data data,
                                 CompletionCallback callback,
                                 DeepScanAccessPoint access_point);

  // In tests, sets a factory function for creating fake
  // DeepScanningDialogDelegates.
  static void SetFactoryForTesting(Factory factory);
  static void ResetFactoryForTesting();

  // Showing the UI is not possible in unit tests, call this to disable it.
  static void DisableUIForTesting();

  // Determines if a request result should be used to allow a data use or to
  // block it.
  static bool ResultShouldAllowDataUse(
      BinaryUploadService::Result result,
      const enterprise_connectors::AnalysisSettings& settings);

 protected:
  DeepScanningDialogDelegate(content::WebContents* web_contents,
                             Data data,
                             CompletionCallback callback,
                             DeepScanAccessPoint access_point);

  // Callbacks from uploading data.  Protected so they can be called from
  // testing derived classes.
  void StringRequestCallback(
      BinaryUploadService::Result result,
      enterprise_connectors::ContentAnalysisResponse response);
  void FileRequestCallback(
      base::FilePath path,
      BinaryUploadService::Result result,
      enterprise_connectors::ContentAnalysisResponse response);

  base::WeakPtr<DeepScanningDialogDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Uploads data for deep scanning.  Returns true if uploading is occurring in
  // the background and false if there is nothing to do.
  bool UploadData();

  // Prepares an upload request for the text in |data_|. If |data_.text| is
  // empty, this method does nothing.
  void PrepareTextRequest();

  // Prepares an upload request for the file at |path|.  If the file
  // cannot be uploaded it will have a failure verdict added to |result_|.
  // Virtual so that it can be overridden in tests.
  void PrepareFileRequest(const base::FilePath& path);

  // Adds required fields to |request| before sending it to the binary upload
  // service.
  void PrepareRequest(enterprise_connectors::AnalysisConnector connector,
                      BinaryUploadService::Request* request);

  // Fills the arrays in |result_| with the given boolean status.
  void FillAllResultsWith(bool status);

  // Upload the request for deep scanning using the binary upload service.
  // These methods exist so they can be overridden in tests as needed.
  // The |result| argument exists as an optimization to finish the request early
  // when the result is known in advance to avoid using the upload service.
  virtual void UploadTextForDeepScanning(
      std::unique_ptr<BinaryUploadService::Request> request);
  virtual void UploadFileForDeepScanning(
      BinaryUploadService::Result result,
      const base::FilePath& path,
      std::unique_ptr<BinaryUploadService::Request> request);

  // Updates the tab modal dialog to show the scanning results. Returns false if
  // the UI was not enabled to indicate no action was taken. Virtual to override
  // in tests.
  virtual bool UpdateDialog();

  // Calls the CompletionCallback |callback_| if all requests associated with
  // scans of |data_| are finished.  This function may delete |this| so no
  // members should be accessed after this call.
  void MaybeCompleteScanRequest();

  // Runs |callback_| with the calculated results if it is non null.
  // |callback_| is cleared after being run.
  void RunCallback();

  // Called when the file info for |path| has been fetched. Also begins the
  // upload process.
  void OnGotFileInfo(std::unique_ptr<BinaryUploadService::Request> request,
                     const base::FilePath& path,
                     BinaryUploadService::Result result,
                     const BinaryUploadService::Request::Data& data);

  // Completion of |FileRequestCallback| once the mime type is obtained
  // asynchronously.
  void CompleteFileRequestCallback(
      size_t index,
      base::FilePath path,
      BinaryUploadService::Result result,
      enterprise_connectors::ContentAnalysisResponse response,
      std::string mime_type);

  // Updates |final_result_| following the precedence established by the
  // DeepScanningFinalResult enum.
  void UpdateFinalResult(DeepScanningFinalResult message);

  // Returns the BinaryUploadService used to upload content for deep scanning.
  // Virtual to override in tests.
  virtual BinaryUploadService* GetBinaryUploadService();

  // The web contents that is attempting to access the data.
  content::WebContents* web_contents_ = nullptr;

  // Description of the data being scanned and the results of the scan.
  // The elements of the vector |file_info_| hold the FileInfo of the file at
  // the same index in |data_.paths|.
  const Data data_;
  Result result_;
  std::vector<FileInfo> file_info_;

  // Set to true if the full text got a DLP warning verdict.
  bool text_warning_ = false;
  enterprise_connectors::ContentAnalysisResponse text_response_;

  // Scanning responses of files that got DLP warning verdicts.
  std::map<size_t, enterprise_connectors::ContentAnalysisResponse>
      file_warnings_;

  // Set to true once the scan of text has completed.  If the scan request has
  // no text requiring deep scanning, this is set to true immediately.
  bool text_request_complete_ = false;

  // The number of files scans that have completed.  If more than one file is
  // requested for scanning in |data_|, each is scanned in parallel with
  // separate requests.
  size_t file_result_count_ = 0;

  // Called once all text and files have completed deep scanning.
  CompletionCallback callback_;

  // Pointer to UI when enabled.
  DeepScanningDialogViews* dialog_ = nullptr;

  // Access point to use to record UMA metrics.
  DeepScanAccessPoint access_point_;

  // Scanning result to be shown to the user once every request is done.
  DeepScanningFinalResult final_result_ = DeepScanningFinalResult::SUCCESS;

  base::TimeTicks upload_start_time_;

  base::WeakPtrFactory<DeepScanningDialogDelegate> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_DIALOG_DELEGATE_H_
