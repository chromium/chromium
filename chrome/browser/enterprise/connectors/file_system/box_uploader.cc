// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/box_uploader.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_flow.h"

#include "base/files/file_util.h"
#include "components/prefs/pref_service.h"
#include "net/http/http_status_code.h"

namespace enterprise_connectors {
BoxUploader::BoxUploader(download::DownloadItem* download_item)
    : local_file_path_(download_item->GetFullPath()),
      target_file_name_(download_item->GetTargetFilePath().BaseName()),
      file_size_(download_item->GetTotalBytes()) {}

BoxUploader::~BoxUploader() = default;

void BoxUploader::Init(
    base::RepeatingCallback<void(void)> authentication_retry_callback,
    base::OnceCallback<void(bool)> download_callback,
    PrefService* prefs) {
  authentication_retry_callback_ = authentication_retry_callback;
  download_callback_ = std::move(download_callback);
  DCHECK(prefs);
  prefs_ = prefs;
  folder_id_ = prefs_->GetString(kFileSystemUploadFolderIdPref);
  if (!folder_id_.empty()) {
    current_api_call_ = MakePreflightCheckApiCall();
    return;
  }
  current_api_call_ = MakeFindUpstreamFolderApiCall();
}

void BoxUploader::TryTask(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& access_token) {
  url_loader_factory_ = std::move(url_loader_factory);
  access_token_ = access_token;
  TryCurrentApiCall();
}

void BoxUploader::TryCurrentApiCall() {
  DCHECK(authentication_retry_callback_);
  DCHECK(download_callback_);
  if (!current_api_call_) {
    // Terminate for now until the whole workflow is implemented.
    // Callback with false so that temporary file gets handled?
    // Remember to report via callback(true) when upload is done in:
    // TODO(https://crbug.com/1157636) OnCommitUploadSessionResponse().
    std::move(download_callback_).Run(false);
  } else {
    current_api_call_->Start(url_loader_factory_, access_token_);
  }
}

const std::string& BoxUploader::GetFolderIdForTesting() const {
  return folder_id_;
}

void BoxUploader::NotifyAuthenFailureForTesting() {
  authentication_retry_callback_.Run();
}

void BoxUploader::NotifyResultForTesting(bool success) {
  std::move(download_callback_).Run(success);
}

bool BoxUploader::EnsureSuccessResponse(bool success, int response_code) {
  if (!success) {
    if (response_code == net::HTTP_UNAUTHORIZED) {
      // Authentication failure, so we need to redo authenticaction.
      authentication_retry_callback_.Run();
    } else {
      // Unexpected error. Notify failure to download thread.
      std::move(download_callback_).Run(false);
    }
    return false;
  }
  return true;
}

void BoxUploader::OnPreflightCheckResponse(bool success, int response_code) {
  if (success) {
    // Create an upload session with the same folder_id and name and continue
    CHECK_EQ(response_code, net::HTTP_OK);
    current_api_call_ = MakeFileUploadApiCall();
    TryCurrentApiCall();
    return;
  }
  switch (response_code) {
    case net::HTTP_CONFLICT:
      // TODO(https://crbug.com/1198617) Deal with filename conflict.
      std::move(download_callback_).Run(false);
      break;
    case net::HTTP_UNAUTHORIZED:
      // Authentication failure, we need to reauth and redo the preflight check.
      current_api_call_ = MakePreflightCheckApiCall();
      authentication_retry_callback_.Run();
      break;
    case net::HTTP_NOT_FOUND:
      // Probably because folder id has changed or been deleted. Restart
      // from the top
      prefs_->SetString(kFileSystemUploadFolderIdPref, std::string());
      folder_id_ = std::string();
      current_api_call_ = MakeFindUpstreamFolderApiCall();
      TryCurrentApiCall();
      break;
    default:
      // Unexpected error. Notify failure to download thread.
      std::move(download_callback_).Run(false);
  }
}

void BoxUploader::OnFindUpstreamFolderResponse(bool success,
                                               int response_code,
                                               const std::string& folder_id) {
  if (!EnsureSuccessResponse(success, response_code)) {
    current_api_call_ = MakeFindUpstreamFolderApiCall();
    return;
  }

  if (folder_id.empty()) {
    // Advance to create a new default download folder.
    current_api_call_ = MakeCreateUpstreamFolderApiCall();
  } else {
    folder_id_ = folder_id;
    prefs_->SetString(kFileSystemUploadFolderIdPref, folder_id);
    // Advance to start an upload session.
    current_api_call_ = MakePreflightCheckApiCall();
  }
  TryCurrentApiCall();
}

void BoxUploader::OnCreateUpstreamFolderResponse(bool success,
                                                 int response_code,
                                                 const std::string& folder_id) {
  if (!EnsureSuccessResponse(success, response_code)) {
    current_api_call_ = MakeCreateUpstreamFolderApiCall();
    return;
  }

  CHECK_EQ(folder_id.empty(), false);
  folder_id_ = folder_id;
  prefs_->SetString(kFileSystemUploadFolderIdPref, folder_id);
  // Advance to start an upload session.
  current_api_call_ = MakePreflightCheckApiCall();
  TryCurrentApiCall();
}

std::unique_ptr<OAuth2ApiCallFlow> BoxUploader::MakePreflightCheckApiCall() {
  return std::make_unique<BoxPreflightCheckApiCallFlow>(
      base::BindOnce(&BoxUploader::OnPreflightCheckResponse,
                     weak_factory_.GetWeakPtr()),
      target_file_name_, folder_id_);
}

std::unique_ptr<OAuth2ApiCallFlow> BoxUploader::MakeFileUploadApiCall() {
  if (file_size_ > BoxApiCallFlow::kChunkFileUploadMinSize) {
    return nullptr;
    // TODO(https://crbug.com/1192671) Start an upload session to the chunked
    // file upload endpoint instead.
  } else {
    return std::make_unique<BoxWholeFileUploadApiCallFlow>(
        base::BindOnce(&BoxUploader::OnWholeFileUploadResponse,
                       weak_factory_.GetWeakPtr()),
        folder_id_, target_file_name_, local_file_path_);
  }
}

std::unique_ptr<OAuth2ApiCallFlow>
BoxUploader::MakeFindUpstreamFolderApiCall() {
  return std::make_unique<BoxFindUpstreamFolderApiCallFlow>(base::BindOnce(
      &BoxUploader::OnFindUpstreamFolderResponse, weak_factory_.GetWeakPtr()));
}

std::unique_ptr<OAuth2ApiCallFlow>
BoxUploader::MakeCreateUpstreamFolderApiCall() {
  return std::make_unique<BoxCreateUpstreamFolderApiCallFlow>(
      base::BindOnce(&BoxUploader::OnCreateUpstreamFolderResponse,
                     weak_factory_.GetWeakPtr()));
}

void BoxUploader::OnWholeFileUploadResponse(bool success, int response_code) {
  if (!EnsureSuccessResponse(success, response_code)) {
    current_api_call_ = MakePreflightCheckApiCall();
    return;
  }

  // Report upload success back to the download thread.
  std::move(download_callback_).Run(success);
}

}  // namespace enterprise_connectors
