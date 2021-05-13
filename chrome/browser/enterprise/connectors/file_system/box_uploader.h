// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_UPLOADER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_UPLOADER_H_

#include "base/files/file_path.h"
#include "components/download/public/common/download_item_impl.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"

namespace enterprise_connectors {

// Task Manager for downloaded items used by FileSystemRenamdHandler that
// connects between the Chrome client and Box. Once given a download item and
// authentication, internally it manages the entire API call flow required to
// find upstream destination, upload the file, and delete the local temporary
// file.
class BoxUploader {
 public:
  static std::unique_ptr<BoxUploader> Create(
      download::DownloadItem* download_item);

  virtual ~BoxUploader();

  using RenameHandlerCallback = base::OnceCallback<void(bool)>;

  // Initialize with callbacks from FileSystemRenameHandler, set
  // current_api_call_ to be the first step of the whole API call workflow. Must
  // be called before calling TryTask() for the first time.
  void Init(base::RepeatingCallback<void(void)> authen_retry_callback,
            RenameHandlerCallback download_callback,
            PrefService* prefs);

  // Kick off the workflow from the step stored in current_api_call_. Will
  // re-attempt the last step from where it left off if it called callback with
  // an API call failure earlier.
  virtual void TryTask(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token);

  virtual GURL GetUploadedFileUrl() const;
  virtual GURL GetDestinationFolderUrl() const;

  // Helper methods for unit tests.
  std::string GetFolderIdForTesting() const;
  void NotifyOAuth2ErrorForTesting();
  void NotifyResultForTesting(bool success);

  class FileChunksHandler;  // To be moved into BoxChunkedFileUploader.

 protected:
  // Constructor with download::DownloadItem* to access download_item fields but
  // does not store the pointer internally and the ownership of download_item
  // remains with the caller.
  explicit BoxUploader(download::DownloadItem* download_item);

  void TryCurrentApiCall();
  bool EnsureSuccessResponse(bool success, int response_code);
  void OnApiCallFlowDone(bool upload_success, GURL uploaded_file_url);
  void NotifyResult(bool success);

  // To be overridden to test API calls flow and file delete separately.
  virtual void StartCurrentApiCall();
  // Must be implemented in child classes.
  virtual std::unique_ptr<OAuth2ApiCallFlow> MakeFileUploadApiCall() = 0;
  // Can be overridden to handle failure differently from simply calling
  // OnApiCallFlowDone(false).
  virtual void OnApiCallFlowFailure();

  const base::FilePath GetLocalFilePath() const;
  const base::FilePath GetTargetFileName() const;
  const std::string GetFolderId();
  const std::string GetFolderId() const;
  void SetFolderId(std::string folder_id);
  void SetCurrentApiCall(std::unique_ptr<OAuth2ApiCallFlow> api_call);

 private:
  // Box API call pre-upload steps:
  std::unique_ptr<OAuth2ApiCallFlow> MakeFindUpstreamFolderApiCall();
  std::unique_ptr<OAuth2ApiCallFlow> MakeCreateUpstreamFolderApiCall();
  std::unique_ptr<OAuth2ApiCallFlow> MakePreflightCheckApiCall();

  // Callbacks from Box*ApiCallFlows:
  void OnFindUpstreamFolderResponse(bool success,
                                    int response_code,
                                    const std::string& folder_id);
  void OnCreateUpstreamFolderResponse(bool success,
                                      int response_code,
                                      const std::string& folder_id);
  void OnPreflightCheckResponse(bool success, int response_code);

  // The followings are not necessarily specific to Box:
  // Post a task to ThreadPool to delete the local file, after the entire file
  // has been uploaded, with callback OnFileDeleted(). Arg of |delete_cb|
  // indicates whether deletion succeeded.
  void PostDeleteFileTask(base::OnceCallback<void(bool)> delete_cb);
  // Callback attached in PostDeleteFileTask(). Report success back to original
  // thread via download_callback_.
  void OnFileDeleted(bool upload_success, bool delete_success);

  // File details.
  const base::FilePath local_file_path_;   // Path of the local temporary file.
  const base::FilePath target_file_name_;  // File name to be used finally.
  // Callback when API call gives Authenetication Error.
  base::RepeatingCallback<void(void)> authentication_retry_callback_;
  // Callback when the entire flow is completed to notify the download thread.
  RenameHandlerCallback download_callback_;
  // Used for OAuth2ApiCallFlow::Start():
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::string access_token_;
  // Ptr that stores the current OAuth2ApiCallFlow step. It is updated to point
  // to another mini OAuth2ApiCallFlow class whenever the workflow needs to
  // advance to the next step; may also gets re-instantiated when there's an API
  // call failure such that, when the external caller calls TryTask() again, the
  // current step is re-attempted.
  std::unique_ptr<OAuth2ApiCallFlow> current_api_call_;
  // Folder id used to specify the destination folder for the Service Provider.
  std::string folder_id_;
  // PrefService used to store folder_id.
  PrefService* prefs_;
  // URL to show uploaded file.
  GURL file_url_;

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
  void OnWholeFileUploadResponse(bool success, int response_code, GURL url);

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
    std::string content;
    size_t byte_from;  // Inclusive of 1st byte of the file part.
    size_t byte_to;    // Inclusive of last byte in the file part.
    // Therefore byte_to == byte_from + content.size() - 1.
  };

 private:
  // BoxUploader interface.
  void OnApiCallFlowFailure() override;
  std::unique_ptr<OAuth2ApiCallFlow> MakeFileUploadApiCall() override;

  // Helper methods to transition between chunked upload steps.
  std::unique_ptr<OAuth2ApiCallFlow> MakeCreateUploadSessionApiCall();
  std::unique_ptr<OAuth2ApiCallFlow> MakePartFileUploadApiCall();
  std::unique_ptr<OAuth2ApiCallFlow> MakeCommitUploadSessionApiCall();
  std::unique_ptr<OAuth2ApiCallFlow> MakeAbortUploadSessionApiCall();

  // Callbacks for chunked file upload.
  void OnCreateUploadSessionResponse(bool success,
                                     int response_code,
                                     base::Value session_endpoints,
                                     size_t part_size);
  void OnPartFileUploadResponse(bool success,
                                int response_code,
                                base::Value part_info);
  void OnCommitUploadSsessionResponse(bool success,
                                      int response_code,
                                      base::TimeDelta retry_after,
                                      GURL file_url);
  void OnAbortUploadSsessionResponse(bool success, int response_code);

  // Callbacks for chunks_handler_.

  void OnFileChunkRead(PartInfo part_info);
  void OnFileCompletelyUploaded(const std::string& sha1_digest);

  std::unique_ptr<FileChunksHandler> chunks_handler_;

  const size_t file_size_;
  base::Value session_endpoints_;
  PartInfo curr_part_;
  base::Value uploaded_parts_;
  std::string sha1_digest_;
  base::WeakPtrFactory<BoxChunkedUploader> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_UPLOADER_H_
