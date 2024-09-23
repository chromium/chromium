// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_PERSISTER_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_PERSISTER_H_

#include <memory>

#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace base {
class FilePath;
}
namespace safe_browsing {

// The `ExtensionTelemetryPersister` stores data collected by the Extension
// Telemetry Service to disk. It creates files until `max_num_files_` are on
// disk and then starts overwriting previously made files. The Persister runs
// read and write functions that are time intensive. To avoid blocking
// important threads, the persister should be created using SequenceBound.
class ExtensionTelemetryPersister {
 public:
  // Indicates when the `WriteReport` function is called. Used to generate
  // histogram suffixes.
  enum class WriteReportTrigger { kAtWriteInterval = 0, kAtShutdown = 1 };

  // The `profile_path` is used to construct where the persister saves it's
  // files. The persister creates a directory under profile_path/CRX_Telemetry
  // and saves files there.
  explicit ExtensionTelemetryPersister(int max_num_files,
                                       base::FilePath profile_path);

  ExtensionTelemetryPersister(const ExtensionTelemetryPersister&) = delete;
  ExtensionTelemetryPersister& operator=(const ExtensionTelemetryPersister&) =
      delete;

  virtual ~ExtensionTelemetryPersister();

  // Determines if there are any existing telemetry reports on disk and
  // initializes `read_index_` and `write_index_` for read and write operations.
  void PersisterInit();

  // Writes a telemetry report to a file on disk. The filename written is not
  // exposed to the caller and is determined by `write_index_`. `trigger`
  // indicates when the caller calls this function, used for logging
  // histograms.
  void WriteReport(const std::string write_string, WriteReportTrigger trigger);

  // Reads a telemetry report from a file on disk. The file is deleted
  // regardless of if the read was successful or not. The filename
  // is not exposed to the caller. Returns a string representation
  // of the read file, or an empty string if the read failed.
  std::string ReadReport();

  // Deletes the CRXTelemetry directory.
  void ClearPersistedFiles();

  // Returns the max size the persister cache can be.
  static int MaxFilesSupported();

 private:
  // Deletes the file that the `path` variable points to.
  // Returns true if the file is deleted, false otherwise.
  bool DeleteFile(const base::FilePath path);

  // Deletes the CRXTelemetry directory.
  friend class ExtensionTelemetryPersisterTest;

  // Stores the directory path where the telemetry files are persisted.
  base::FilePath dir_path_;

  // The maximum number of files that are stored on disk.
  int max_num_files_;

  // The index of which file is next for writing. `write_index_`
  // increments from 0 to `kMaxNumFiles` - 1 and then back around to 0.
  int write_index_;

  // The index of which file is next for reading. `read_index_`
  // increments and stops at `kMaxNumFiles` - 1. Allowing the
  // persister to read from the highest numbered file first.
  int read_index_;

  // Ensures write and read operations are not called before the
  // persister is done initializing.
  bool initialization_complete_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ExtensionTelemetryPersister> weak_factory_{this};
};
}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_PERSISTER_H_
