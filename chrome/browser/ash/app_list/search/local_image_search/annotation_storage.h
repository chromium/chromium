// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_ANNOTATION_STORAGE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_ANNOTATION_STORAGE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/local_image_search/file_search_result.h"

namespace app_list {

class ImageAnnotationWorker;
class SqlDatabase;

// Image metadata retrieved from the database.
struct ImageInfo {
  // All the annotations attributed to the image.
  std::set<std::string> annotations;
  // The full path to the image.
  base::FilePath path;
  // The image's last modified time.
  base::Time last_modified;
  // File size.
  int64_t file_size;

  ImageInfo(const std::set<std::string>& annotations,
            const base::FilePath& path,
            const base::Time& last_modified,
            int64_t file_size);

  ~ImageInfo();
  ImageInfo(const ImageInfo&);
  ImageInfo& operator=(const ImageInfo&) = delete;
};

// A persistent storage to efficiently store, retrieve and search annotations.
// Creates or opens a database under `path_to_db`. If `annotation_worker` is
// not null, it updates the database on file changes.
class AnnotationStorage {
 public:
  AnnotationStorage(const base::FilePath& path_to_db,
                    std::unique_ptr<ImageAnnotationWorker> annotation_worker);
  AnnotationStorage(const AnnotationStorage&) = delete;
  AnnotationStorage& operator=(const AnnotationStorage&) = delete;
  ~AnnotationStorage();

  // Initializes the db. Must be called before any other method.
  void Initialize();

  // Adds a new image to the storage.
  void Insert(const ImageInfo& image_info);

  // Removes an image from the storage. It does nothing if the file does not
  void Remove(const base::FilePath& image_path);

  // Returns all the stored annotations.
  std::vector<ImageInfo> GetAllAnnotationsForTest();

  // Returns all the stored file paths.
  std::vector<base::FilePath> GetAllFiles();

  // Find all the files in a directory.
  std::vector<base::FilePath> SearchByDirectory(
      const base::FilePath& directory) const;

  // Searches the database for a desired `image_path`.
  std::vector<ImageInfo> FindImagePath(const base::FilePath& image_path);

  // Get `last_modified_time` of the image by `image_path` from the database.
  // If the image is not found in the database, return `base::Time()` instead.
  const base::Time GetLastModifiedTime(const base::FilePath& image_path);

  // Search for a multi-term query. The results are sorted by `relevance`.
  std::vector<FileSearchResult> Search(const std::u16string& query,
                                       size_t max_num_results);

 private:
  AnnotationStorage(const base::FilePath& path_to_db,
                    int current_version_number,
                    std::unique_ptr<ImageAnnotationWorker> annotation_worker);

  // Regex search for annotations using FuzzyTokenizedStringMatch to obtain
  // relevance for the `query_term`. The results are sorted by `file_path`.
  std::vector<FileSearchResult> PrefixSearch(const std::u16string& query_term);

  std::unique_ptr<ImageAnnotationWorker> annotation_worker_;
  std::unique_ptr<SqlDatabase> sql_database_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_ANNOTATION_STORAGE_H_
