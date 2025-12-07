// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/multipart_drive_uploader.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/save_to_drive/content_reader.h"
#include "chrome/browser/save_to_drive/drive_uploader.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace save_to_drive {
namespace {

using extensions::api::pdf_viewer_private::SaveToDriveErrorType;
using extensions::api::pdf_viewer_private::SaveToDriveProgress;
using extensions::api::pdf_viewer_private::SaveToDriveStatus;

constexpr char kContentTypeKey[] = "Content-Type";
constexpr char kDataContentType[] = "Content-Type: application/octet-stream";
constexpr char kDriveUploadUrl[] =
    "https://www.googleapis.com/upload/drive/v3beta/files";
constexpr char kMetadataContentType[] =
    "Content-Type: application/json; charset=UTF-8";
constexpr char kMultiPartUploadType[] = "multipart";
constexpr char kUploadContentLengthKey[] =
    "X-Goog-Upload-Header-Content-Length";
constexpr char kUploadProtocolKey[] = "X-Goog-Upload-Protocol";
constexpr char kUploadTypeQueryParameterKey[] = "uploadType";

}  // namespace

MultipartDriveUploader::MultipartDriveUploader(
    std::string title,
    AccountInfo account_info,
    ProgressCallback progress_callback,
    Profile* profile,
    ContentReader* content_reader)
    : DriveUploader(DriveUploaderType::kMultipart,
                    std::move(title),
                    std::move(account_info),
                    std::move(progress_callback),
                    profile,
                    content_reader) {}

MultipartDriveUploader::~MultipartDriveUploader() = default;

void MultipartDriveUploader::UploadFile() {
  auto callback = base::BindOnce(&MultipartDriveUploader::OnContentRead,
                                 weak_ptr_factory_.GetWeakPtr());
  content_reader_->Read(0, content_reader_->GetSize(), std::move(callback));
}

void MultipartDriveUploader::OnContentRead(mojo_base::BigBuffer buffer) {
  const std::vector<std::string>& headers = oauth_headers();
  SaveToDriveErrorType error_type = SaveToDriveErrorType::kNoError;
  if (headers.empty()) {
    error_type = SaveToDriveErrorType::kOauthError;
  } else if (!parent_folder_) {
    error_type = SaveToDriveErrorType::kParentFolderSelectionFailed;
  } else if (buffer.size() == 0) {
    error_type = SaveToDriveErrorType::kUnknownError;
  }
  if (error_type != SaveToDriveErrorType::kNoError) {
    NotifyError(error_type);
    return;
  }
  const GURL url = net::AppendOrReplaceQueryParameter(
      GURL(kDriveUploadUrl), kUploadTypeQueryParameterKey,
      kMultiPartUploadType);
  base::Value::Dict metadata_dict;
  metadata_dict.Set("name", title_);
  metadata_dict.Set("parents", base::Value::List().Append(parent_folder_->id));

  const std::string boundary = net::GenerateMimeMultipartBoundary();
  const std::string request_body =
      base::StrCat({"--", boundary, "\r\n", kMetadataContentType, "\r\n\r\n",
                    *base::WriteJson(metadata_dict), "\r\n--", boundary, "\r\n",
                    kDataContentType, "\r\n\r\n", base::as_string_view(buffer),
                    "\r\n--", boundary, "--\r\n"});

  const std::string content_type =
      base::StrCat({"multipart/related; boundary=", boundary});

  std::vector<std::string> request_headers = headers;
  request_headers.insert(
      request_headers.end(),
      {kContentTypeKey, content_type, kUploadProtocolKey, kMultiPartUploadType,
       kUploadContentLengthKey, base::NumberToString(buffer.size())});

  upload_endpoint_fetcher_ = CreateEndpointFetcher(
      url, endpoint_fetcher::HttpMethod::kPost, content_type, request_body,
      request_headers,
      base::BindRepeating(&MultipartDriveUploader::OnUploadProgress,
                          weak_ptr_factory_.GetWeakPtr()));
  upload_endpoint_fetcher_->Fetch(
      base::BindOnce(&MultipartDriveUploader::HandleUploadResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MultipartDriveUploader::OnUploadProgress(uint64_t current_bytes,
                                              uint64_t total_bytes) {
  NotifyUploadInProgress(current_bytes, total_bytes);
}

void MultipartDriveUploader::HandleUploadResponse(
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  if (response->http_status_code == net::HTTP_OK ||
      response->http_status_code == net::HTTP_CREATED) {
    NotifyUploadSuccess(std::move(response));
    return;
  }
  NotifyUploadFailure(std::move(response));
}

}  // namespace save_to_drive
