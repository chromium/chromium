// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_FILE_SYSTEM_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_FILE_SYSTEM_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "storage/common/file_system/file_system_types.h"
#include "url/origin.h"

namespace storage {
class FileSystemContext;
}

class Profile;

// BrowsingDataFileSystemHelper instances for a specific profile should be
// created via the static Create method. Each instance will lazily fetch file
// system data when a client calls StartFetching from the UI thread, and will
// notify the client via a supplied callback when the data is available.
//
// The client's callback is passed a list of FileSystemInfo objects containing
// usage information for each origin's temporary and persistent file systems.
//
// Clients may remove an origin's file systems at any time (even before fetching
// data) by calling DeleteFileSystemOrigin() on the UI thread. Calling
// DeleteFileSystemOrigin() for an origin that doesn't have any is safe; it's
// just an expensive NOOP.
class BrowsingDataFileSystemHelper
    : public base::RefCountedThreadSafe<BrowsingDataFileSystemHelper> {
 public:
  // Detailed information about a file system, including its origin and the
  // amount of data (in bytes) for each sandboxed filesystem type.
  struct FileSystemInfo {
    explicit FileSystemInfo(const url::Origin& origin);
    FileSystemInfo(const FileSystemInfo& other);
    ~FileSystemInfo();

    // The origin for which the information is relevant.
    url::Origin origin;
    // FileSystemType to usage (in bytes) map.
    std::map<storage::FileSystemType, int64_t> usage_map;
  };

  using FetchCallback =
      base::OnceCallback<void(const std::list<FileSystemInfo>&)>;

  // Creates a BrowsingDataFileSystemHelper instance for the file systems
  // stored in |profile|'s user data directory. The BrowsingDataFileSystemHelper
  // object will hold a reference to the Profile that's passed in, but is not
  // responsible for destroying it.
  //
  // The BrowsingDataFileSystemHelper will not change the profile itself, but
  // can modify data it contains (by removing file systems).
  static BrowsingDataFileSystemHelper* Create(
      storage::FileSystemContext* file_system_context);

  // Starts the process of fetching file system data, which will call |callback|
  // upon completion, passing it a constant list of FileSystemInfo objects.
  // StartFetching must be called only in the UI thread; the provided Callback1
  // will likewise be executed asynchronously on the UI thread.
  //
  // BrowsingDataFileSystemHelper takes ownership of the Callback1, and is
  // responsible for deleting it once it's no longer needed.
  virtual void StartFetching(FetchCallback callback);

  // Deletes any temporary or persistent file systems associated with |origin|
  // from the disk. Deletion will occur asynchronously on the FILE thread, but
  // this function must be called only on the UI thread.
  virtual void DeleteFileSystemOrigin(const url::Origin& origin);

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataFileSystemHelper>;

  explicit BrowsingDataFileSystemHelper(
      storage::FileSystemContext* filesystem_context);

  virtual ~BrowsingDataFileSystemHelper();

 private:
  // Enumerates all filesystem files, storing the resulting list into
  // file_system_file_ for later use. This must be called on the file
  // task runner.
  void FetchFileSystemInfoInFileThread(FetchCallback callback);

  // Deletes all file systems associated with |origin|. This must be called on
  // the file task runner.
  void DeleteFileSystemOriginInFileThread(const url::Origin& origin);

  // Returns the file task runner for the |filesystem_context_|.
  base::SequencedTaskRunner* file_task_runner();

  // Keep a reference to the FileSystemContext object for the current profile
  // for use on the file task runner.
  scoped_refptr<storage::FileSystemContext> filesystem_context_;
};

// An implementation of the BrowsingDataFileSystemHelper interface that can
// be manually populated with data, rather than fetching data from the file
// systems created in a particular Profile. Only kTemporary file systems
// are supported.
class CannedBrowsingDataFileSystemHelper
    : public BrowsingDataFileSystemHelper {
 public:
  explicit CannedBrowsingDataFileSystemHelper(
      storage::FileSystemContext* filesystem_context);

  // Manually adds a filesystem to the set of canned file systems that this
  // helper returns via StartFetching.
  void Add(const url::Origin& origin);

  // Clear this helper's list of canned filesystems.
  void Reset();

  // True if no filesystems are currently stored.
  bool empty() const;

  // Returns the number of currently stored filesystems.
  size_t GetCount() const;

  // Returns the current list of filesystems.
  const std::set<url::Origin>& GetOrigins() const { return pending_origins_; }

  // BrowsingDataFileSystemHelper implementation.
  void StartFetching(FetchCallback callback) override;
  void DeleteFileSystemOrigin(const url::Origin& origin) override;

 private:
  ~CannedBrowsingDataFileSystemHelper() override;

  // Holds the current list of filesystems returned to the client.
  std::set<url::Origin> pending_origins_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataFileSystemHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_FILE_SYSTEM_HELPER_H_
