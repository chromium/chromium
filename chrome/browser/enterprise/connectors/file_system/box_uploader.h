// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_UPLOADER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_UPLOADER_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_response.h"
#include "components/download/public/common/download_item_impl.h"
#include "components/download/public/common/download_item_rename_progress_update.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"

namespace enterprise_connectors {
class FileSystemRenameHandler;

// The UMA label used to log the number of renames to avoid a collision when
// uploading to Box
extern const char kUniquifierUmaLabel[];

// Task Manager for downloaded items used by FileSystemRenamdHandler that
// connects between the Chrome client and Box. Once given a download item and
// authentication, internally it manages the entire API call flow required to
// find upstream destination, upload the file, and delete the local temporary
// file.
class BoxUploader {
 public:
  static const FileSystemServiceProvider kServiceProvider;

  static std::unique_ptr<BoxUploader> Create(
      download::DownloadItem* download_item);

  // Test observer class that monitors BoxUploader behaviors.
  class TestObserver : public base::CheckedObserver {
   public:
    explicit TestObserver(FileSystemRenameHandler* rename_handler);
    ~TestObserver() override;

    enum Status { kNotStarted, kInProgress, kSucceeded, kFailed };

    void OnUploadStart();
    void OnUploadDone(bool succeeded);
    void OnFileDeletionStart();
    void OnFileDeletionDone(bool succeeded);
    void OnDestruction();
    void WaitForUploadStart();
    bool WaitForUploadCompletion();
    bool WaitForTmpFileDeletion();
    GURL GetFileUrl();

   private:
    base::WeakPtr<BoxUploader> uploader_;
    GURL file_url_;
    Status upload_status_ = Status::kNotStarted;
    Status tmp_file_deletion_status_ = Status::kNotStarted;
    base::OnceClosure stop_waiting_for_upload_to_start_;
    base::OnceClosure stop_waiting_for_upload_to_complete_;
    base::OnceClosure stop_waiting_for_deletion_to_complete_;
  };

  virtual ~BoxUploader();

  using InterruptReason = download::DownloadInterruptReason;
  using ProgressUpdate = download::DownloadItemRenameProgressUpdate;
  // Callback to update the DownloadItem and send BoxInfo into databases.
  using ProgressUpdateCallback = base::RepeatingCallback<void(
      const download::DownloadItemRenameProgressUpdate&)>;
  // Callback when upload completes. Args indicate result to be updated to UX,
  // and the final file name validated on Box.
  using UploadCompleteCallback =
      base::OnceCallback<void(InterruptReason, const base::FilePath&)>;

  // Initialize with callbacks from FileSystemRenameHandler, set
  // current_api_call_ to be the first step of the whole API call workflow. Must
  // be called before calling TryTask() for the first time.
  void Init(base::RepeatingCallback<void(void)> authen_retry_callback,
            ProgressUpdateCallback progress_update_cb,
            UploadCompleteCallback upload_complete_cb,
            PrefService* prefs);

  // Kick off the workflow from the step stored in current_api_call_. Will
  // re-attempt the last step from where it left off if it called callback with
  // an API call failure earlier.
  virtual void TryTask(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token);

  // Cancel the upload and delete the local temporary file.
  void TerminateTask(InterruptReason reason);

  virtual GURL GetUploadedFileUrl() const;
  virtual GURL GetDestinationFolderUrl() const;

  // Helper methods for unit tests.
  std::string GetFolderIdForTesting() const;
  void NotifyOAuth2ErrorForTesting();
  void SetUploadApiCallFlowDoneForTesting(InterruptReason reason,
                                          std::string file_id);

  // The largest number of retries attempted in OnPreflightCheckResponse.
  enum UploadAttemptCount {
    kNotRenamed = 0,
    kMaxRenamedWithSuffix = 9,
    kTimestampBasedName = 1000,
    kAbandonedUpload = 2000,
  };

 protected:
  // Constructor with download::DownloadItem* to access download_item fields but
  // does not store the pointer internally and the ownership of download_item
  // remains with the caller.
  explicit BoxUploader(download::DownloadItem* download_item);

  void TryCurrentApiCall();
  bool EnsureSuccess(BoxApiCallResponse response);
  void OnFileError(base::File::Error error);
  void OnApiCallFlowDone(InterruptReason upload_interrupt_reason,
                         std::string uploaded_file_id);
  void SendProgressUpdate() const;
  // Notify upload success or failure + reason back to the download thread.
  void NotifyResult(InterruptReason reason);

  // To be overridden to test API calls flow and file delete separately.
  virtual void StartCurrentApiCall();
  // Must be implemented in child classes.
  virtual std::unique_ptr<OAuth2ApiCallFlow> MakeFileUploadApiCall() = 0;
  // After preflight check succeeds, go into either BoxDirectUploader or
  // BoxChunkedUploader.
  virtual void StartUpload();
  // Can be overridden to handle failure differently from simply calling
  // OnApiCallFlowDone(<failure reasons>).
  virtual void OnApiCallFlowFailure(BoxApiCallResponse response);
  virtual void OnApiCallFlowFailure(InterruptReason reason);

  const base::FilePath GetLocalFilePath() const;
  // Return the file name used for the upload, which, if there was naming
  // conflict, can be formatted with suffix or timestamp and thus different from
  // |target_file_name_|.
  const base::FilePath GetUploadFileName() const;
  const std::string GetFolderId();
  const std::string GetFolderId() const;
  void SetFolderId(std::string folder_id);
  void SetCurrentApiCall(std::unique_ptr<OAuth2ApiCallFlow> api_call);
  BoxInfo& reroute_info() { return *(reroute_info_.mutable_box()); }
  const BoxInfo& reroute_info() const { return reroute_info_.box(); }

  // Iff InterruptReason returned is
  // download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED, the error messages in
  // the proto should be used in place of localized download interrupt reason
  // strings.
  static InterruptReason ConvertToInterruptReasonOrErrorMessage(
      BoxApiCallResponse response,
      BoxInfo& reroute_info);

 private:
  // Box API call pre-upload steps:
  std::unique_ptr<OAuth2ApiCallFlow> MakeFindUpstreamFolderApiCall();
  std::unique_ptr<OAuth2ApiCallFlow> MakeCreateUpstreamFolderApiCall();
  std::unique_ptr<OAuth2ApiCallFlow> MakePreflightCheckApiCall();

  // Callbacks from Box*ApiCallFlows:
  void OnFindUpstreamFolderResponse(BoxApiCallResponse response,
                                    const std::string& folder_id);
  void OnCreateUpstreamFolderResponse(BoxApiCallResponse response,
                                      const std::string& folder_id);
  void OnPreflightCheckResponse(BoxApiCallResponse response);
  void LogUniquifierCountToUma();

  // The followings are not necessarily specific to Box:
  // Post a task to ThreadPool to delete the local file, after the entire file
  // upload was done, with callback OnFileDeleted().
  void PostDeleteFileTask(InterruptReason upload_reason);
  // Callback attached in PostDeleteFileTask(). Report success back to original
  // thread via upload_complete_cb_.
  void OnFileDeleted(InterruptReason upload_reason,
                     base::File::Error delete_status);

  // File details.
  const base::FilePath local_file_path_;   // Path of the local temporary file.
  const base::FilePath target_file_name_;  // File name to be used for upload.
  const base::Time download_start_time_;   // Start time of the download.
  uint32_t uniquifier_;  // Number suffix for the filename to uniquify.

  // Reroute info loaded from / to be stored into download databases.
  DownloadItemRerouteInfo reroute_info_;
  // Callback when there's an update for DownloadItem's observers.
  ProgressUpdateCallback progress_update_cb_;
  // Callback when the entire flow is completed to notify the download thread.
  UploadCompleteCallback upload_complete_cb_;
  // Callback when API call gives Authenetication Error.
  base::RepeatingCallback<void(void)> authentication_retry_callback_;

  // Used for OAuth2ApiCallFlow::Start():
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::string access_token_;
  // Ptr that stores the current OAuth2ApiCallFlow step. It is updated to point
  // to another mini OAuth2ApiCallFlow class whenever the workflow needs to
  // advance to the next step; may also gets re-instantiated when there's an API
  // call failure such that, when the external caller calls TryTask() again, the
  // current step is re-attempted.
  std::unique_ptr<OAuth2ApiCallFlow> current_api_call_;
  // PrefService used to store folder_id.
  raw_ptr<PrefService> prefs_ =
      nullptr;  // Must be initialized to nullptr for DCHECKs.

  // Test observers
  base::ObserverList<TestObserver> observers_;

  base::WeakPtrFactory<BoxUploader> weak_factory_{this};
};

// Task Manager extended from BoxUploader specifically to upload the whole file
// directly using API specified at
// https://developer.box.com/guides/uploads/direct/.
class BoxDirectUploader : public BoxUploader {
 public:
  explicit BoxDirectUploader(download::DownloadItem* download_item);
  ~BoxDirectUploader() override;

 private:
  // BoxUploader interface.
  std::unique_ptr<OAuth2ApiCallFlow> MakeFileUploadApiCall() override;

  // Box API call step.
  void OnWholeFileUploadResponse(BoxApiCallResponse response,
                                 const std::string& file_id);

  const std::string mime_type_;
  base::WeakPtrFactory<BoxDirectUploader> weak_factory_{this};
};

// Task Manager extended from BoxUploader specifically to upload the file
// in chunks using API specified at
// https://developer.box.com/guides/uploads/chunked/.
class BoxChunkedUploader : public BoxUploader {
 public:
  explicit BoxChunkedUploader(download::DownloadItem* download_item);
  ~BoxChunkedUploader() override;

  class FileChunksHandler;

  struct PartInfo {
    base::File::Error error;
    std::string content;
    size_t byte_from;  // Inclusive of 1st byte of the file part.
    size_t byte_to;    // Inclusive of last byte in the file part.
    // Therefore byte_to == byte_from + content.size() - 1.
  };

 private:
  // BoxUploader interface.
  void OnApiCallFlowFailure(InterruptReason reason) override;
  std::unique_ptr<OAuth2ApiCallFlow> MakeFileUploadApiCall() override;

  // Helper methods to transition between chunked upload steps.
  std::unique_ptr<OAuth2ApiCallFlow> MakeCreateUploadSessionApiCall();
  std::unique_ptr<OAuth2ApiCallFlow> MakePartFileUploadApiCall();
  std::unique_ptr<OAuth2ApiCallFlow> MakeCommitUploadSessionApiCall();
  std::unique_ptr<OAuth2ApiCallFlow> MakeAbortUploadSessionApiCall(
      InterruptReason reason);

  // Callbacks for chunked file upload.
  void OnCreateUploadSessionResponse(BoxApiCallResponse response,
                                     base::Value session_endpoints,
                                     size_t part_size);
  void OnPartFileUploadResponse(BoxApiCallResponse response,
                                base::Value part_info);
  void OnCommitUploadSessionResponse(BoxApiCallResponse response,
                                     base::TimeDelta retry_after,
                                     const std::string& file_id);
  void OnAbortUploadSessionResponse(InterruptReason reason,
                                    BoxApiCallResponse response);

  // Callbacks for chunks_handler_.
  void OnFileChunkRead(PartInfo part_info);
  void OnFileCompletelyUploaded(const std::string& sha1_digest);

  std::unique_ptr<FileChunksHandler> chunks_handler_;

  const size_t file_size_;
  base::Value session_endpoints_;
  PartInfo curr_part_;
  base::ListValue uploaded_parts_;
  std::string sha1_digest_;

  base::WeakPtrFactory<BoxChunkedUploader> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_UPLOADER_H_
