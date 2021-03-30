// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_DOWNLOAD_CONTROLLER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_DOWNLOAD_CONTROLLER_H_

#include "base/files/file_path.h"
#include "base/values.h"
#include "components/download/public/common/download_item_impl.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"

namespace enterprise_connectors {

// Task Manager for downloaded items used by FileSystemRenamdHandler that
// connects between the Chrome client and the File System Service Provider in
// the cloud. Once given a download item and authentication, internally it
// manages the entire API call flow required to find upstream destination,
// upload the file, and delete the local temporary file.
class FileSystemDownloadController {
 public:
  // Constructor with download::DownloadItem* to access download_item fields but
  // does not store the pointer internally and the ownership of download_item
  // remains with the caller.
  explicit FileSystemDownloadController(download::DownloadItem* download_item);
  virtual ~FileSystemDownloadController();

  // Initialize with callbacks from FileSystemRenameHandler, set
  // current_api_call_ to be the first step of the whole API call workflow. Must
  // be called before calling TryTask() for the first time.
  void Init(base::RepeatingCallback<void(void)> authen_retry_callback,
            base::OnceCallback<void(bool)> download_callback,
            PrefService* prefs);

  // Kick off the workflow from the step stored in current_api_call_. Will
  // re-attempt the last step from where it left off if it called callback with
  // an API call failure earlier.
  void TryTask(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& access_token);

  const std::string& GetFolderIdForTesting() const;
  void NotifyAuthenFailureForTesting();
  void NotifyResultForTesting(bool success);

 private:
  void TryCurrentApiCall();
  bool EnsureSuccessResponse(bool success, int response_code);
  void OnFindUpstreamFolderResponse(bool success,
                                    int response_code,
                                    const std::string& folder_id);
  void OnCreateUpstreamFolderResponse(bool success,
                                      int response_code,
                                      const std::string& folder_id);
  std::unique_ptr<OAuth2ApiCallFlow> CreateUploadApiCall();
  void OnWholeFileUploadResponse(bool success, int response_code);

  // Callback when API call gives Authenetication Error.
  base::RepeatingCallback<void(void)> authentication_retry_callback_;
  // Callback when the entire flow is completed to notify the download thread.
  base::OnceCallback<void(bool)> download_callback_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::string access_token_;

  // Ptr that stores the current OAuth2ApiCallFlow step. It is updated to point
  // to another mini OAuth2ApiCallFlow class whenever the workflow needs to
  // advance to the next step; may also gets re-instantiated when there's an API
  // call failure such that, when the external caller calls TryTask() again, the
  // current step is re-attempted.
  std::unique_ptr<OAuth2ApiCallFlow> current_api_call_;
  // Folder id used to specify the destination folder to the Service Provider in
  // the cloud.
  std::string folder_id_;
  const base::FilePath local_file_path_;
  const base::FilePath target_file_name_;
  const size_t file_size_;
  PrefService* prefs_;

  base::WeakPtrFactory<FileSystemDownloadController> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_DOWNLOAD_CONTROLLER_H_
