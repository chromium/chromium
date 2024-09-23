// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer_win.h"

#include <stddef.h>

#include <array>
#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

std::vector<base::FilePath> GetCriticalBinariesPath() {
  static constexpr auto kUnversionedFiles = std::to_array({
      FILE_PATH_LITERAL("chrome.exe"),
  });
  static constexpr auto kVersionedFiles = std::to_array({
      FILE_PATH_LITERAL("chrome.dll"),
      FILE_PATH_LITERAL("chrome_child.dll"),
      FILE_PATH_LITERAL("chrome_elf.dll"),
  });

  // Find where chrome.exe is installed.
  base::FilePath chrome_exe_dir;
  if (!base::PathService::Get(base::DIR_EXE, &chrome_exe_dir))
    NOTREACHED_IN_MIGRATION();

  std::vector<base::FilePath> critical_binaries;
  critical_binaries.reserve(std::size(kUnversionedFiles) +
                            std::size(kVersionedFiles));

  for (size_t i = 0; i < std::size(kUnversionedFiles); ++i) {
    critical_binaries.push_back(chrome_exe_dir.Append(kUnversionedFiles[i]));
  }

  base::FilePath version_dir(
      chrome_exe_dir.AppendASCII(CHROME_VERSION_STRING));
  for (size_t i = 0; i < std::size(kVersionedFiles); ++i) {
    critical_binaries.push_back(version_dir.Append(kVersionedFiles[i]));
  }

  return critical_binaries;
}

void VerifyBinaryIntegrity(
    std::unique_ptr<IncidentReceiver> incident_receiver) {
  scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor(
      new BinaryFeatureExtractor());

  std::vector<base::FilePath> critical_binaries = GetCriticalBinariesPath();
  for (size_t i = 0; i < critical_binaries.size(); ++i) {
    base::FilePath binary_path(critical_binaries[i]);
    if (!base::PathExists(binary_path))
      continue;

    std::unique_ptr<ClientDownloadRequest_SignatureInfo> signature_info(
        new ClientDownloadRequest_SignatureInfo());

    binary_feature_extractor->CheckSignature(binary_path, signature_info.get());

    // Only create a report if the signature is untrusted.
    if (!signature_info->trusted()) {
      std::unique_ptr<ClientIncidentReport_IncidentData_BinaryIntegrityIncident>
          incident(
              new ClientIncidentReport_IncidentData_BinaryIntegrityIncident());

      incident->set_file_basename(binary_path.BaseName().AsUTF8Unsafe());
      incident->set_allocated_signature(signature_info.release());

      // Send the report.
      incident_receiver->AddIncidentForProcess(
          std::make_unique<BinaryIntegrityIncident>(std::move(incident)));
    } else {
      // The binary is integral, remove previous report so that next incidents
      // for the binary will be reported.
      ClearBinaryIntegrityForFile(incident_receiver.get(),
                                  binary_path.BaseName().AsUTF8Unsafe());
    }
  }
}

}  // namespace safe_browsing
