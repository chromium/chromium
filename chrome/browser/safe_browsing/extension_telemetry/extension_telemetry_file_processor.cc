// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_file_processor.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/sha2.h"

namespace safe_browsing {

namespace {
// Max number of files to read per extension.
constexpr int64_t kMaxFilesToRead = 1000;

const base::FilePath::CharType kManifestFilePath[] =
    FILE_PATH_LITERAL("manifest.json");
const base::FilePath::CharType kJSFileSuffix[] = FILE_PATH_LITERAL(".js");
const base::FilePath::CharType kHTMLFileSuffix[] = FILE_PATH_LITERAL(".html");
const base::FilePath::CharType kCSSFileSuffix[] = FILE_PATH_LITERAL(".css");

const base::flat_map<base::FilePath::StringType, int> kFileTypePriorityMap = {
    {kJSFileSuffix, 3},
    {kHTMLFileSuffix, 2},
    {kCSSFileSuffix, 1}};

void RecordLargestFileSizeObserved(size_t size) {
  base::UmaHistogramCounts1M(
      "SafeBrowsing.ExtensionTelemetry.FileData.LargestFileSizeObserved", size);
}

void RecordNumFilesFound(int count) {
  base::UmaHistogramCounts1000(
      "SafeBrowsing.ExtensionTelemetry.FileData.NumFilesFound", count);
}

void RecordNumFilesOverSizeLimit(int count) {
  base::UmaHistogramCounts1000(
      "SafeBrowsing.ExtensionTelemetry.FileData.NumFilesOverSizeLimit", count);
}

void RecordNumFilesProcessed(int count) {
  base::UmaHistogramCounts1000(
      "SafeBrowsing.ExtensionTelemetry.FileData.NumFilesProcessed", count);
}

void RecordProcessedFileSize(size_t size) {
  base::UmaHistogramCounts1M(
      "SafeBrowsing.ExtensionTelemetry.FileData.ProcessedFileSize", size);
}
}  // namespace

struct ExtensionTelemetryFileProcessor::FileExtensionsComparator {
  bool operator()(const base::FilePath& a, const base::FilePath& b) const {
    return kFileTypePriorityMap.at(a.Extension()) >=
           kFileTypePriorityMap.at(b.Extension());
  }
};

ExtensionTelemetryFileProcessor::~ExtensionTelemetryFileProcessor() = default;

ExtensionTelemetryFileProcessor::ExtensionTelemetryFileProcessor(
    const uint32_t max_files_to_process,
    const int64_t max_file_size,
    base::FilePath extension_root_dir)
    : max_files_to_process_(max_files_to_process),
      max_file_size_(max_file_size),
      max_files_to_read_(kMaxFilesToRead),
      extension_root_dir_(std::move(extension_root_dir)) {}

base::Value::Dict ExtensionTelemetryFileProcessor::ProcessExtension() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Gather all installed extension files, filter and sort by types.
  SortedFilePaths installed_files = RetrieveFilePaths();

  // Compute hashes of files until |max_files_to_process_| limit is reached.
  base::Value::Dict extension_data = ComputeHashes(std::move(installed_files));

  // Add Manifest.json file data, unhashed.
  base::FilePath manifest_path = extension_root_dir_.Append(kManifestFilePath);
  std::string manifest_contents;

  if (base::ReadFileToString(manifest_path, &manifest_contents) &&
      !manifest_contents.empty()) {
    extension_data.Set(manifest_path.BaseName().AsUTF8Unsafe(),
                       std::move(manifest_contents));
  }

  RecordNumFilesProcessed(extension_data.size());
  return extension_data;
}

ExtensionTelemetryFileProcessor::SortedFilePaths
ExtensionTelemetryFileProcessor::RetrieveFilePaths() {
  base::FileEnumerator enumerator(extension_root_dir_, /*recursive=*/true,
                                  base::FileEnumerator::FILES);
  int64_t exceeded_file_size_counter = 0;
  int64_t largest_file_size = 0;
  SortedFilePaths sorted_file_paths;

  // Find all file paths within extension directory and add them to a list.
  for (int read_counter = 0; read_counter < max_files_to_read_;
       read_counter++) {
    base::FilePath full_path = enumerator.Next();
    if (full_path.empty()) {
      break;
    }

    int64_t file_size;
    // Skip invalid, empty, and non-applicable type files
    if (!base::GetFileSize(full_path, &file_size) || file_size <= 0 ||
        !IsApplicableType(full_path)) {
      continue;
    }

    // Record largest file size observed.
    largest_file_size = std::max(largest_file_size, file_size);

    // Add file for processing if within size limit, otherwise, skip and record.
    if (file_size <= max_file_size_) {
      sorted_file_paths.insert(std::move(full_path));
    } else {
      exceeded_file_size_counter++;
    }
  }

  RecordLargestFileSizeObserved(largest_file_size);
  RecordNumFilesOverSizeLimit(exceeded_file_size_counter);
  RecordNumFilesFound(sorted_file_paths.size());
  return sorted_file_paths;
}

base::Value::Dict ExtensionTelemetryFileProcessor::ComputeHashes(
    const SortedFilePaths& file_paths) {
  base::Value::Dict extension_data;

  for (const auto& full_path : file_paths) {
    std::string file_contents;

    if (extension_data.size() < max_files_to_process_ &&
        base::ReadFileToString(full_path, &file_contents) &&
        !file_contents.empty()) {
      // Use relative path as key since file names can repeat.
      base::FilePath relative_path;
      extension_root_dir_.AppendRelativePath(full_path, &relative_path);

      std::string hash = crypto::SHA256HashString(file_contents);
      std::string hex_encode = base::HexEncode(hash.c_str(), hash.size());

      extension_data.Set(
          relative_path.NormalizePathSeparatorsTo('/').AsUTF8Unsafe(),
          std::move(hex_encode));

      RecordProcessedFileSize(file_contents.size());
    }
  }

  return extension_data;
}

bool ExtensionTelemetryFileProcessor::IsApplicableType(
    const base::FilePath& file_path) {
  return file_path.MatchesExtension(kJSFileSuffix) ||
         file_path.MatchesExtension(kHTMLFileSuffix) ||
         file_path.MatchesExtension(kCSSFileSuffix);
}

void ExtensionTelemetryFileProcessor::SetMaxFilesToReadForTest(
    int64_t max_files_to_read) {
  max_files_to_read_ = max_files_to_read;
}

}  // namespace safe_browsing
