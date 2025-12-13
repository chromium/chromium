// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/content_analysis_delegate_base.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContent;
struct ClipboardPasteData;
}  // namespace content

namespace safe_browsing {
class BinaryUploadService;
class SafeBrowsingNavigationObserverManager;
}  // namespace safe_browsing

namespace enterprise_connectors {

class ClipboardRequestHandler;
class ContentAnalysisDialogController;
class FilesRequestHandler;
class PagePrintRequestHandler;

// A class that performs deep scans of data (for example malicious or sensitive
// content checks) before allowing a page to access it.
//
// If the UI is enabled, then a dialog is shown and blocks user interactions
// with the page until a scan verdict is obtained.
//
// If the UI is not enabled, then the dialog is not shown and the delegate
// proceeds as if all data (strings and files) have successfully passed the
// deep checks.  However the checks are still performed in the background and
// verdicts reported, implementing an audit-only mode.
//
// Users of this class normally call IsEnabled() first to determine if deep
// scanning is required for the given web page.  If so, the caller fills in
// the appropriate data members of `Data` and then call CreateForWebContents()
// to start the scan.
//
// Example:
//
//   contents::WebContent* contents = ...
//   Profile* profile = ...
//   safe_browsing::ContentAnalysisDelegate::Data data;
//   if (safe_browsing::ContentAnalysisDelegate::IsEnabled(
//           profile, contents->GetLastCommittedURL(), &data)) {
//     data.text.push_back(...);  // As needed.
//     data.paths.push_back(...);  // As needed.
//     safe_browsing::ContentAnalysisDelegate::CreateForWebContents(
//         contents, std::move(data), base::BindOnce(...));
//   }
class ContentAnalysisDelegate : public ContentAnalysisDelegateBase,
                                public ContentAnalysisInfo {
 public:
  // Used as an input to CreateForWebContents() to describe what data needs
  // deeper scanning.  Any members can be empty.
  struct Data {
    Data();
    Data(Data&& other);
    Data& operator=(Data&& other);
    ~Data();

    // Helper function to populate `text` and `image` with the data in a
    // `content::ClipboardPasteData` object.
    void AddClipboardData(
        const content::ClipboardPasteData& clipboard_paste_data);

    // URL of the page that is to receive sensitive data.
    GURL url;

    // UTF-8 encoded text data to scan, such as plain text, URLs, HTML, etc.
    std::vector<std::string> text;

    // Binary image data to scan, such as png, svg, etc (here we assume the data
    // struct holds one image only).
    std::string image;

    // List of files to scan.
    std::vector<base::FilePath> paths;

    // Page to be printed to scan.
    base::ReadOnlySharedMemoryRegion page;

    // Printer name of the page being sent to, empty for non-print actions.
    std::string printer_name;

    // TODO(b/283108167): Delete or send printer type information to local
    // service partner.
    //  Printer type of the page being sent to, the default value is UNKNOWN.
    ContentMetaData::PrintMetadata::PrinterType printer_type =
        ContentMetaData::PrintMetadata::UNKNOWN;

    // The reason the scanning should happen. This should be populated at the
    // same time as fields like `text`, `paths`, `page`, etc. so that caller
    // code can let enterprise code know the user action triggering content
    // analysis.
    ContentAnalysisRequest::Reason reason = ContentAnalysisRequest::UNKNOWN;

    // The clipboard source of data being pasted into the browser. Empty for
    // non-clipboard pastes, and clipboard pastes in special cases (ex. OTR).
    ContentMetaData::CopiedTextSource clipboard_source;

    // The email for the content area user of the source of clipboard data.
    // Only populated for Workspace sites.
    std::string source_content_area_email;

    // The settings to use for the analysis of the data in this struct.
    AnalysisSettings settings;
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

    // Image data result. A value of true means the image complies with all
    // checks and is safe to be used.  A false means the image does not comply
    //  with all checks and the caller should not use it.
    bool image_result;

    // File data result.  Each element in this array is the result for the
    // corresponding Data::paths element.  A value of true means the file
    // complies with all checks and is safe to be used.  A false means the
    // file does not comply with all checks and the caller should not use it.
    std::vector<bool> paths_results;

    // Page data result. A value of true means the page complies with checks and
    // can be printed, and a value of false means it shouldn't be allowed to
    // print.
    bool page_result;
  };

  // Callback used with CreateForWebContents() that informs caller of verdict
  // of deep scans.
  using CompletionCallback =
      base::OnceCallback<void(const Data& data, Result& result)>;

  // Callback used with CreateForFilesInWebContents() that informs caller of
  // verdict of deep scans.  `data` is the object passed to
  // CreateForFilesInWebContents(). The boolean vector holds the same number of
  // elements as `data.paths` and each corresponds to a path in `data.paths`
  // with the same index.
  using ForFilesCompletionCallback =
      base::OnceCallback<void(std::vector<base::FilePath> paths,
                              std::vector<bool>)>;

  // A factory function used in tests to create fake ContentAnalysisDelegate
  // instances.
  using Factory =
      base::RepeatingCallback<std::unique_ptr<ContentAnalysisDelegate>(
          content::WebContents*,
          Data,
          CompletionCallback)>;

  ContentAnalysisDelegate(const ContentAnalysisDelegate&) = delete;
  ContentAnalysisDelegate& operator=(const ContentAnalysisDelegate&) = delete;
  ~ContentAnalysisDelegate() override;

  // ContentAnalysisDelegateBase:

  // Called when the user decides to bypass the verdict they obtained from DLP.
  // This will allow the upload of files marked as DLP warnings.
  void BypassWarnings(
      std::optional<std::u16string> user_justification) override;

  // Called when the user decides to cancel the file upload. This will stop the
  // upload to Chrome since the scan wasn't allowed to complete. If `warning` is
  // true, it means the user clicked Cancel after getting a warning, meaning the
  // "CancelledByUser" metrics should not be recorded.
  void Cancel(bool warning) override;

  // Returns both rule-based and policy-based custom message without the prefix.
  std::optional<std::u16string> GetCustomMessage() const override;

  std::optional<GURL> GetCustomLearnMoreUrl() const override;

  std::optional<std::vector<std::pair<gfx::Range, GURL>>>
  GetCustomRuleMessageRanges() const override;

  bool BypassRequiresJustification() const override;

  std::u16string GetBypassJustificationLabel() const override;

  std::optional<std::u16string> OverrideCancelButtonText() const override;

  // Returns true if the deep scanning feature is enabled in the upload
  // direction via enterprise policies.  If the appropriate enterprise policies
  // are not set this feature is not enabled.
  //
  // The scan tags in `data.settings.tags` are filled in as needed. If that set
  // is not empty and there is at least 1 tag to scan for, the function returns
  // true, otherwise it returns false.
  static bool IsEnabled(Profile* profile,
                        GURL url,
                        Data* data,
                        AnalysisConnector connector);

  // Entry point for starting a deep scan, with the callback being called once
  // all results are available.  When the UI is enabled, a tab-modal dialog
  // is shown while the scans proceed in the background.  When the UI is
  // disabled, the callback will immedaitely inform the callers that all data
  // has successfully passed the checks, even though the checks will proceed
  // in the background.
  //
  // Whether the UI is enabled or not, verdicts of the scan will be reported.
  static void CreateForWebContents(content::WebContents* web_contents,
                                   Data data,
                                   CompletionCallback callback,
                                   DeepScanAccessPoint access_point);

  // Helper function for calling CreateForWebContents() when the data to
  // process is a collection of files on disk.  This requires first expanding
  // any directories in the given paths in order analyze all the files.
  // If the calling code has already done the directory expansion then it can
  // call `CreateForWebContents()` directly.
  //
  // `data.paths` is expected to contain the files and/or directories to
  // analyze.  `text` and `page` are expected to be null/empty.
  static void CreateForFilesInWebContents(content::WebContents* web_contents,
                                          Data data,
                                          ForFilesCompletionCallback callback,
                                          DeepScanAccessPoint access_point);

  // In tests, sets a factory function for creating fake
  // ContentAnalysisDelegates.
  static void SetFactoryForTesting(Factory factory);
  static void ResetFactoryForTesting();

  // Showing the UI is not possible in unit tests, call this to disable it.
  static void DisableUIForTesting();

  // Undoes the effects of DisableUIForTesting() after testing is finished.
  static void EnableUIAfterTesting();

  // Add a callback to allow tests to validate `AckAllRequests` will send the
  // appropriate actions.
  using OnAckAllRequestsCallback = base::OnceCallback<void(
      const std::map<std::string,
                     ContentAnalysisAcknowledgement::FinalAction>&)>;
  static void SetOnAckAllRequestsCallbackForTesting(
      OnAckAllRequestsCallback callback);

  void SetPageWarningForTesting();

  // ContentAnalysisInfo:
  const AnalysisSettings& settings() const override;
  signin::IdentityManager* identity_manager() const override;
  int user_action_requests_count() const override;
  std::string tab_title() const override;
  std::string user_action_id() const override;
  std::string email() const override;
  const GURL& url() const override;
  const GURL& tab_url() const override;
  ContentAnalysisRequest::Reason reason() const override;
  google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>
  referrer_chain() const override;
  google::protobuf::RepeatedPtrField<std::string> frame_url_chain()
      const override;
  content::WebContents* web_contents() const override;

 protected:
  ContentAnalysisDelegate(content::WebContents* web_contents,
                          Data data,
                          CompletionCallback callback,
                          DeepScanAccessPoint access_point);

  // Callbacks from uploading data. Protected so they can be called from
  // testing derived classes.
  void TextRequestCallback(RequestHandlerResult result);
  void ImageRequestCallback(RequestHandlerResult result);
  void PageRequestCallback(RequestHandlerResult result);

  // Callback called after all files are scanned by the FilesRequestHandler.
  void FilesRequestCallback(std::vector<RequestHandlerResult> results);

  base::WeakPtr<ContentAnalysisDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  FilesRequestHandler* GetFilesRequestHandlerForTesting();

  const Data& GetDataForTesting() { return data_; }

  const std::map<std::string, ContentAnalysisAcknowledgement::FinalAction>&
  GetFinalActionsForTesting() {
    return final_actions_;
  }

  // Methods to either show the final result in the analysis dialog and to
  // cancel the dialog.  These methods are protected and virtual for testing.
  // Returns false if the UI was not enabled to indicate no action was taken.
  virtual bool ShowFinalResultInDialog();
  virtual bool CancelDialog();

 private:
  // Enum representing the data uploading status.
  enum class UploadDataStatus {
    kNoLocalClientFound = 0,
    kInProgress = 1,
    kComplete = 2,
  };

  // Uploads data for deep scanning.  Returns "kNoClientFound" if there's no
  // client to receive data, "kInProgress" if uploading is occurring in the
  // background, and "kComplete" if data uploading is finished.
  UploadDataStatus UploadData();

  // Helper function to evaluate if fail-closed conditions are met.
  bool IsFailClosed(UploadDataStatus upload_data_status,
                    bool should_allow_by_default);

  // Helper function to decide if fail-closed settings should be applied when
  // LCAC cannot establish connection with local client.
  bool ShouldFailOpenWithoutLocalClient(bool should_allow_by_default);

  // Helper function to decide if the page request should be terminated early.
  bool ShouldNotUploadLargePage(size_t page_size);

  // Prepares an upload request for the text in `data_`. If `data_.text` is
  // empty, this method does nothing.
  void PrepareTextRequest();

  // Prepares an upload request for the image in `data_`. If `data_.image` is
  // empty, this method does nothing.
  void PrepareImageRequest();

  // Prepares an upload request for the printed page bytes in `data_`. If there
  // aren't any, this method does nothing.
  void PreparePageRequest();

  // Fills the arrays in `result_` with the given boolean status.
  void FillAllResultsWith(bool status);

  // Updates the tab modal dialog to show the scanning results. Returns false if
  // the UI was not enabled to indicate no action was taken. Virtual to override
  // in tests.
  virtual bool UpdateDialog();

  // Calls the CompletionCallback `callback_` if all requests associated with
  // scans of `data_` are finished.  This function may delete `this` so no
  // members should be accessed after this call.
  void MaybeCompleteScanRequest();

  // Runs `callback_` with the calculated results if it is non null.
  // `callback_` is cleared after being run.
  void RunCallback();

  // Updates `final_result_` following the precedence established by the
  // FinalResult enum.
  void UpdateFinalResult(
      FinalContentAnalysisResult message,
      const std::string& tag,
      const ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage&
          custom_rule_message);

  // Send an acknowledgement to the service provider of the final result
  // for the requests of this ContentAnalysisDelegate instance.
  void AckAllRequests();

  // Returns the BinaryUploadService used to upload content for deep scanning.
  // Virtual to override in tests.
  virtual safe_browsing::BinaryUploadService* GetBinaryUploadService();

  safe_browsing::SafeBrowsingNavigationObserverManager*
  GetNavigationObserverManager() const;

  // Returns the content transfer method for the action. This is only used for
  // reporting and can be empty if the exact transfer method isn't supported in
  // reporting.
  std::string GetContentTransferMethod() const;

  // Returns `true` if `data_` contains corresponding data to be scanned, and
  // when that data isn't too large to be exempt from scanning.
  bool text_request_required() const;
  bool image_request_required() const;

  // The Profile corresponding to the pending scan request(s).
  raw_ptr<Profile> profile_ = nullptr;

  // The GURL corresponding to the page where the scan triggered.
  GURL url_;

  // Parent URL chain of the frame from which the action was triggered.
  google::protobuf::RepeatedPtrField<std::string> frame_url_chain_;

  // The title corresponding to the WebContents triggering the scan.
  std::string title_;

  // The unique ID for keeping track of each user action.
  std::string user_action_id_;

  // Description of the data being scanned and the results of the scan.
  Data data_;
  Result result_;

  // Indices of warned files.
  std::vector<size_t> warned_file_indices_;

  // Set to true if the printed page got a DLP warning verdict.
  bool page_warning_ = false;

  // Set to true once the scan of text has completed.  If the scan request has
  // no text requiring deep scanning, this is set to true immediately.
  bool text_request_complete_ = false;

  // Set to true once the scan of image has completed.  If the scan request has
  // no image requiring deep scanning, this is set to true immediately.
  bool image_request_complete_ = false;

  // Set to true once all file scans have completed.  If the scan requests have
  // no files requiring deep scanning, this is set to true immediately.
  bool files_request_complete_ = false;

  // Set to true once the scan of a printed page has completed. If the scan
  // request has no page requiring deep scanning, this is set to true
  // immediately.
  bool page_request_complete_ = false;

  // Called once all text and files have completed deep scanning.
  CompletionCallback callback_;

  // Pointer to UI when enabled.
  raw_ptr<ContentAnalysisDialogController> dialog_ = nullptr;

  // Access point to use to record UMA metrics.
  DeepScanAccessPoint access_point_;

  // Scanning result to be shown to the user once every request is done.
  FinalContentAnalysisResult final_result_ =
      FinalContentAnalysisResult::SUCCESS;
  // The tag (dlp, malware, etc) of the result that triggered the verdict
  // represented by `final_result_`.
  std::string final_result_tag_;

  // Set to true at the end of UploadData to indicate requests have been made
  // for every file/text. This is read to ensure `this` isn't deleted too early.
  bool data_uploaded_ = false;

  // Responsible for opening and scanning multiple files on parallel threads.
  // Always nullptr for non-file content scanning.
  std::unique_ptr<FilesRequestHandler> files_request_handler_;

  // Responsible for managing the scan of printed pages.
  // Always nullptr for non-print content scanning.
  std::unique_ptr<PagePrintRequestHandler> page_print_request_handler_;

  // Responsible for managing the scan of pasted text.
  // Always nullptr for non-text paste content scanning.
  std::unique_ptr<ClipboardRequestHandler> text_request_handler_;

  // Responsible for managing the scan of a pasted image.
  // Always nullptr for non-image paste content scanning.
  std::unique_ptr<ClipboardRequestHandler> image_request_handler_;

  // A mapping of request tokens to ack final actions for all requests that make
  // up the user action represented by this ContentAnalysisDelegate.
  std::map<std::string, ContentAnalysisAcknowledgement::FinalAction>
      final_actions_;

  // Results returned from `files_request_handler_`.
  std::vector<RequestHandlerResult> files_request_results_;

  // Result updated in `StringRequestCallback()`.
  RequestHandlerResult text_request_result_;

  // Result updated in `ImageRequestCallback()`.
  RequestHandlerResult image_request_result_;

  // Result updated in `PageRequestCallback()`.
  RequestHandlerResult page_print_request_result_;

  // Indicate that `callback_` is currently being called. This is almost always
  // false, but in some cases UI thread tasks can run while `callback_` is not
  // over due showing UI, such as during native print dialogs.
  bool callback_running_ = false;

  // Indicates that `this` can be deleted right away. This is used with
  // `callback_running_` to handle race conditions where non-blocking scans
  // should wait before deleting `this`.
  bool all_work_done_ = false;

  // Content type of the page that triggered the action.
  std::string page_content_type_;

  base::TimeTicks upload_start_time_;

  // Custom message for rule.
  ContentAnalysisResponse::Result::TriggeredRule::CustomRuleMessage
      custom_rule_message_;

  base::WeakPtr<content::WebContents> web_contents_;

  base::WeakPtrFactory<ContentAnalysisDelegate> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DELEGATE_H_
