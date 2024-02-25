// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_DIVERSION_FILE_MANAGER_H_
#define CHROME_BROWSER_ASH_FILEAPI_DIVERSION_FILE_MANAGER_H_

#include <map>
#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"

namespace ash {

// Manages the creation, destruction and access to Diversion Files.
//
// Chromium's SBFS (//storage/browser/file_system) code implements an
// in-process virtual file system. It presents a traditional, POSIX-like API
// for a block-based file model (e.g. "open; write; write; write; close" or,
// when executing untrusted third-party code where file descriptor Denial of
// Service is a concern, "owc; owc; owc", where "owc" is a combined "open;
// write; close") where writes can be incremental and appending to an existing
// file is assumed to have O(1) complexity, where N is the prior file size.
//
// Some SBFS backends are backed by 'the cloud' instead of by local disk, and
// may be document-based (centered on a one-shot download/upload API) instead
// of block-based (open, read, write and close). In particular, appending M
// bytes to a virtual file (of size N) may first require downloading N bytes
// and then uploading (N + M) bytes, which has O(N) complexity for a single
// write operation. Overall, with a naive implementation, workflows with
// multiple write ops may require a quadratic amount of time and network.
//
// A Diversion File is a local file (with "O(1) append" behavior) that proxies
// or caches these remote files. For read-only workflows, this is basically
// just a local cache. For read-write workflows, writes are diverted to this
// file before ultimately being uploaded to the remote database, and follow-up
// reads (e.g. calculating Safe Browsing hashes of freshly written files) can
// hit the faster local disk instead of the slower remote database.
//
// Diversion (for a specific FileSystemURL) starts with a StartDiverting call
// and stops either explicitly or implicitly. Explicitly means after a
// FinishDiverting call. Implicitly means after an idle_timeout amount of time
// has passed since there were no live (constructed but not yet destroyed)
// FileStreamReader or FileStreamWriter objects for that FileSystemURL (and
// FinishDiverting has still not been called). At most one of explicit_callback
// and implicit_callback will be run.
//
// A DiversionFileManager's methods should only be called from the
// content::BrowserThread::IO thread. Callbacks run on the same thread.
class DiversionFileManager : public base::RefCounted<DiversionFileManager> {
 public:
  enum class StoppedReason {
    kExplicitFinish,
    kImplicitIdle,
  };

  enum class StartDivertingResult {
    kOK,                  // Returned when IsDiverting was false.
    kWasAlreadyDiverted,  // Returned when IsDiverting was true.
  };

  enum class FinishDivertingResult {
    kOK,               // Returned when IsDiverting was true.
    kWasNotDiverting,  // Returned when IsDiverting was false.
  };

  // Presents the cached contents as a ScopedFD to a real (in that it's a
  // kernel-visible file, not an SBFS virtual file), temporary file.
  //
  // If scoped_fd.is_valid() then it will have zero offset (in the "lseek(fd,
  // 0, SEEK_CUR)" sense).
  using Callback = base::OnceCallback<void(StoppedReason stopped_reason,
                                           const storage::FileSystemURL& url,
                                           base::ScopedFD scoped_fd,
                                           int64_t file_size,
                                           base::File::Error error)>;

  DiversionFileManager();

  DiversionFileManager(const DiversionFileManager&) = delete;
  DiversionFileManager& operator=(const DiversionFileManager&) = delete;

  bool IsDiverting(const storage::FileSystemURL& url);

  // Has no effect (and will not return kOK) if IsDiverting(url) was true. "No
  // effect" means that the callback will not be run.
  //
  // The implicit_callback may be null, equivalent to an empty-body Callback (a
  // no-op other than running the base::ScopedFD destructor; the temporary
  // file's contents are discarded).
  StartDivertingResult StartDiverting(const storage::FileSystemURL& url,
                                      base::TimeDelta idle_timeout,
                                      Callback implicit_callback);

  // Has no effect (and will not return kOK) if IsDiverting(url) was false. "No
  // effect" means that the callback will not be run.
  //
  // The explicit_callback may be null, equivalent to an empty-body Callback (a
  // no-op other than running the base::ScopedFD destructor; the temporary
  // file's contents are discarded).
  //
  // If IsDiverting(url) returned true, immediately before FinishDiverting was
  // called, then it will now return false, immediately afterwards. Regardless,
  // the explicit_callback might not run straight away, as it only runs after
  // all of the existing readers and writers are destroyed.
  //
  // If StartDiverting is called (with an equivalent url) after FinishDiverting
  // returns but before the explicit_callback runs, then it starts a new,
  // independent Diversion File. Subsequent reader or writer activity will not
  // affect the file contents or file size seen by explicit_callback.
  FinishDivertingResult FinishDiverting(const storage::FileSystemURL& url,
                                        Callback explicit_callback);

  // Factory methods for objects that allow reading from or writing to
  // Diversion Files. They return nullptr when IsDiverting(url) is false.
  std::unique_ptr<storage::FileStreamReader> CreateDivertedFileStreamReader(
      const storage::FileSystemURL& url,
      int64_t offset);
  std::unique_ptr<storage::FileStreamWriter> CreateDivertedFileStreamWriter(
      const storage::FileSystemURL& url,
      int64_t offset);

  void GetDivertedFileInfo(
      const storage::FileSystemURL& url,
      storage::FileSystemOperation::GetMetadataFieldSet fields,
      base::OnceCallback<void(base::File::Error result,
                              const base::File::Info& file_info)> callback);

  void TruncateDivertedFile(
      const storage::FileSystemURL& url,
      int64_t length,
      base::OnceCallback<void(base::File::Error result)> callback);

  void OverrideTmpfileDirForTesting(const base::FilePath& tmpfile_dir);

 private:
  class Entry;
  class Worker;
  using Map = std::map<storage::FileSystemURL,
                       scoped_refptr<Entry>,
                       storage::FileSystemURL::Comparator>;

  friend class base::RefCounted<DiversionFileManager>;
  ~DiversionFileManager();

  std::string TmpfileDirAsString() const;

  Map entries_;
  base::FilePath tmpfile_dir_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_DIVERSION_FILE_MANAGER_H_
