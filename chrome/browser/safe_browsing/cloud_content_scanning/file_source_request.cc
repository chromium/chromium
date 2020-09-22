// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/file_source_request.h"

#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"
#include "chrome/services/file_util/public/cpp/sandboxed_zip_analyzer.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

namespace safe_browsing {

namespace {

std::pair<BinaryUploadService::Result, BinaryUploadService::Request::Data>
GetFileContentsForLargeFile(const base::FilePath& path, base::File* file) {
  size_t file_size = file->GetLength();
  BinaryUploadService::Request::Data file_data;
  file_data.size = file_size;

  // Only read 50MB at a time to avoid having very large files in memory.
  std::unique_ptr<crypto::SecureHash> secure_hash =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  size_t bytes_read = 0;
  std::string buf;
  buf.reserve(BinaryUploadService::kMaxUploadSizeBytes);
  while (bytes_read < file_size) {
    int64_t bytes_currently_read = file->ReadAtCurrentPos(
        &buf[0], BinaryUploadService::kMaxUploadSizeBytes);

    if (bytes_currently_read == -1) {
      return std::make_pair(BinaryUploadService::Result::UNKNOWN,
                            BinaryUploadService::Request::Data());
    }

    secure_hash->Update(buf.data(), bytes_currently_read);

    bytes_read += bytes_currently_read;
  }

  file_data.hash.resize(crypto::kSHA256Length);
  secure_hash->Finish(base::data(file_data.hash), crypto::kSHA256Length);
  file_data.hash =
      base::HexEncode(base::as_bytes(base::make_span(file_data.hash)));
  return std::make_pair(BinaryUploadService::Result::FILE_TOO_LARGE, file_data);
}

std::pair<BinaryUploadService::Result, BinaryUploadService::Request::Data>
GetFileContentsForNormalFile(const base::FilePath& path, base::File* file) {
  size_t file_size = file->GetLength();
  BinaryUploadService::Request::Data file_data;
  file_data.size = file_size;
  file_data.contents.resize(file_size);

  int64_t bytes_currently_read =
      file->ReadAtCurrentPos(&file_data.contents[0], file_size);

  if (bytes_currently_read == -1) {
    return std::make_pair(BinaryUploadService::Result::UNKNOWN,
                          BinaryUploadService::Request::Data());
  }

  DCHECK_EQ(static_cast<size_t>(bytes_currently_read), file_size);

  file_data.hash = crypto::SHA256HashString(file_data.contents);
  file_data.hash =
      base::HexEncode(base::as_bytes(base::make_span(file_data.hash)));
  return std::make_pair(BinaryUploadService::Result::SUCCESS, file_data);
}

std::pair<BinaryUploadService::Result, BinaryUploadService::Request::Data>
GetFileDataBlocking(const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    return std::make_pair(BinaryUploadService::Result::UNKNOWN,
                          BinaryUploadService::Request::Data());
  }

  return static_cast<size_t>(file.GetLength()) >
                 BinaryUploadService::kMaxUploadSizeBytes
             ? GetFileContentsForLargeFile(path, &file)
             : GetFileContentsForNormalFile(path, &file);
}

}  // namespace

FileSourceRequest::FileSourceRequest(
    const enterprise_connectors::AnalysisSettings& analysis_settings,
    base::FilePath path,
    base::FilePath file_name,
    BinaryUploadService::ContentAnalysisCallback callback)
    : Request(std::move(callback), analysis_settings.analysis_url),
      has_cached_result_(false),
      block_unsupported_types_(analysis_settings.block_unsupported_file_types),
      path_(std::move(path)),
      file_name_(std::move(file_name)) {
  set_filename(file_name_.AsUTF8Unsafe());
}

FileSourceRequest::~FileSourceRequest() = default;

void FileSourceRequest::GetRequestData(DataCallback callback) {
  if (has_cached_result_) {
    std::move(callback).Run(cached_result_, cached_data_);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&GetFileDataBlocking, path_),
      base::BindOnce(&FileSourceRequest::OnGotFileData,
                     weakptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool FileSourceRequest::FileTypeUnsupportedByDlp() const {
  for (const std::string& tag : content_analysis_request().tags()) {
    if (tag == "dlp")
      return !FileTypeSupportedForDlp(file_name_);
  }
  return false;
}

bool FileSourceRequest::HasMalwareRequest() const {
  for (const std::string& tag : content_analysis_request().tags()) {
    if (tag == "malware")
      return true;
  }
  return false;
}

void FileSourceRequest::OnGotFileData(
    DataCallback callback,
    std::pair<BinaryUploadService::Result, Data> result_and_data) {
  set_digest(result_and_data.second.hash);

  if (result_and_data.first != BinaryUploadService::Result::SUCCESS) {
    CacheResultAndData(result_and_data.first,
                       std::move(result_and_data.second));
    std::move(callback).Run(cached_result_, cached_data_);
    return;
  }

  if (FileTypeUnsupportedByDlp()) {
    // Abort the request early if settings say to block unsupported types or if
    // there was no malware request to be done, otherwise proceed with the
    // malware request only.
    if (block_unsupported_types_ || !HasMalwareRequest()) {
      CacheResultAndData(
          BinaryUploadService::Result::DLP_SCAN_UNSUPPORTED_FILE_TYPE,
          std::move(result_and_data.second));
      std::move(callback).Run(cached_result_, cached_data_);
      return;
    } else {
      clear_dlp_scan_request();
    }
  }

  base::FilePath::StringType ext(file_name_.FinalExtension());
  std::transform(ext.begin(), ext.end(), ext.begin(), tolower);
  if (ext == FILE_PATH_LITERAL(".zip")) {
    auto analyzer = base::MakeRefCounted<SandboxedZipAnalyzer>(
        path_,
        base::BindOnce(&FileSourceRequest::OnCheckedForEncryption,
                       weakptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(result_and_data.second)),
        LaunchFileUtilService());
    analyzer->Start();
  } else if (ext == FILE_PATH_LITERAL(".rar")) {
    auto analyzer = base::MakeRefCounted<SandboxedRarAnalyzer>(
        path_,
        base::BindOnce(&FileSourceRequest::OnCheckedForEncryption,
                       weakptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(result_and_data.second)),
        LaunchFileUtilService());
    analyzer->Start();
  } else {
    CacheResultAndData(BinaryUploadService::Result::SUCCESS,
                       std::move(result_and_data.second));
    std::move(callback).Run(cached_result_, cached_data_);
  }
}

void FileSourceRequest::OnCheckedForEncryption(
    DataCallback callback,
    Data data,
    const ArchiveAnalyzerResults& analyzer_result) {
  bool encrypted =
      std::any_of(analyzer_result.archived_binary.begin(),
                  analyzer_result.archived_binary.end(),
                  [](const auto& binary) { return binary.is_encrypted(); });

  BinaryUploadService::Result result =
      encrypted ? BinaryUploadService::Result::FILE_ENCRYPTED
                : BinaryUploadService::Result::SUCCESS;
  CacheResultAndData(result, std::move(data));
  std::move(callback).Run(cached_result_, cached_data_);
}

void FileSourceRequest::CacheResultAndData(BinaryUploadService::Result result,
                                           Data data) {
  has_cached_result_ = true;
  cached_result_ = result;
  cached_data_ = std::move(data);
}

}  // namespace safe_browsing
