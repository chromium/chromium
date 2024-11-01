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
#include "chrome/common/extensions/api/file_manager_private.h"
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
  typedef base::OnceCallback<void(std::vector<RecentFile> files)>
      GetRecentFilesCallback;

  // File types to filter the results of GetRecentFiles().
  enum class FileType {
    kAll,
    kAudio,
    kDocument,
    kImage,
    kVideo,
  };

  // Parameters passed to GetRecentFiles(). May be copied.
  class Params {
   public:
    Params(storage::FileSystemContext* file_system_context,
           int32_t call_id,
           const GURL& origin,
           const std::string& query,
           const size_t max_files,
           const base::Time& cutoff_time,
           const base::TimeTicks& end_time,
           FileType file_type);
    Params(const Params& params);

    ~Params();

    // FileSystemContext that can be used for file system operations.
    storage::FileSystemContext* file_system_context() const {
      return file_system_context_.get();
    }

    int32_t call_id() const { return call_id_; }

    // Origin of external file system URLs.
    // E.g. "chrome-extension://<extension-ID>/"
    const GURL& origin() const { return origin_; }

    // The query to be applied to recent files to further narrow the returned
    // matches.
    const std::string& query() const { return query_; }

    // The maximum number of files to be returned by this source.
    size_t max_files() const { return max_files_; }

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

   private:
    scoped_refptr<storage::FileSystemContext> file_system_context_;
    const int32_t call_id_;
    const GURL origin_;
    const std::string query_;
    const size_t max_files_;
    const base::Time cutoff_time_;
    const FileType file_type_;
    const base::TimeTicks end_time_;
  };

  virtual ~RecentSource();

  // Retrieves a list of recent files from this source.
  //
  // You can assume that, once this function is called, it is not called again
  // until the callback is invoked. This means that you can safely save internal
  // states to compute recent files in member variables.
  virtual void GetRecentFiles(const Params& params,
                              GetRecentFilesCallback callback) = 0;

  // Called by the RecentModel if it wants to interrupt search for recent files.
  // The recent source may return whatever recent files it has collected so far
  // as the response to this call. If the Stop method is called, the callback
  // passed to GetRecentFiles is NEVER called. The `call_id` corresponds to one
  // of the `call_id` passed in the `params` of the GetRecentFiles` method.
  virtual std::vector<RecentFile> Stop(const int32_t call_id) = 0;

  // Returns the volume type that is serviced by this recent source.
  extensions::api::file_manager_private::VolumeType volume_type() const {
    return volume_type_;
  }

 protected:
  // Creates a new recent source that handles a volume of the given type.
  explicit RecentSource(
      extensions::api::file_manager_private::VolumeType volume_type);

 private:
  extensions::api::file_manager_private::VolumeType volume_type_;
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
