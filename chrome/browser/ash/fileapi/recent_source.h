// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_RECENT_SOURCE_H_
#define CHROME_BROWSER_ASH_FILEAPI_RECENT_SOURCE_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/i18n/string_search.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

namespace ash {

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

  // File types to filter the results of GetRecentFiles().
  enum class FileType {
    kAll,
    kAudio,
    kDocument,
    kImage,
    kVideo,
  };

  // Parameters passed to GetRecentFiles().
  class Params {
   public:
    Params(storage::FileSystemContext* file_system_context,
           const GURL& origin,
           size_t max_files,
           const std::string& query,
           const base::Time& cutoff_time,
           const base::TimeTicks& end_time,
           FileType file_type,
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

    // The query to be applied to recent files to further narrow the returned
    // matches.
    const std::string& query() const { return query_; }

    // Cut-off last modified time. RecentSource is expected to return files
    // modified at this time or later. It is fine to return older files than
    // requested here, but they will be filtered out by RecentModel.
    const base::Time& cutoff_time() const { return cutoff_time_; }

    // File type to filter the results from RecentSource. RecentSource is
    // expected to return files which matches the specified file type.
    FileType file_type() const { return file_type_; }

    // Returns the time by which recent scan operation should terminate (with or
    // without results).
    base::TimeTicks end_time() const { return end_time_; }

    // If maximum duration was set, this method checks if a recent source is
    // late with delivery of recent files or still on schedule. If maximum
    // duration was never set, this method always returns false.
    bool IsLate() const;

    // Callback to be called for the result of GetRecentFiles().
    GetRecentFilesCallback& callback() { return callback_; }

   private:
    scoped_refptr<storage::FileSystemContext> file_system_context_;
    GURL origin_;
    size_t max_files_;
    std::string query_;
    base::Time cutoff_time_;
    FileType file_type_;
    const base::TimeTicks end_time_;
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

// A common to all recent sources function for checking a file name against the
// query. This function returns true if the query is contained in the given file
// name. This function does case-insensitive, accent-insensitive comparison.
inline bool FileNameMatches(const std::u16string& file_name,
                            const std::u16string& query) {
  return query.empty() || base::i18n::StringSearchIgnoringCaseAndAccents(
                              query, file_name, nullptr, nullptr);
}

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_RECENT_SOURCE_H_
