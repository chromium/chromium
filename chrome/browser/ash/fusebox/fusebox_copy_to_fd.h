// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_COPY_TO_FD_H_
#define CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_COPY_TO_FD_H_

#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "storage/browser/file_system/file_system_context.h"

namespace fusebox {

// CopyToFileDescriptor copies the contents of |src_fs_url| to the
// |dst_scoped_fd|. Ownership of that base::ScopedFD is passed to the callee
// and returned (via the callback) when done.
//
// It should only be called from the content::BrowserThread::IO thread.
//
// It is a bit like storage::FileSystemOperation::Copy (or CopyInForeignFile)
// but the destination is an already-open file descriptor (possibly a nameless
// O_TMPFILE file), not a storage::FileSystemURL (which can only refer to
// namable things).
//
// It is a bit like storage::FileSystemOperation::CreateSnapshotFile but it
// always copies the bytes. CreateSnapshotFile can sometimes be effectively a
// no-op, if the source file is already on the local disk. This works when the
// snapshot is only read from (not written to) but can introduce aliasing or
// ownership questions when the destination file will be further modified.
//
// CreateSnapshotFile (a cross-platform API) also does not write to an
// automatically deleted-on-close O_TMPFILE file (a Linux-specific concept)
// which means that the CreateSnapshotFile caller needs to explicitly manage
// and garbage-collect snapshot files after use.
void CopyToFileDescriptor(
    scoped_refptr<storage::FileSystemContext> fs_context,
    const storage::FileSystemURL& src_fs_url,
    base::ScopedFD dst_scoped_fd,
    // The E=int in the base::expected<T, E> is a POSIX error code.
    base::OnceCallback<void(base::expected<base::ScopedFD, int>)> callback);

}  // namespace fusebox

#endif  // CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_COPY_TO_FD_H_
