// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_FILE_ANALYZER_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_FILE_ANALYZER_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"
#include "chrome/services/file_util/public/cpp/sandboxed_seven_zip_analyzer.h"
#include "chrome/services/file_util/public/cpp/sandboxed_zip_analyzer.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/common/safe_browsing/disk_image_type_sniffer_mac.h"
#include "chrome/services/file_util/public/cpp/sandboxed_dmg_analyzer_mac.h"
#endif

namespace safe_browsing {

// This class does the file content analysis for a user download, extracting the
// features that will be sent to the SB backend. This class lives on the UI
// thread, which is where the result callback will be invoked.
class FileAnalyzer {
 public:
  enum class ArchiveValid { UNSET, VALID, INVALID };

  // This struct holds the possible features extracted from a file.
  struct Results {
    Results();
    Results(const Results& other);
    ~Results();

    // What type of inspection was performed to yield the results here.
    DownloadFileType::InspectionType inspection_performed =
        DownloadFileType::NONE;

    // When analyzing a ZIP or RAR, the type becomes clarified by content
    // inspection (does it contain binaries/archives?). So we return a type.
    ClientDownloadRequest::DownloadType type;

    // For archive files, whether the archive contains an executable. Has
    // unspecified contents for non-archive files.
    bool archived_executable = false;

    // For archive files, whether the archive contains an archive. Has
    // unspecified contents for non-archive files.
    bool archived_archive = false;

    // For archive files, the features extracted from each contained
    // archive/binary.
    google::protobuf::RepeatedPtrField<ClientDownloadRequest::ArchivedBinary>
        archived_binaries;

    // For executables, information about the signature of the executable.
    ClientDownloadRequest::SignatureInfo signature_info;

    // For executables, information about the file headers.
    ClientDownloadRequest::ImageHeaders image_headers;

#if BUILDFLAG(IS_MAC)
    // For DMG files, the signature of the DMG.
    std::vector<uint8_t> disk_image_signature;

    // For DMG files, any detached code signatures in the DMG.
    google::protobuf::RepeatedPtrField<
        ClientDownloadRequest::DetachedCodeSignature>
        detached_code_signatures;
#endif

    // For archives, the features and metadata extracted from the file.
    ClientDownloadRequest::ArchiveSummary archive_summary;

    // Information about the encryption on this file.
    EncryptionInfo encryption_info;
  };

  explicit FileAnalyzer(
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor);
  ~FileAnalyzer();
  void Start(const base::FilePath& target_file_name,
             const base::FilePath& tmp_path,
             base::optional_ref<const std::string> password,
             base::OnceCallback<void(Results)> callback);

 private:
  void StartExtractFileFeatures();
  void OnFileAnalysisFinished(FileAnalyzer::Results results);

#if !BUILDFLAG(IS_ANDROID)
  void StartExtractZipFeatures();
  void OnZipAnalysisFinished(const ArchiveAnalyzerResults& archive_results);

  void StartExtractRarFeatures();
  void OnRarAnalysisFinished(const ArchiveAnalyzerResults& archive_results);
#endif

#if BUILDFLAG(IS_MAC)
  void StartExtractDmgFeatures();
  void ExtractFileOrDmgFeatures(bool download_file_has_koly_signature);
  void OnDmgAnalysisFinished(
      const safe_browsing::ArchiveAnalyzerResults& archive_results);
#endif

#if !BUILDFLAG(IS_ANDROID)
  void StartExtractSevenZipFeatures();
  void OnSevenZipAnalysisFinished(
      const ArchiveAnalyzerResults& archive_results);
#endif

  void LogAnalysisDurationWithAndWithoutSuffix(const std::string& suffix);

  // The ultimate destination/filename for the download. This is used to
  // determine the filetype from the filename extension/suffix, and should be a
  // human-readable filename (i.e. not a content-URI, on Android).
  base::FilePath target_file_name_;

  // The current path to the file contents.
  base::FilePath tmp_path_;

  std::optional<std::string> password_;
  scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor_;
  base::OnceCallback<void(Results)> callback_;
  base::Time start_time_;
  Results results_;

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<SandboxedZipAnalyzer, base::OnTaskRunnerDeleter>
      zip_analyzer_{nullptr, base::OnTaskRunnerDeleter(nullptr)};

  std::unique_ptr<SandboxedRarAnalyzer, base::OnTaskRunnerDeleter>
      rar_analyzer_{nullptr, base::OnTaskRunnerDeleter(nullptr)};
#endif

#if BUILDFLAG(IS_MAC)
  std::unique_ptr<SandboxedDMGAnalyzer, base::OnTaskRunnerDeleter>
      dmg_analyzer_{nullptr, base::OnTaskRunnerDeleter(nullptr)};
#endif

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<SandboxedSevenZipAnalyzer, base::OnTaskRunnerDeleter>
      seven_zip_analyzer_{nullptr, base::OnTaskRunnerDeleter(nullptr)};
#endif

  base::WeakPtrFactory<FileAnalyzer> weakptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_FILE_ANALYZER_H_
