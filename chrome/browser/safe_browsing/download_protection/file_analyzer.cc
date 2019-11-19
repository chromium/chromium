// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/file_analyzer.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/safe_browsing/features.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

using content::BrowserThread;

void CopyArchivedBinaries(
    const google::protobuf::RepeatedPtrField<
        ClientDownloadRequest::ArchivedBinary>& src_binaries,
    google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>*
        dest_binaries) {
  // Limit the number of entries so we don't clog the backend.
  // We can expand this limit by pushing a new download_file_types update.
  int limit = FileTypePolicies::GetInstance()->GetMaxArchivedBinariesToReport();

  dest_binaries->Clear();
  for (int i = 0; i < limit && i < src_binaries.size(); i++) {
    *dest_binaries->Add() = src_binaries[i];
  }
}

void RecordArchivedArchiveFileExtensionType(const base::FilePath& file) {
  base::UmaHistogramSparse(
      "SBClientDownload.ArchivedArchiveExtensions",
      FileTypePolicies::GetInstance()->UmaValueForFile(file));
}

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
                         base::OnceCallback<void(Results)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  target_path_ = target_path;
  tmp_path_ = tmp_path;
  callback_ = std::move(callback);

  results_.type = download_type_util::GetDownloadType(target_path_);

  DownloadFileType::InspectionType inspection_type =
      FileTypePolicies::GetInstance()
          ->PolicyForFile(target_path_)
          .inspection_type();

  if (inspection_type == DownloadFileType::ZIP) {
    StartExtractZipFeatures();
  } else if (inspection_type == DownloadFileType::RAR) {
    StartExtractRarFeatures();
#if defined(OS_MACOSX)
  } else if (inspection_type == DownloadFileType::DMG) {
    StartExtractDmgFeatures();
#endif
  } else {
#if defined(OS_MACOSX)
    // Checks for existence of "koly" signature even if file doesn't have
    // archive-type extension, then calls ExtractFileOrDmgFeatures() with
    // result.
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskPriority::USER_VISIBLE},
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

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ExtractFileFeatures, binary_feature_extractor_,
                     tmp_path_),
      base::BindOnce(&FileAnalyzer::OnFileAnalysisFinished,
                     weakptr_factory_.GetWeakPtr()));
}

void FileAnalyzer::OnFileAnalysisFinished(FileAnalyzer::Results results) {
  results.type = download_type_util::GetDownloadType(target_path_);
  results.archive_is_valid = ArchiveValid::UNSET;
  std::move(callback_).Run(results);
}

void FileAnalyzer::StartExtractZipFeatures() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  zip_analysis_start_time_ = base::TimeTicks::Now();
  // We give the zip analyzer a weak pointer to this object.  Since the
  // analyzer is refcounted, it might outlive the request.
  zip_analyzer_ = new SandboxedZipAnalyzer(
      tmp_path_,
      base::BindRepeating(&FileAnalyzer::OnZipAnalysisFinished,
                          weakptr_factory_.GetWeakPtr()),
      LaunchFileUtilService());
  zip_analyzer_->Start();
}

void FileAnalyzer::OnZipAnalysisFinished(
    const ArchiveAnalyzerResults& archive_results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Even if !results.success, some of the zip may have been parsed.
  // Some unzippers will successfully unpack archives that we cannot,
  // so we're lenient here.
  results_.archive_is_valid =
      (archive_results.success ? ArchiveValid::VALID : ArchiveValid::INVALID);
  results_.archived_executable = archive_results.has_executable;
  results_.archived_archive = archive_results.has_archive;
  CopyArchivedBinaries(archive_results.archived_binary,
                       &results_.archived_binaries);

  // Log metrics for ZIP analysis
  if (results_.archived_executable) {
    UMA_HISTOGRAM_COUNTS_1M("SBClientDownload.ZipFileArchivedBinariesCount",
                            archive_results.archived_binary.size());
  }
  UMA_HISTOGRAM_BOOLEAN("SBClientDownload.ZipFileSuccess",
                        archive_results.success);
  UMA_HISTOGRAM_BOOLEAN("SBClientDownload.ZipFileHasExecutable",
                        archive_results.has_executable);
  UMA_HISTOGRAM_BOOLEAN(
      "SBClientDownload.ZipFileHasArchiveButNoExecutable",
      archive_results.has_archive && !archive_results.has_executable);
  UMA_HISTOGRAM_MEDIUM_TIMES("SBClientDownload.ExtractZipFeaturesTimeMedium",
                             base::TimeTicks::Now() - zip_analysis_start_time_);
  for (const auto& file_name : archive_results.archived_archive_filenames)
    RecordArchivedArchiveFileExtensionType(file_name);

  if (!results_.archived_executable) {
    if (archive_results.has_archive) {
      results_.type = ClientDownloadRequest::ZIPPED_ARCHIVE;
    } else if (!archive_results.success) {
      // .zip files that look invalid to Chrome can often be successfully
      // unpacked by other archive tools, so they may be a real threat.
      results_.type = ClientDownloadRequest::INVALID_ZIP;
    }
  } else {
    results_.type = ClientDownloadRequest::ZIPPED_EXECUTABLE;
  }

  results_.file_count = archive_results.file_count;
  results_.directory_count = archive_results.directory_count;

  std::move(callback_).Run(std::move(results_));
}

void FileAnalyzer::StartExtractRarFeatures() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  rar_analysis_start_time_ = base::TimeTicks::Now();
  // We give the rar analyzer a weak pointer to this object.  Since the
  // analyzer is refcounted, it might outlive the request.
  rar_analyzer_ = new SandboxedRarAnalyzer(
      tmp_path_,
      base::BindRepeating(&FileAnalyzer::OnRarAnalysisFinished,
                          weakptr_factory_.GetWeakPtr()),
      LaunchFileUtilService());
  rar_analyzer_->Start();
}

void FileAnalyzer::OnRarAnalysisFinished(
    const ArchiveAnalyzerResults& archive_results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  results_.archive_is_valid =
      (archive_results.success ? ArchiveValid::VALID : ArchiveValid::INVALID);
  results_.archived_executable = archive_results.has_executable;
  results_.archived_archive = archive_results.has_archive;
  CopyArchivedBinaries(archive_results.archived_binary,
                       &results_.archived_binaries);

  // Log metrics for Rar Analysis
  if (results_.archived_executable) {
    UMA_HISTOGRAM_COUNTS_100("SBClientDownload.RarFileArchivedBinariesCount",
                             archive_results.archived_binary.size());
  }
  UMA_HISTOGRAM_BOOLEAN("SBClientDownload.RarFileSuccess",
                        archive_results.success);
  UMA_HISTOGRAM_BOOLEAN("SBClientDownload.RarFileHasExecutable",
                        results_.archived_executable);
  UMA_HISTOGRAM_BOOLEAN(
      "SBClientDownload.RarFileHasArchiveButNoExecutable",
      archive_results.has_archive && !archive_results.has_executable);
  UMA_HISTOGRAM_MEDIUM_TIMES("SBClientDownload.ExtractRarFeaturesTimeMedium",
                             base::TimeTicks::Now() - rar_analysis_start_time_);
  for (const auto& file_name : archive_results.archived_archive_filenames)
    RecordArchivedArchiveFileExtensionType(file_name);

  if (!results_.archived_executable) {
    if (archive_results.has_archive) {
      results_.type = ClientDownloadRequest::RAR_COMPRESSED_ARCHIVE;
    } else if (!archive_results.success) {
      // .rar files that look invalid to Chrome may be successfully unpacked by
      // other archive tools, so they may be a real threat.
      results_.type = ClientDownloadRequest::INVALID_RAR;
    }
  } else {
    results_.type = ClientDownloadRequest::RAR_COMPRESSED_EXECUTABLE;
  }

  results_.file_count = archive_results.file_count;
  results_.directory_count = archive_results.directory_count;

  std::move(callback_).Run(std::move(results_));
}

#if defined(OS_MACOSX)
// This is called for .DMGs and other files that can be parsed by
// SandboxedDMGAnalyzer.
void FileAnalyzer::StartExtractDmgFeatures() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Directly use 'dmg' extension since download file may not have any
  // extension, but has still been deemed a DMG through file type sniffing.
  dmg_analyzer_ = new SandboxedDMGAnalyzer(
      tmp_path_,
      FileTypePolicies::GetInstance()->GetMaxFileSizeToAnalyze("dmg"),
      base::BindRepeating(&FileAnalyzer::OnDmgAnalysisFinished,
                          weakptr_factory_.GetWeakPtr()),
      LaunchFileUtilService());
  dmg_analyzer_->Start();
  dmg_analysis_start_time_ = base::TimeTicks::Now();
}

void FileAnalyzer::ExtractFileOrDmgFeatures(
    bool download_file_has_koly_signature) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (download_file_has_koly_signature)
    StartExtractDmgFeatures();
  else
    StartExtractFileFeatures();
}

void FileAnalyzer::OnDmgAnalysisFinished(
    const safe_browsing::ArchiveAnalyzerResults& archive_results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (archive_results.signature_blob.size() > 0) {
    results_.disk_image_signature =
        std::vector<uint8_t>(archive_results.signature_blob);
  }

  results_.detached_code_signatures.CopyFrom(
      archive_results.detached_code_signatures);

  // Even if !results.success, some of the DMG may have been parsed.
  results_.archive_is_valid =
      (archive_results.success ? ArchiveValid::VALID : ArchiveValid::INVALID);
  results_.archived_executable = archive_results.has_executable;
  results_.archived_archive = archive_results.has_archive;
  CopyArchivedBinaries(archive_results.archived_binary,
                       &results_.archived_binaries);

  // Log metrics for DMG analysis.
  int64_t uma_file_type =
      FileTypePolicies::GetInstance()->UmaValueForFile(target_path_);

  if (archive_results.success) {
    base::UmaHistogramSparse("SBClientDownload.DmgFileSuccessByType",
                             uma_file_type);
    results_.type = ClientDownloadRequest::MAC_EXECUTABLE;
  } else {
    results_.type = ClientDownloadRequest::MAC_ARCHIVE_FAILED_PARSING;
  }

  UMA_HISTOGRAM_MEDIUM_TIMES("SBClientDownload.ExtractDmgFeaturesTimeMedium",
                             base::TimeTicks::Now() - dmg_analysis_start_time_);

  std::move(callback_).Run(std::move(results_));
}
#endif  // defined(OS_MACOSX)

}  // namespace safe_browsing
