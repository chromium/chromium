// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_ROOT_DELETE_HELPER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_ROOT_DELETE_HELPER_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/file_system_url.h"

class GURL;

namespace storage {
class FileSystemContext;
}

namespace sync_file_system {

class LocalFileSyncStatus;

// A helper class to delete the root directory of a given Sync FileSystem.
// This could happen when AppRoot (or SyncRoot) is deleted by remote operation,
// and we want to delete all local files + all pending local changes in this
// case.
//
// Expected to be called on and will callback on IO thread.
class RootDeleteHelper {
 public:
  typedef base::Callback<void(base::File::Error)> FileStatusCallback;

  RootDeleteHelper(storage::FileSystemContext* file_system_context,
                   LocalFileSyncStatus* sync_status,
                   const storage::FileSystemURL& url,
                   const FileStatusCallback& callback);
  ~RootDeleteHelper();

  void Run();

 private:
  void DidDeleteFileSystem(base::File::Error error);
  void DidResetFileChangeTracker();
  void DidOpenFileSystem(const GURL& root,
                         const std::string& name,
                         base::File::Error error);

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  const storage::FileSystemURL url_;
  FileStatusCallback callback_;

  // Not owned; owner of this instance owns it.
  LocalFileSyncStatus* sync_status_;

  base::WeakPtrFactory<RootDeleteHelper> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RootDeleteHelper);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_ROOT_DELETE_HELPER_H_
