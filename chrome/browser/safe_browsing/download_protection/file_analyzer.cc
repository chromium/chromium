// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/file_analyzer.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "components/safe_browsing/content/common/file_type_policies.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

using content::BrowserThread;

FileAnalyzer::Results ExtractFileFeatures(
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor,
    base::FilePath file_path) {
  FileAnalyzer::Results results;
  binary_feature_extractor->CheckSignature(file_path, &results.signature_info);

  if (!binary_feature_extractor->ExtractImageFeatures(
          file_path, BinaryFeatureExtractor::kDefaultOptions,
          &results.image_headers, nullptr)) {
    results.image_headers = ClientDownloadRequest::ImageHeaders();
  }

  return results;
}

}  // namespace

FileAnalyzer::Results::Results() = default;
FileAnalyzer::Results::~Results() {}
FileAnalyzer::Results::Results(const FileAnalyzer::Results& other) = default;

FileAnalyzer::FileAnalyzer(
    scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor)
    : binary_feature_extractor_(binary_feature_extractor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

FileAnalyzer::~FileAnalyzer() {}

void FileAnalyzer::Start(const base::FilePath& target_path,
                         const base::FilePath& tmp_path,
                         base::optional_ref<const std::string> password,
                         base::OnceCallback<void(Results)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  target_path_ = target_path;
  tmp_path_ = tmp_path;
  password_ = password.CopyAsOptional();
  callback_ = std::move(callback);
  start_time_ = base::Time::Now();

  DownloadFileType::InspectionType inspection_type =
      FileTypePolicies::GetInstance()
          ->PolicyForFile(target_path_, GURL{}, nullptr)
          .inspection_type();

  if (inspection_type == DownloadFileType::ZIP) {
    StartExtractZipFeatures();
  } else if (inspection_type == DownloadFileType::RAR) {
    StartExtractRarFeatures();
#if BUILDFLAG(IS_MAC)
  } else if (inspection_type == DownloadFileType::DMG) {
    StartExtractDmgFeatures();
#endif
  } else if (inspection_type == DownloadFileType::SEVEN_ZIP) {
    StartExtractSevenZipFeatures();
  } else {
#if BUILDFLAG(IS_MAC)
    // Checks for existence of "koly" signature even if file doesn't have
    // archive-type extension, then calls ExtractFileOrDmgFeatures() with
    // result.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(DiskImageTypeSnifferMac::IsAppleDiskImage, tmp_path_),
        base::BindOnce(&FileAnalyzer::ExtractFileOrDmgFeatures,
                       weakptr_factory_.GetWeakPtr()));
#else
    StartExtractFileFeatures();
#endif
  }
}

void FileAnalyzer::StartExtractFileFeatures() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ExtractFileFeatures, binary_feature_extractor_,
                     tmp_path_),
      base::BindOnce(&FileAnalyzer::OnFileAnalysisFinished,
                     weakptr_factory_.GetWeakPtr()));
}

void FileAnalyzer::OnFileAnalysisFinished(FileAnalyzer::Results results) {
  LogAnalysisDurationWithAndWithoutSuffix("Executable");
  results.type = download_type_util::GetDownloadType(target_path_);
  std::move(callback_).Run(results);
}

void FileAnalyzer::StartExtractZipFeatures() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We give the zip analyzer a weak pointer to this object.
  zip_analyzer_ = SandboxedZipAnalyzer::CreateAnalyzer(
      tmp_path_, password_,
      base::BindOnce(&FileAnalyzer::OnZipAnalysisFinished,
                     weakptr_factory_.GetWeakPtr()),
      LaunchFileUtilService());
  zip_analyzer_->Start();
}

void FileAnalyzer::OnZipAnalysisFinished(
    const ArchiveAnalyzerResults& archive_results) {
  base::UmaHistogramEnumeration("SBClientDownload.ZipArchiveAnalysisResult",
                                archive_results.analysis_result);
  LogAnalysisDurationWithAndWithoutSuffix("Zip");
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Even if !results.success, some of the zip may have been parsed.
  // Some unzippers will successfully unpack archives that we cannot,
  // so we're lenient here.
  if (archive_results.success) {
    results_.archive_summary.set_parser_status(
        ClientDownloadRequest::ArchiveSummary::VALID);
  } else if (archive_results.analysis_result ==
             ArchiveAnalysisResult::kTimeout) {
    results_.archive_summary.set_parser_status(
        ClientDownloadRequest::ArchiveSummary::PARSER_TIMED_OUT);
  } else if (archive_results.analysis_result ==
             ArchiveAnalysisResult::kTooLarge) {
    results_.archive_summary.set_parser_status(
        ClientDownloadRequest::ArchiveSummary::TOO_LARGE);
  } else if (archive_results.analysis_result ==
             ArchiveAnalysisResult::kDiskError) {
    results_.archive_summary.set_parser_status(
        ClientDownloadRequest::ArchiveSummary::DISK_ERROR);
  }
  results_.archived_executable = archive_results.has_executable;
  results_.archived_archive = archive_results.has_archive;
  results_.archived_binaries =
      SelectArchiveEntries(archive_results.archived_binary);

  if (archive_results.has_executable) {
    results_.type = ClientDownloadRequest::ZIPPED_EXECUTABLE;
  } else if (archive_results.has_archive) {
    results_.type = ClientDownloadRequest::ZIPPED_ARCHIVE;
  } else if (!archive_results.success) {
    // .zip files that look invalid to Chrome can often be successfully
    // unpacked by other archive tools, so they may be a real threat.
    results_.type = ClientDownloadRequest::INVALID_ZIP;
  } else {
    results_.type = download_type_util::GetDownloadType(target_path_);
  }

  results_.archive_summary.set_file_count(archive_results.file_count);
  results_.archive_summary.set_directory_count(archive_results.directory_count);
  results_.archive_summary.set_is_encrypted(
      archive_results.encryption_info.is_encrypted);
  results_.encryption_info = archive_results.encryption_info;

  std::move(callback_).Run(std::move(results_));
}

void FileAnalyzer::StartExtractRarFeatures() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We give the rar analyzer a weak pointer to this object. Since the
  // analyzer is refcounted, it might outlive the request.
  rar_analyzer_ = SandboxedRarAnalyzer::CreateAnalyzer(
      tmp_path_, password_,
      base::BindOnce(&FileAnalyzer::OnRarAnalysisFinished,
                     weakptr_factory_.GetWeakPtr()),
      LaunchFileUtilService());
  rar_analyzer_->Start();
}

void FileAnalyzer::OnRarAnalysisFinished(
    const ArchiveAnalyzerResults& archive_results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::UmaHistogramEnumeration("SBClientDownload.RarArchiveAnalysisResult",
                                archive_results.analysis_result);
  LogAnalysisDurationWithAndWithoutSuffix("Rar");

  if (archive_results.success) {
    results_.archive_summary.set_parser_status(
        ClientDownloadRequest::ArchiveSummary::VALID);
  } else if (archive_results.analysis_result ==
             ArchiveAnalysisResult::kTimeout) {
    results_.archive_summary.set_parser_status(
        ClientDownloadRequest::ArchiveSummary::PARSER_TIMED_OUT);
  } else if (archive_results.analysis_result ==
             ArchiveAnalysisResult::kTooLarge) {
    results_.archive_summary.set_parser_status(
        ClientDownloadRequest::ArchiveSummary::TOO_LARGE);
  }
  results_.archived_executable = archive_results.has_executable;
  results_.archived_archive = archive_results.has_archive;
  results_.archived_binaries =
      SelectArchiveEntries(archive_results.archived_binary);

  if (archive_results.has_executable) {
    results_.type = ClientDownloadRequest::RAR_COMPRESSED_EXECUTABLE;
  } else if (archive_results.has_archive) {
    results_.type = ClientDownloadRequest::RAR_COMPRESSED_ARCHIVE;
  } else if (!archive_results.success) {
    // .rar files that look invalid to Chrome may be successfully unpacked by
    // other archive tools, so they may be a real threat.
    results_.type = ClientDownloadRequest::INVALID_RAR;
  } else {
    results_.type = download_type_util::GetDownloadType(target_path_);
  }

  results_.archive_summary.set_file_count(archive_results.file_count);
  results_.archive_summary.set_directory_count(archive_results.directory_count);
  results_.archive_summary.set_is_encrypted(
      archive_results.encryption_info.is_encrypted);
  results_.encryption_info = archive_results.encryption_info;

  std::move(callback_).Run(std::move(results_));
}

#if BUILDFLAG(IS_MAC)
// This is called for .DMGs and other files that can be parsed by
// SandboxedDMGAnalyzer.
void FileAnalyzer::StartExtractDmgFeatures() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Directly use 'dmg' extension since download file may not have any
  // extension, but has still been deemed a DMG through file type sniffing.
  dmg_analyzer_ = SandboxedDMGAnalyzer::CreateAnalyzer(
      tmp_path_,
      FileTypePolicies::GetInstance()->GetMaxFileSizeToAnalyze("dmg"),
      base::BindRepeating(&FileAnalyzer::OnDmgAnalysisFinished,
                          weakptr_factory_.GetWeakPtr()),
      LaunchFileUtilService());
  dmg_analyzer_->Start();
}

void FileAnalyzer::ExtractFileOrDmgFeatures(
    bool download_file_has_koly_signature) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (download_file_has_koly_signature) {
    StartExtractDmgFeatures();
  } else {
    StartExtractFileFeatures();
  }
}

void FileAnalyzer::OnDmgAnalysisFinished(
    const safe_browsing::ArchiveAnalyzerResults& archive_results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::UmaHistogramEnumeration("SBClientDownload.DmgArchiveAnalysisResult",
                                archive_results.analysis_result);
  LogAnalysisDurationWithAndWithoutSuffix("Dmg");

  if (archive_results.signature_blob.size() > 0) {
    results_.disk_image_signature =
        std::vector<uint8_t>(archive_results.signature_blob);
  }

  results_.detached_code_signatures.CopyFrom(
      archive_results.detached_code_signatures);

  // Even if !results.success, some of the DMG may have been parsed.
  results_.archived_executable = archive_results.has_executable;
  results_.archived_archive = archive_results.has_archive;
  results_.archived_binaries =
      SelectArchiveEntries(archive_results.archived_binary);

  if (archive_results.success) {
    results_.type = ClientDownloadRequest::MAC_EXECUTABLE;
  } else {
    results_.type = ClientDownloadRequest::MAC_ARCHIVE_FAILED_PARSING;
  }

  if (archive_results.success) {
    results_.archive_summary.set_parser_status(
        ClientDownloadRequest::ArchiveSummary::VALID);
  } else if (archive_results.analysis_result ==
             ArchiveAnalysisResult::kTimeout) {
    results_.archive_summary.set_parser_status(
        ClientDownloadRequest::ArchiveSummary::PARSER_TIMED_OUT);
  } else if (archive_results.analysis_result ==
             ArchiveAnalysisResult::kTooLarge) {
    results_.archive_summary.set_parser_status(
        ClientDownloadRequest::ArchiveSummary::TOO_LARGE);
  }

  results_.archive_summary.set_is_encrypted(
      archive_results.encryption_info.is_encrypted);
  results_.encryption_info = archive_results.encryption_info;

  std::move(callback_).Run(std::move(results_));
}
#endif  // BUILDFLAG(IS_MAC)

void FileAnalyzer::StartExtractSevenZipFeatures() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  seven_zip_analyzer_ = SandboxedSevenZipAnalyzer::CreateAnalyzer(
      tmp_path_,
      base::BindOnce(&FileAnalyzer::OnSevenZipAnalysisFinished,
                     weakptr_factory_.GetWeakPtr()),
      LaunchFileUtilService());
  seven_zip_analyzer_->Start();
}

void FileAnalyzer::OnSevenZipAnalysisFinished(
    const ArchiveAnalyzerResults& archive_results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::UmaHistogramEnumeration(
      "SBClientDownload.SevenZipArchiveAnalysisResult",
      archive_results.analysis_result);
  LogAnalysisDurationWithAndWithoutSuffix("SevenZip");

  // Even if !results.success, some of the 7z may have been parsed.
  // Some unzippers will successfully unpack archives that we cannot,
  // so we're lenient here.
  if (archive_results.success) {
    results_.archive_summary.set_parser_status(
        ClientDownloadRequest::ArchiveSummary::VALID);
  } else if (archive_results.analysis_result ==
             ArchiveAnalysisResult::kTimeout) {
    results_.archive_summary.set_parser_status(
        ClientDownloadRequest::ArchiveSummary::PARSER_TIMED_OUT);
  } else if (archive_results.analysis_result ==
             ArchiveAnalysisResult::kTooLarge) {
    results_.archive_summary.set_parser_status(
        ClientDownloadRequest::ArchiveSummary::TOO_LARGE);
  }
  results_.archived_executable = archive_results.has_executable;
  results_.archived_archive = archive_results.has_archive;
  results_.archived_binaries =
      SelectArchiveEntries(archive_results.archived_binary);

  if (archive_results.has_executable) {
    results_.type = ClientDownloadRequest::SEVEN_ZIP_COMPRESSED_EXECUTABLE;
  } else if (archive_results.has_archive) {
    results_.type = ClientDownloadRequest::SEVEN_ZIP_COMPRESSED_ARCHIVE;
  } else if (!archive_results.success) {
    // .7z files that look invalid to Chrome can sometimes be successfully
    // unpacked by other archive tools, so they may be a real threat.
    results_.type = ClientDownloadRequest::INVALID_SEVEN_ZIP;
  } else {
    results_.type = download_type_util::GetDownloadType(target_path_);
  }

  results_.archive_summary.set_file_count(archive_results.file_count);
  results_.archive_summary.set_directory_count(archive_results.directory_count);
  results_.archive_summary.set_is_encrypted(
      archive_results.encryption_info.is_encrypted);
  results_.encryption_info = archive_results.encryption_info;

  std::move(callback_).Run(std::move(results_));
}

void FileAnalyzer::LogAnalysisDurationWithAndWithoutSuffix(
    const std::string& suffix) {
  base::UmaHistogramMediumTimes("SBClientDownload.FileAnalysisDuration",
                                base::Time::Now() - start_time_);
  base::UmaHistogramMediumTimes(
      "SBClientDownload.FileAnalysisDuration." + suffix,
      base::Time::Now() - start_time_);
}

}  // namespace safe_browsing
