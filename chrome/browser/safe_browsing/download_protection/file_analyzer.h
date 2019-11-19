// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_FILE_ANALYZER_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_FILE_ANALYZER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"
#include "chrome/services/file_util/public/cpp/sandboxed_zip_analyzer.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

#if defined(OS_MACOSX)
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

    // When analyzing a ZIP or RAR, the type becomes clarified by content
    // inspection (does it contain binaries/archives?). So we return a type.
    ClientDownloadRequest::DownloadType type;

    // For archive files, whether the archive is valid. Has unspecified contents
    // for non-archive files.
    ArchiveValid archive_is_valid;

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

#if defined(OS_MACOSX)
    // For DMG files, the signature of the DMG.
    std::vector<uint8_t> disk_image_signature;

    // For DMG files, any detached code signatures in the DMG.
    google::protobuf::RepeatedPtrField<
        ClientDownloadRequest::DetachedCodeSignature>
        detached_code_signatures;
#endif

    // For archive files, the number of contained files.
    int file_count = 0;

    // For archive files, the number of contained directories.
    int directory_count = 0;
  };

  explicit FileAnalyzer(
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor);
  ~FileAnalyzer();
  void Start(const base::FilePath& target_path,
             const base::FilePath& tmp_path,
             base::OnceCallback<void(Results)> callback);

 private:
  void StartExtractFileFeatures();
  void OnFileAnalysisFinished(FileAnalyzer::Results results);

  void StartExtractZipFeatures();
  void OnZipAnalysisFinished(const ArchiveAnalyzerResults& archive_results);

  void StartExtractRarFeatures();
  void OnRarAnalysisFinished(const ArchiveAnalyzerResults& archive_results);

#if defined(OS_MACOSX)
  void StartExtractDmgFeatures();
  void ExtractFileOrDmgFeatures(bool download_file_has_koly_signature);
  void OnDmgAnalysisFinished(
      const safe_browsing::ArchiveAnalyzerResults& archive_results);
#endif

  base::FilePath target_path_;
  base::FilePath tmp_path_;
  scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor_;
  base::OnceCallback<void(Results)> callback_;
  Results results_;

  scoped_refptr<SandboxedZipAnalyzer> zip_analyzer_;
  base::TimeTicks zip_analysis_start_time_;

  scoped_refptr<SandboxedRarAnalyzer> rar_analyzer_;
  base::TimeTicks rar_analysis_start_time_;

#if defined(OS_MACOSX)
  scoped_refptr<SandboxedDMGAnalyzer> dmg_analyzer_;
  base::TimeTicks dmg_analysis_start_time_;
#endif

  base::WeakPtrFactory<FileAnalyzer> weakptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_FILE_ANALYZER_H_
