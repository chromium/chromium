// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/resumable_drive_uploader.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/byte_count.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/save_to_drive/content_reader.h"
#include "chrome/browser/save_to_drive/drive_uploader.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"

namespace save_to_drive {

namespace {

using extensions::api::pdf_viewer_private::SaveToDriveErrorType;
using extensions::api::pdf_viewer_private::SaveToDriveProgress;
using extensions::api::pdf_viewer_private::SaveToDriveStatus;

constexpr char kDriveUploadUrl[] =
    "https://www.googleapis.com/upload/drive/v3beta/files";
constexpr char kResumableUploadProtocol[] = "resumable";

// Resumable upload request headers.
constexpr char kXUploadContentLengthHeader[] =
    "X-Goog-Upload-Header-Content-Length";
constexpr char kXUploadContentTypeHeader[] =
    "X-Goog-Upload-Header-Content-Type";
constexpr char kXUploadCommandHeader[] = "X-Goog-Upload-Command";
constexpr char kXUploadOffsetHeader[] = "X-Goog-Upload-Offset";
constexpr char kXUploadProtocolHeader[] = "X-Goog-Upload-Protocol";

// Resumable upload response headers.
constexpr char kXUploadStatusResponseHeader[] = "X-Goog-Upload-Status";
constexpr char kXUploadUrlResponseHeader[] = "X-Goog-Upload-URL";

// Resumable upload commands and statuses.
constexpr char kStartUploadCommand[] = "start";
constexpr char kUploadCommand[] = "upload";
constexpr char kUploadFinalizeCommand[] = "upload, finalize";
constexpr char kUploadStatusActive[] = "active";
constexpr char kUploadStatusFinal[] = "final";

// Content types.
constexpr char kDataContentType[] = "application/octet-stream";
constexpr char kJsonContentType[] = "application/json; charset=UTF-8";

// The maximum size of a chunk to upload at a time. This should be multiple
// of 256KiB. See
// https://developers.google.com/workspace/drive/api/guides/manage-uploads#resumable
constexpr base::ByteCount kChunkSize = base::MiB(2);

}  // namespace

ResumableDriveUploader::ResumableDriveUploader(
    std::string title,
    AccountInfo account_info,
    ProgressCallback progress_callback,
    Profile* profile,
    ContentReader* content_reader)
    : DriveUploader(DriveUploaderType::kResumable,
                    std::move(title),
                    std::move(account_info),
                    std::move(progress_callback),
                    profile,
                    content_reader) {}

ResumableDriveUploader::~ResumableDriveUploader() = default;

void ResumableDriveUploader::UploadFile() {
  const std::vector<std::string>& headers = oauth_headers();
  if (headers.empty()) {
    NotifyError(SaveToDriveErrorType::kOauthError);
    return;
  }
  if (!parent_folder_) {
    NotifyError(SaveToDriveErrorType::kParentFolderSelectionFailed);
    return;
  }
  std::vector<std::string> request_headers = headers;
  request_headers.insert(
      request_headers.end(),
      {// X-Goog-Upload-Protocol: resumable
       kXUploadProtocolHeader, kResumableUploadProtocol,
       // X-Goog-Upload-Command: start
       kXUploadCommandHeader, kStartUploadCommand,
       // X-Goog-Upload-Header-Content-Type: application/octet-stream
       kXUploadContentTypeHeader, kDataContentType,
       // X-Goog-Upload-Header-Content-Length: <content_length>
       kXUploadContentLengthHeader,
       base::NumberToString(content_reader_->GetSize())});
  initiation_endpoint_fetcher_ = CreateEndpointFetcher(
      GURL(kDriveUploadUrl), endpoint_fetcher::HttpMethod::kPost,
      kJsonContentType,
      *base::WriteJson(
          base::Value::Dict()
              .Set("name", title_)
              .Set("parents", base::Value::List().Append(parent_folder_->id))),
      request_headers, base::DoNothing());

  initiation_endpoint_fetcher_->Fetch(
      base::BindOnce(&ResumableDriveUploader::HandleInitiationResponse,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ResumableDriveUploader::HandleInitiationResponse(
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  if (response->http_status_code != net::HTTP_OK) {
    NotifyUploadFailure(std::move(response));
    return;
  }

  std::string_view upload_status =
      response->headers->EnumerateHeader(nullptr, kXUploadStatusResponseHeader)
          .value_or("");
  if (upload_status != kUploadStatusActive) {
    NotifyError(SaveToDriveErrorType::kUnknownError);
    return;
  }

  upload_url_ = GURL(
      response->headers->EnumerateHeader(nullptr, kXUploadUrlResponseHeader)
          .value_or(""));
  if (!upload_url_.is_valid()) {
    NotifyError(SaveToDriveErrorType::kUnknownError);
    return;
  }

  offset_ = 0;
  const size_t chunk_size =
      std::min<size_t>(kChunkSize.InBytes(), content_reader_->GetSize());
  content_reader_->Read(offset_, chunk_size,
                        base::BindOnce(&ResumableDriveUploader::OnContentRead,
                                       weak_ptr_factory_.GetWeakPtr()));
}

void ResumableDriveUploader::OnContentRead(mojo_base::BigBuffer buffer) {
  const size_t chunk_size = buffer.size();
  CHECK(upload_url_.is_valid());
  if (chunk_size == 0) {
    NotifyError(SaveToDriveErrorType::kUnknownError);
    return;
  }
  std::vector<std::string> headers = oauth_headers();
  bool is_final_chunk = (offset_ + chunk_size) == content_reader_->GetSize();
  std::string upload_command =
      is_final_chunk ? kUploadFinalizeCommand : kUploadCommand;

  headers.insert(headers.end(),
                 {
                     // X-Goog-Upload-Protocol: resumable
                     kXUploadProtocolHeader,
                     kResumableUploadProtocol,
                     // X-Goog-Upload-Command: upload | (upload, finalize)
                     kXUploadCommandHeader,
                     std::move(upload_command),
                     // X-Goog-Upload-Offset: <offset>
                     kXUploadOffsetHeader,
                     base::NumberToString(offset_),
                 }

  );

  upload_endpoint_fetcher_ = CreateEndpointFetcher(
      upload_url_, endpoint_fetcher::HttpMethod::kPut, kDataContentType,
      base::as_string_view(buffer), headers,
      base::BindRepeating(&ResumableDriveUploader::OnUploadProgress,
                          weak_ptr_factory_.GetWeakPtr()));
  upload_endpoint_fetcher_->Fetch(
      base::BindOnce(&ResumableDriveUploader::HandleUploadResponse,
                     weak_ptr_factory_.GetWeakPtr(), chunk_size));
}

void ResumableDriveUploader::NotifyUploadProgress(size_t uploaded_bytes) {
  const size_t file_size = content_reader_->GetSize();
  CHECK_LE(uploaded_bytes, file_size);
  NotifyUploadInProgress(uploaded_bytes, file_size);
}

void ResumableDriveUploader::OnUploadProgress(uint64_t current_chunk_bytes,
                                              uint64_t total_chunk_bytes) {
  NotifyUploadProgress(offset_ + current_chunk_bytes);
}

void ResumableDriveUploader::HandleHttpSuccessUploadResponse(
    size_t chunk_size,
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  offset_ += chunk_size;
  const size_t file_size = content_reader_->GetSize();
  std::string_view upload_status =
      response->headers->EnumerateHeader(nullptr, kXUploadStatusResponseHeader)
          .value_or("");
  if (upload_status == kUploadStatusFinal) {
    CHECK_EQ(offset_, file_size);
    NotifyUploadSuccess(std::move(response));
    return;
  }
  if (upload_status != kUploadStatusActive) {
    NotifyError(SaveToDriveErrorType::kUnknownError);
    return;
  }
  CHECK_LT(offset_, file_size);
  NotifyUploadProgress(offset_);
  content_reader_->Read(
      offset_, std::min<size_t>(kChunkSize.InBytes(), file_size - offset_),
      base::BindOnce(&ResumableDriveUploader::OnContentRead,
                     weak_ptr_factory_.GetWeakPtr()));
}

// See
// https://developers.google.com/workspace/drive/api/guides/manage-uploads#resumable
void ResumableDriveUploader::HandleUploadResponse(
    size_t chunk_size,
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  switch (response->http_status_code) {
    case net::HTTP_OK:
    case net::HTTP_CREATED:
      HandleHttpSuccessUploadResponse(chunk_size, std::move(response));
      return;
    case net::HTTP_NOT_FOUND:
      // The upload was not found. This means that the upload was not completed,
      // so the upload was likely interrupted. The upload needs to be
      // restarted from the beginning.
      UploadFile();
      return;
    default:
      // TODO(crbug.com/435142523): Consider retrying the upload automatically
      // on retryable errors. Currently, when an error occurs, it is
      // shown to the user. The user needs to manually retry the upload.
      NotifyUploadFailure(std::move(response));
      return;
  }
}

}  // namespace save_to_drive
