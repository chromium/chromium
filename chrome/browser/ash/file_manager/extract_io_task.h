// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_EXTRACT_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_EXTRACT_IO_TASK_H_

#include <optional>
#include <vector>

#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chromeos/ash/components/file_manager/speedometer.h"
#include "components/file_access/scoped_file_access.h"
#include "components/services/unzip/public/cpp/unzip.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

namespace file_manager::io_task {

// Histogram name for FileBrowser.ExtractTask.
inline constexpr char kExtractTaskStatusHistogramName[] =
    "FileBrowser.ExtractTask.Status";

// Extract archive status. These values are persisted to logs. Entries should
// not be renumbered and numeric values should never be reused.
// See enum FileManagerExtractStatus in enums.xml.
enum class ExtractStatus {
  kSuccess = 0,
  kUnknownError = 1,
  kCancelled = 2,
  kInsufficientDiskSpace = 3,
  kPasswordError = 4,
  kAesEncrypted = 5,
  kMaxValue = kAesEncrypted,
};

class ExtractIOTask : public IOTask {
 public:
  // Create a task to extract any ZIP files in |source_urls|. These
  // must be under the |parent_folder| directory, and the resulting extraction
  // will be created there.
  ExtractIOTask(std::vector<storage::FileSystemURL> source_urls,
                std::string password,
                storage::FileSystemURL parent_folder,
                Profile* profile,
                scoped_refptr<storage::FileSystemContext> file_system_context,
                bool show_notification = true);
  ~ExtractIOTask() override;

  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;

  // Cancels ongoing unzips. Must be called on the same sequence as
  // ExtractIntoNewDirectory and FinishedExtraction.
  void Cancel() override;

 private:
  void Complete();

  void FinishedExtraction(base::FilePath directory, bool success);

  void ZipExtractCallback(base::FilePath destination_directory, bool success);

  void ZipListenerCallback(uint64_t bytes);

  void ExtractIntoNewDirectory(base::FilePath destination_directory,
                               base::FilePath source_file,
                               bool created_ok);

  void ExtractArchive(
      size_t index,
      base::FileErrorOr<storage::FileSystemURL> destination_result);

  void ExtractAllSources();

  void ZipInfoCallback(unzip::mojom::InfoPtr info);

  void GetExtractedSize(base::FilePath source_file);

  void GotFreeDiskSpace(int64_t free_space);

  void CheckSizeThenExtract();

  // Stores the file access object and begins the actual extraction. This object
  // needs to survive for the all extraction time, otherwise when Data Leak
  // Prevention features are enabled, managed archives cannot be opened.
  void GotScopedFileAccess(file_access::ScopedFileAccess file_access);

  // Retrieves a scoped file access object for the zip files under examination
  // and calls `GotScopedFileAccess`. This is required to open the zip files
  // when Data Leak Prevention features are enabled. When these features are not
  // enabled, `GotScopedFileAccess` is directly called with a default
  // always-allow file access object.
  void GetScopedFileAccess();

  // URLs of the files that have archives in them for extraction.
  const std::vector<storage::FileSystemURL> source_urls_;

  // Password for decrypting encrypted source_urls_ (one only).
  const std::string password_;

  // Parent folder of the files in 'source_urls_'.
  const storage::FileSystemURL parent_folder_;

  // Raw pointer not owned by this.
  raw_ptr<Profile> profile_;
  const scoped_refptr<storage::FileSystemContext> file_system_context_;

  // Speedometer used to calculate the remaining time to finish the operation.
  Speedometer speedometer_;

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  // Counter of the number of archives needing extracted size retrieved.
  size_t sizingCount_;

  // Boolean set to true if we find archives that are encrypted.
  bool have_encrypted_content_ = false;

  // Boolean set to true if the encryption scheme is AES.
  bool uses_aes_encryption_ = false;

  // Boolean set to true if any archive extraction fails.
  bool any_archive_failed_ = false;

  // Counter of the number of archives needing extraction.
  size_t extractCount_;

  // A closure that triggers a chain of cancellation callbacks, cancelling all
  // ongoing unzipping operations.
  base::OnceClosure cancellation_chain_ = base::DoNothing();

  // Scoped file access object required to open the zipped files when Data Leak
  // Prevention features are enabled.
  std::optional<file_access::ScopedFileAccess> file_access_;

  base::WeakPtrFactory<ExtractIOTask> weak_ptr_factory_{this};
};

}  // namespace file_manager::io_task

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_EXTRACT_IO_TASK_H_
