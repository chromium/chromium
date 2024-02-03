// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_COPY_FROM_FD_H_
#define CHROME_BROWSER_ASH_FILEAPI_COPY_FROM_FD_H_

#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "net/base/net_errors.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/common/file_system/file_system_mount_option.h"

namespace ash {

// The callback returns the ScopedFD and unique_ptr<FileStreamWriter> - both
// are ownership types - originally passed (loaned) to CopyFromFileDescriptor,
// along with any error from the actual copy.
using CopyFromFileDescriptorCallback = base::OnceCallback<void(
    base::ScopedFD scoped_fd,
    std::unique_ptr<storage::FileStreamWriter> fs_writer,
    net::Error error)>;

// CopyFromFileDescriptor copies the contents of src_scoped_fd to the
// dst_fs_writer. Ownership of that ScopedFD and FileStreamWriter is passed to
// the callee and returned (via the callback) when done.
//
// The copy starts from the file descriptor's current location, in the
// "lseek(fd, 0, SEEK_CUR)" sense, copying up to EOF (End Of File).
//
// On success, it calls FileStreamWriter::Flush(FlushMode::kEndOfFile) if
// dst_flush_policy is FlushPolicy::FLUSH_ON_COMPLETION, but it does not
// destroy the FileStreamWriter object and, as a FileStreamWriter has no
// explicit Close method, some FileStreamWriter implementations may delay
// actually committing written data until their destructor runs.
//
// It should only be called from the content::BrowserThread::IO thread.
//
// It is a bit like storage::FileSystemOperation::Copy (or CopyInForeignFile)
// but the source is an already-open file descriptor (possibly a nameless
// O_TMPFILE file), not a storage::FileSystemURL (which can only refer to
// namable things). The destination is also a (storage::FileStreamWriter,
// storage::FlushPolicy) pair, not a storage::FileSystemURL, so that callers
// can pass specialized (private) FileStreamWriter implementations.
void CopyFromFileDescriptor(
    base::ScopedFD src_scoped_fd,
    std::unique_ptr<storage::FileStreamWriter> dst_fs_writer,
    storage::FlushPolicy dst_flush_policy,
    CopyFromFileDescriptorCallback callback);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_COPY_FROM_FD_H_
