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
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/safe_browsing/download_protection/binary_upload_service.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_delegate.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/safe_browsing/proto/webprotect.pb.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "url/gurl.h"

class Profile;

namespace safe_browsing {

extern const base::Feature kDeepScanningOfUploads;
extern const base::Feature kDeepScanningOfUploadsUI;

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
class DeepScanningDialogDelegate : public TabModalConfirmDialogDelegate {
 public:
  // Used as an input to ShowForWebContents() to describe what data needs
  // deeper scanning.  Any members can be empty.
  struct Data {
    Data();
    Data(Data&& other);
    ~Data();

    // Members than indicate what type of scans to perform.  If |do_dlp_scan|
    // is true then the text and files specified by the members below will be
    // scanned for content compliance.  If |do_malware_scan| is true then the
    // files specified by the members below will be scanned for malware.  Text
    // strings are not scanned for malware.
    bool do_dlp_scan = false;
    bool do_malware_scan = false;

    // Text data to scan, such as plain text, URLs, HTML content, etc.
    std::vector<base::string16> text;

    // List of files to scan.
    std::vector<base::FilePath> paths;
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

    // SHA256 hash for the given file.
    std::string sha256;

    // File size in bytes. -1 represents an unknown size.
    uint64_t size;
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
  ~DeepScanningDialogDelegate() override;

  // TabModelConfirmDialogDelegate implementation.
  base::string16 GetTitle() override;
  base::string16 GetDialogMessage() override;
  int GetDialogButtons() const override;
  void OnCanceled() override;

  // Returns true if the deep scanning feature is enabled in the upload
  // direction via enterprise policies.  If the appropriate enterprise policies
  // are not set this feature is not enabled.
  //
  // The |do_dlp_scan| and |do_malware_scan| members of |data| are filled in
  // as needed.  If either is true, the function returns true, otherwise it
  // returns false.
  static bool IsEnabled(Profile* profile, GURL url, Data* data);

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
                                 CompletionCallback callback);

  // In tests, sets a factory function for creating fake
  // DeepScanningDialogDelegates.
  static void SetFactoryForTesting(Factory factory);

  // Overrides the DM token used for testing purposes.
  static void SetDMTokenForTesting(const policy::DMToken& dm_token);

  // Returns true if the given file type is supported for scanning.
  static bool FileTypeSupported(const bool for_malware_scan,
                                const bool for_dlp_scan,
                                const base::FilePath& path);

 protected:
  DeepScanningDialogDelegate(content::WebContents* web_contents,
                             Data data,
                             CompletionCallback callback);

  // Callbacks from uploading data.  Protected so they can be called from
  // testing derived classes.
  void StringRequestCallback(BinaryUploadService::Result result,
                             DeepScanningClientResponse response);
  void FileRequestCallback(base::FilePath path,
                           BinaryUploadService::Result result,
                           DeepScanningClientResponse response);

  base::WeakPtr<DeepScanningDialogDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  class FileSourceRequest;

  // Gets the device level DM token to use with deep scans.
  static policy::DMToken GetDMToken();

  // Uploads data for deep scanning.  Returns true if uploading is occurring in
  // the background and false if there is nothing to do.
  bool UploadData();

  // Adds required fields to |request| before sending it to the binary upload
  // service.
  void PrepareRequest(DlpDeepScanningClientRequest::ContentSource trigger,
                      BinaryUploadService::Request* request);

  // Fills the arrays in |result_| with the given boolean status.
  void FillAllResultsWith(bool status);

  // Upload the request for deep scanning using the binary upload service.
  // These methods exist so they can be overridden in tests as needed.
  virtual void UploadTextForDeepScanning(
      std::unique_ptr<BinaryUploadService::Request> request);
  virtual void UploadFileForDeepScanning(
      const base::FilePath& path,
      std::unique_ptr<BinaryUploadService::Request> request);

  // Closes the tab modal dialog.  Returns false if the UI was not enabled to
  // indicate no action was taken.  Otherwise returns true.
  // Virtual to override in tests.
  virtual bool CloseTabModalDialog();

  // Calls the CompletionCallback |callback_| if all requests associated with
  // scans of |data_| are finished.  This function may delete |this| so no
  // members should be accessed after this call.
  void MaybeCompleteScanRequest();

  // Runs |callback_| with the calculated results if it is non null.
  // |callback_| is cleared after being run.
  void RunCallback();

  // Sets the FileInfo the given file.
  void SetFileInfo(const base::FilePath& path,
                   std::string sha256,
                   int64_t size);

  // The web contents that is attempting to access the data.
  content::WebContents* web_contents_ = nullptr;

  // Description of the data being scanned and the results of the scan.
  // The elements of the vector |file_info_| hold the FileInfo of the file at
  // the same index in |data_.paths|.
  const Data data_;
  Result result_;
  std::vector<FileInfo> file_info_;

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
  TabModalConfirmDialog* dialog_ = nullptr;

  base::WeakPtrFactory<DeepScanningDialogDelegate> weak_ptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_DIALOG_DELEGATE_H_
