// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/file_analysis_request.h"

#include <string_view>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/memory_mapped_file.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/filename_util.h"
#include "net/base/mime_sniffer.h"
#include "net/base/mime_util.h"

namespace safe_browsing {

namespace {

constexpr size_t kReadFileChunkSize = 4096;

std::string GetFileMimeType(const base::FilePath& path,
                            std::string_view first_bytes) {
  std::string sniffed_mime_type;
  bool sniff_found = net::SniffMimeType(
      std::string_view(first_bytes.data(),
                       std::min(first_bytes.size(),
                                static_cast<size_t>(net::kMaxBytesToSniff))),
      net::FilePathToFileURL(path),
      /*type_hint*/ std::string(), net::ForceSniffFileUrlsForHtml::kDisabled,
      &sniffed_mime_type);

  if (sniff_found && !sniffed_mime_type.empty() &&
      sniffed_mime_type != "text/*" &&
      sniffed_mime_type != "application/octet-stream") {
    return sniffed_mime_type;
  }

  // If the file got a trivial or empty mime type sniff, fall back to using its
  // extension if possible.
  base::FilePath::StringType ext = path.FinalExtension();
  if (ext.empty())
    return sniffed_mime_type;

  if (ext[0] == FILE_PATH_LITERAL('.'))
    ext = ext.substr(1);

  std::string ext_mime_type;
  bool ext_found = net::GetMimeTypeFromExtension(ext, &ext_mime_type);

  if (!ext_found || ext_mime_type.empty())
    return sniffed_mime_type;

  return ext_mime_type;
}

std::pair<BinaryUploadService::Result, BinaryUploadService::Request::Data>
GetFileDataBlocking(const base::FilePath& path,
                    bool detect_mime_type,
                    bool is_obfuscated) {
  DCHECK(!path.empty());

  // The returned `Data` must always have a valid `path` member, regardless
  // if this function succeeds or not.  The other members of `Data` may or
  // may not be filled in.
  BinaryUploadService::Request::Data file_data;
  file_data.path = path;

  // FLAG_WIN_SHARE_DELETE is necessary to allow the file to be renamed by the
  // user clicking "Open Now" without causing download errors.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE);

  if (!file.IsValid()) {
    return std::make_pair(BinaryUploadService::Result::UNKNOWN, file_data);
  }

  file_data.size = file.GetLength();
  if (file_data.size == 0) {
    return std::make_pair(BinaryUploadService::Result::SUCCESS, file_data);
  }

  std::unique_ptr<crypto::SecureHash> secure_hash =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  size_t bytes_read = 0;
  std::vector<char> buf(kReadFileChunkSize);

  while (bytes_read < file_data.size) {
    std::optional<size_t> bytes_currently_read =
        file.ReadAtCurrentPos(base::as_writable_byte_span(buf));
    if (!bytes_currently_read.has_value()) {
      // Reset the size to zero since some code assumes an UNKNOWN result is
      // matched with a zero size.
      file_data.size = 0;
      return {BinaryUploadService::Result::UNKNOWN, file_data};
    }

    // Use the first read chunk to get the mimetype as necessary.
    if (detect_mime_type && bytes_read == 0) {
      file_data.mime_type = GetFileMimeType(
          path, std::string_view(buf.data(), bytes_currently_read.value()));
    }

    secure_hash->Update(
        base::as_byte_span(buf).subspan(0, bytes_currently_read.value()));
    bytes_read += bytes_currently_read.value();
  }

  std::array<uint8_t, crypto::kSHA256Length> hash;
  secure_hash->Finish(hash);

  // TODO(b/367257039): Pass along hash of unobfuscated file for enterprise
  // scans
  file_data.hash = base::HexEncode(hash);

  // Since we will be sending the deobfuscated file data in the request, set the
  // size to match.
  if (is_obfuscated) {
    enterprise_obfuscation::DownloadObfuscator obfuscator;
    auto overhead = obfuscator.CalculateDeobfuscationOverhead(file);
    if (overhead.has_value()) {
      file_data.size -= overhead.value();
    }
  }

  return {file_data.size <= BinaryUploadService::kMaxUploadSizeBytes
              ? BinaryUploadService::Result::SUCCESS
              : BinaryUploadService::Result::FILE_TOO_LARGE,
          std::move(file_data)};
}

bool IsZipFile(const base::FilePath::StringType& extension,
               const std::string& mime_type) {
  return extension == FILE_PATH_LITERAL(".zip") ||
         mime_type == "application/x-zip-compressed" ||
         mime_type == "application/zip";
}

bool IsRarFile(const base::FilePath::StringType& extension,
               const std::string& mime_type) {
  return extension == FILE_PATH_LITERAL(".rar") ||
         mime_type == "application/vnd.rar" ||
         mime_type == "application/x-rar-compressed";
}

}  // namespace

FileAnalysisRequest::FileAnalysisRequest(
    const enterprise_connectors::AnalysisSettings& analysis_settings,
    base::FilePath path,
    base::FilePath file_name,
    std::string mime_type,
    bool delay_opening_file,
    BinaryUploadService::ContentAnalysisCallback callback,
    BinaryUploadService::Request::RequestStartCallback start_callback,
    bool is_obfuscated)
    : Request(std::move(callback),
              analysis_settings.cloud_or_local_settings,
              std::move(start_callback)),
      has_cached_result_(false),
      tag_settings_(analysis_settings.tags),
      path_(std::move(path)),
      file_name_(std::move(file_name)),
      delay_opening_file_(delay_opening_file),
      is_obfuscated_(is_obfuscated) {
  DCHECK(!path_.empty());
  set_filename(path_.AsUTF8Unsafe());
  cached_data_.mime_type = std::move(mime_type);
}

FileAnalysisRequest::~FileAnalysisRequest() = default;

void FileAnalysisRequest::GetRequestData(DataCallback callback) {
  data_callback_ = std::move(callback);

  if (has_cached_result_) {
    RunCallback();
    return;
  }

  if (!delay_opening_file_) {
    file_access::RequestFilesAccessForSystem(
        {path_}, base::BindOnce(&FileAnalysisRequest::GetData,
                                weakptr_factory_.GetWeakPtr()));
  }
}

void FileAnalysisRequest::OpenFile() {
  DCHECK(!data_callback_.is_null());

  // Opening the file synchronously here is OK since OpenFile should be called
  // on a base::MayBlock() thread.
  std::pair<BinaryUploadService::Result, Data> file_data = GetFileDataBlocking(
      path_, cached_data_.mime_type.empty(), is_obfuscated_);

  // The result of opening the file is passed back to the UI thread since
  // |data_callback_| calls functions that must run there.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FileAnalysisRequest::OnGotFileData,
                     weakptr_factory_.GetWeakPtr(), std::move(file_data)));
}

bool FileAnalysisRequest::HasMalwareRequest() const {
  for (const std::string& tag : content_analysis_request().tags()) {
    if (tag == "malware")
      return true;
  }
  return false;
}

void FileAnalysisRequest::OnGotFileData(
    std::pair<BinaryUploadService::Result, Data> result_and_data) {
  DCHECK(!result_and_data.second.path.empty());
  DCHECK_EQ(result_and_data.second.path, path_);

  scoped_file_access_.reset();
  if (result_and_data.first != BinaryUploadService::Result::SUCCESS) {
    CacheResultAndData(result_and_data.first,
                       std::move(result_and_data.second));
    RunCallback();
    return;
  }

  const std::string& mime_type = cached_data_.mime_type.empty()
                                     ? result_and_data.second.mime_type
                                     : cached_data_.mime_type;
  base::FilePath::StringType ext(file_name_.FinalExtension());
  base::ranges::transform(ext, ext.begin(), tolower);
  if (IsZipFile(ext, mime_type)) {
    zip_analyzer_ = SandboxedZipAnalyzer::CreateAnalyzer(
        path_,
        /*password=*/password(),
        base::BindOnce(&FileAnalysisRequest::OnCheckedForEncryption,
                       weakptr_factory_.GetWeakPtr(),
                       std::move(result_and_data.second)),
        LaunchFileUtilService());
    zip_analyzer_->Start();
  } else if (IsRarFile(ext, mime_type)) {
    rar_analyzer_ = SandboxedRarAnalyzer::CreateAnalyzer(
        path_,
        /*password=*/password(),
        base::BindOnce(&FileAnalysisRequest::OnCheckedForEncryption,
                       weakptr_factory_.GetWeakPtr(),
                       std::move(result_and_data.second)),
        LaunchFileUtilService());
    rar_analyzer_->Start();
  } else {
    CacheResultAndData(BinaryUploadService::Result::SUCCESS,
                       std::move(result_and_data.second));
    RunCallback();
  }
}

void FileAnalysisRequest::OnCheckedForEncryption(
    Data data,
    const ArchiveAnalyzerResults& analyzer_result) {
  bool encrypted = analyzer_result.encryption_info.is_encrypted &&
                   analyzer_result.encryption_info.password_status ==
                       EncryptionInfo::kKnownIncorrect;

  BinaryUploadService::Result result =
      encrypted ? BinaryUploadService::Result::FILE_ENCRYPTED
                : BinaryUploadService::Result::SUCCESS;
  CacheResultAndData(result, std::move(data));
  RunCallback();
}

void FileAnalysisRequest::CacheResultAndData(BinaryUploadService::Result result,
                                             Data data) {
  has_cached_result_ = true;
  cached_result_ = result;

  // If the mime type is already set, it shouldn't be overwritten.
  if (!cached_data_.mime_type.empty())
    data.mime_type = std::move(cached_data_.mime_type);

  DCHECK(!data.path.empty());
  cached_data_ = std::move(data);

  set_digest(cached_data_.hash);
  set_content_type(cached_data_.mime_type);
}

void FileAnalysisRequest::RunCallback() {
  if (!data_callback_.is_null()) {
    std::move(data_callback_).Run(cached_result_, cached_data_);
  }
}

void FileAnalysisRequest::GetData(file_access::ScopedFileAccess file_access) {
  scoped_file_access_ =
      std::make_unique<file_access::ScopedFileAccess>(std::move(file_access));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&GetFileDataBlocking, path_,
                     cached_data_.mime_type.empty(), is_obfuscated_),
      base::BindOnce(&FileAnalysisRequest::OnGotFileData,
                     weakptr_factory_.GetWeakPtr()));
}

}  // namespace safe_browsing
