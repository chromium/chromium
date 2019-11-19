// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

namespace chromeos {

class RecentFile;

// Interface class for a source of recent files.
//
// Recent file system retrieves recent files from several sources such as
// local directories and cloud storages. To provide files to Recent file
// system, this interface should be implemented for each source.
//
// Note: If a source implementation depends on KeyedServices, remember to add
// dependencies in RecentModelFactory.
//
// All member functions must be called on the UI thread.
class RecentSource {
 public:
  using GetRecentFilesCallback =
      base::OnceCallback<void(std::vector<RecentFile> files)>;

  // Parameters passed to GetRecentFiles().
  class Params {
   public:
    Params(storage::FileSystemContext* file_system_context,
           const GURL& origin,
           size_t max_files,
           const base::Time& cutoff_time,
           GetRecentFilesCallback callback);

    Params(const Params& other) = delete;
    Params(Params&& other);
    ~Params();
    Params& operator=(const Params& other) = delete;

    // FileSystemContext that can be used for file system operations.
    storage::FileSystemContext* file_system_context() const {
      return file_system_context_.get();
    }

    // Origin of external file system URLs.
    // E.g. "chrome-extension://<extension-ID>/"
    const GURL& origin() const { return origin_; }

    // Maximum number of files a RecentSource is expected to return. It is fine
    // to return more files than requested here, but excessive items will be
    // filtered out by RecentModel.
    size_t max_files() const { return max_files_; }

    // Cut-off last modified time. RecentSource is expected to return files
    // modified at this time or later. It is fine to return older files than
    // requested here, but they will be filtered out by RecentModel.
    const base::Time& cutoff_time() const { return cutoff_time_; }

    // Callback to be called for the result of GetRecentFiles().
    GetRecentFilesCallback& callback() { return callback_; }

   private:
    scoped_refptr<storage::FileSystemContext> file_system_context_;
    GURL origin_;
    size_t max_files_;
    base::Time cutoff_time_;
    GetRecentFilesCallback callback_;
  };

  virtual ~RecentSource();

  // Retrieves a list of recent files from this source.
  //
  // You can assume that, once this function is called, it is not called again
  // until the callback is invoked. This means that you can safely save internal
  // states to compute recent files in member variables.
  virtual void GetRecentFiles(Params params) = 0;

 protected:
  RecentSource();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_RECENT_SOURCE_H_
