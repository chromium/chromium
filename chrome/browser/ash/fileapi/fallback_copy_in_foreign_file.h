// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_FALLBACK_COPY_IN_FOREIGN_FILE_H_
#define CHROME_BROWSER_ASH_FILEAPI_FALLBACK_COPY_IN_FOREIGN_FILE_H_

#include "storage/browser/file_system/async_file_util.h"

namespace ash {

// Implements `storage::AsyncFileUtil::CopyInForeignFile` semantics by calling
// other `AsyncFileUtil` (and `FileStreamWriter` and `FileSystemContext`
// methods). Specifically:
//   - `AsyncFileUtil::DeleteFile`
//   - `AsyncFileUtil::EnsureFileExists`
//   - `AsyncFileUtil::GetFileInfo`
//   - `AsyncFileUtil::MoveFileLocal`
//   - `FileStreamWriter::*`
//   - `FileSystemContext::CreateFileStreamWriter`
//
// Its implementation may create temporary files in the destination file
// system, since overwriting the `dest_url` directly is generally not atomic
// (but can still take a long time, especially for network-backed virtual file
// systems) and so would be:
//   1. More likely to expose partial results to concurrent readers.
//   2. Difficult (if not impossible) to roll back on failure.
//
// Those temporary files' names or other aspects are private implementation
// details. Those details may change in the future and callers should not rely
// on specific filenames being touched. The callee (not caller) is responsible
// for cleaning up any temporary files, regardless of success or failure.
// However, virtual file systems generally don't provide atomic transactions.
// Severe interruption (e.g. due to network or power failure) may result in
// these temporary files persisting (and the `dest_url` may be deleted or
// renamed away) without automatic rollback.
//
// This function enables code re-use: `AsyncFileUtil` subclasses may delegate
// their `AsyncFileUtil::CopyInForeignFile` implementation to simply call this
// function (provided that they implement the other methods listed above). They
// may also choose to (perhaps conditionally) call their own backend-specific
// specialized or optimized implementations instead.
COMPONENT_EXPORT(STORAGE_BROWSER)
void FallbackCopyInForeignFile(
    storage::AsyncFileUtil& async_file_util,
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const storage::FileSystemURL& dest_url,
    storage::AsyncFileUtil::StatusCallback callback);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_FALLBACK_COPY_IN_FOREIGN_FILE_H_
