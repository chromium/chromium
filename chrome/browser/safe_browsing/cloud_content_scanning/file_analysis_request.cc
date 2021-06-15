// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"

#include "base/files/memory_mapped_file.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"
#include "chrome/services/file_util/public/cpp/sandboxed_zip_analyzer.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/mime_util.h"

namespace safe_browsing {

namespace {

std::string GetFileMimeType(const base::FilePath& path) {
  // TODO(crbug.com/1013252): Obtain a more accurate MimeType by parsing the
  // file content.
  base::FilePath::StringType ext = path.FinalExtension();
  if (ext.empty())
    return "";

  if (ext[0] == FILE_PATH_LITERAL('.'))
    ext = ext.substr(1);

  std::string mime_type;
  net::GetMimeTypeFromExtension(ext, &mime_type);
  return mime_type;
}

std::pair<BinaryUploadService::Result, BinaryUploadService::Request::Data>
GetFileDataBlocking(const base::FilePath& path, bool detect_mime_type) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);

  if (!file.IsValid()) {
    return std::make_pair(BinaryUploadService::Result::UNKNOWN,
                          BinaryUploadService::Request::Data());
  }

  size_t file_size = file.GetLength();
  if (file_size == 0) {
    return std::make_pair(BinaryUploadService::Result::SUCCESS,
                          BinaryUploadService::Request::Data());
  }

  base::MemoryMappedFile mm_file;
  if (!mm_file.Initialize(std::move(file)) || !mm_file.IsValid()) {
    return std::make_pair(BinaryUploadService::Result::UNKNOWN,
                          BinaryUploadService::Request::Data());
  }

  BinaryUploadService::Request::Data file_data;
  file_data.size = file_size;
  file_data.path = path;
  if (detect_mime_type)
    file_data.mime_type = GetFileMimeType(path);

  std::unique_ptr<crypto::SecureHash> secure_hash =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  secure_hash->Update(mm_file.data(), file_size);
  file_data.hash.resize(crypto::kSHA256Length);
  secure_hash->Finish(base::data(file_data.hash), crypto::kSHA256Length);
  file_data.hash =
      base::HexEncode(base::as_bytes(base::make_span(file_data.hash)));

  return {file_size <= BinaryUploadService::kMaxUploadSizeBytes
              ? BinaryUploadService::Result::SUCCESS
              : BinaryUploadService::Result::FILE_TOO_LARGE,
          file_data};
}

}  // namespace

FileAnalysisRequest::FileAnalysisRequest(
    const enterprise_connectors::AnalysisSettings& analysis_settings,
    base::FilePath path,
    base::FilePath file_name,
    std::string mime_type,
    BinaryUploadService::ContentAnalysisCallback callback)
    : Request(std::move(callback), analysis_settings.analysis_url),
      has_cached_result_(false),
      block_unsupported_types_(analysis_settings.block_unsupported_file_types),
      path_(std::move(path)),
      file_name_(std::move(file_name)) {
  set_filename(file_name_.AsUTF8Unsafe());
  cached_data_.mime_type = std::move(mime_type);
}

FileAnalysisRequest::~FileAnalysisRequest() = default;

void FileAnalysisRequest::GetRequestData(DataCallback callback) {
  if (has_cached_result_) {
    std::move(callback).Run(cached_result_, cached_data_);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&GetFileDataBlocking, path_,
                     cached_data_.mime_type.empty()),
      base::BindOnce(&FileAnalysisRequest::OnGotFileData,
                     weakptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool FileAnalysisRequest::FileTypeUnsupportedByDlp() const {
  for (const std::string& tag : content_analysis_request().tags()) {
    if (tag == "dlp")
      return !FileTypeSupportedForDlp(file_name_);
  }
  return false;
}

bool FileAnalysisRequest::HasMalwareRequest() const {
  for (const std::string& tag : content_analysis_request().tags()) {
    if (tag == "malware")
      return true;
  }
  return false;
}

void FileAnalysisRequest::OnGotFileData(
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
        base::BindOnce(&FileAnalysisRequest::OnCheckedForEncryption,
                       weakptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(result_and_data.second)),
        LaunchFileUtilService());
    analyzer->Start();
  } else if (ext == FILE_PATH_LITERAL(".rar")) {
    auto analyzer = base::MakeRefCounted<SandboxedRarAnalyzer>(
        path_,
        base::BindOnce(&FileAnalysisRequest::OnCheckedForEncryption,
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

void FileAnalysisRequest::OnCheckedForEncryption(
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

void FileAnalysisRequest::CacheResultAndData(BinaryUploadService::Result result,
                                             Data data) {
  has_cached_result_ = true;
  cached_result_ = result;

  // If the mime type is already set, it shouldn't be overwritten.
  if (!cached_data_.mime_type.empty())
    data.mime_type = std::move(cached_data_.mime_type);

  cached_data_ = std::move(data);
}

}  // namespace safe_browsing
