// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_PERSISTER_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_PERSISTER_H_

#include <memory>

#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"

namespace base {
class FilePath;
}
namespace safe_browsing {

// The `ExtensionTelemetryPersister` stores data collected by the Extension
// Telemetry Service to disk. It creates files until `kMaxNumFiles` are on disk
// and then starts overwriting previously made files. When a file is read it is
// also deleted.
class ExtensionTelemetryPersister {
 public:
  ExtensionTelemetryPersister();

  ExtensionTelemetryPersister(const ExtensionTelemetryPersister&) = delete;
  ExtensionTelemetryPersister& operator=(const ExtensionTelemetryPersister&) =
      delete;

  virtual ~ExtensionTelemetryPersister();

  // Determines if there are any existing telemetry reports on disk and
  // initializes `read_index_` and `write_index_` for read and write operations.
  void PersisterInit();

  // Writes a telemetry report to a file on disk. The filename written is not
  // exposed to the caller and is determined by `write_index_`.
  void WriteReport(const std::string write_string);

  // Caller should use this method exactly once to write a telemetry report to
  // disk during Chrome/Profile shutdown. The persister object should not be
  // used after calling this method and should be destroyed.
  void WriteReportDuringShutdown(const std::string write_string);

  // Reads a telemetry report from a file on disk. The file is deleted
  // regardless of if the read was successful or not. The filename
  // is not exposed to the caller. The callback passes back the result
  // of the read operation and the contents of the report if the read succeeded.
  // The callback is expected to be bound to the thread it needs to run on.
  void ReadReport(base::OnceCallback<void(std::string, bool)> callback);

  // Deletes the CRXTelemetry folder by calling DeleteAllFiles.
  void ClearPersistedFiles();

 private:
  // A helper function of PersisterInit allowing it post actions to the
  // thread pool.
  void InitHelper();

  // Writes data to the file represented by `write_index_`.
  void SaveFile(std::string write_string);

  // Writes data during a profile or Chrome shutdown. Persister
  // tasks run on the threadpool but it's destructor runs on the
  // main UI thread. This function is static to prevent threading
  // errors when the persister's destructor and posted task execute
  // at the same time but on different threads.
  static void SaveFileDuringShutdown(std::string write_string,
                                     base::FilePath dir_path,
                                     int write_index);

  // Reads data from the file represented by `read_index_`.
  void LoadFile(base::OnceCallback<void(std::string, bool)> callback);

  // Deletes the file that the `path` variable points to.
  // Returns true if the file is deleted, false otherwise.
  bool DeleteFile(const base::FilePath path);

  // Deletes the CRXTelemetry directory.
  void DeleteAllFiles();

  friend class ExtensionTelemetryPersisterTest;

  // Stores the directory path.
  base::FilePath dir_path_;

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

  // Ensures once the persister has run it's shutdown write function
  // the persister will not post any other tasks.
  bool is_shut_down_ = false;

  // Task runner for read and write operations.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<ExtensionTelemetryPersister> weak_factory_{this};
};
}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_PERSISTER_H_
