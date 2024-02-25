// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_FILE_PROCESSOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_FILE_PROCESSOR_H_

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"

namespace base {
class FilePath;
}  // namespace base

namespace safe_browsing {

// Given an extension root path, the FileProcessor does the following:
//   - Computes and retrieves the installed file hashes
//   - Retrieves the manifest.json file contents
// The FileProcessor is owned and instantiated by ExtensionTelemetryService when
// it is enabled and destroyed when it is disabled. This object lives on a
// different sequence from the ExtensionTelemetryService because it performs
// blocking file read operations.
class ExtensionTelemetryFileProcessor {
 public:
  ExtensionTelemetryFileProcessor();

  ExtensionTelemetryFileProcessor(const ExtensionTelemetryFileProcessor&) =
      delete;
  ExtensionTelemetryFileProcessor& operator=(
      const ExtensionTelemetryFileProcessor&) = delete;

  virtual ~ExtensionTelemetryFileProcessor();

  // Selects and processes installed extension files from the given root
  // directory. Returns files data in a Dict:
  // <file path 1, file hash 1>
  // ...
  // <file path N, file hash N>
  // <manifest.json, file contents>
  // Each file path is relative starting from the extension root. Manifest.json
  // file is unhashed.
  base::Value::Dict ProcessExtension(const base::FilePath& root_dir);

  void SetMaxFilesToProcessForTest(int64_t max_files_to_process);
  void SetMaxFileSizeBytesForTest(int64_t max_file_size);
  void SetMaxFilesToReadForTest(int64_t max_files_to_read);

 protected:
  struct FileExtensionsComparator;

  using SortedFilePaths =
      base::flat_set<base::FilePath, FileExtensionsComparator>;

  // TODO(richche): Process other types of files until |max_files_to_process_|
  // limit is reached.
  // Retrieves installed extension files to process. Applicable files:
  //   - Only JavaScript, HTML, CSS files and sorted in that file type
  //   order
  //   - Ignore empty files
  //   - Ignore files over max_file_size_ limit
  SortedFilePaths RetrieveFilePaths(const base::FilePath& root_dir);

  // Hashes the given list of extension files and returns a Dict of <relative
  // file path, file hash> until |max_files_to_process_| is reached.
  base::Value::Dict ComputeHashes(const base::FilePath& root_dir,
                                  const SortedFilePaths& file_paths);

  // Returns true if a file has JS, HTML, or CSS extension.
  bool IsApplicableType(const base::FilePath& file_path);

  // Max number of files processed per extension.
  size_t max_files_to_process_;

  // Max file size limit for extension files in bytes.
  int64_t max_file_size_;

  // Max number of files read limit enforced in case an extension has too many
  // files.
  int64_t max_files_to_read_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ExtensionTelemetryFileProcessor> weak_factory_{this};
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_FILE_PROCESSOR_H_
