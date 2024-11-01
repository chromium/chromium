// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_ANNOTATION_STORAGE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_ANNOTATION_STORAGE_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/local_image_search/file_search_result.h"
#include "chrome/browser/ash/app_list/search/local_image_search/search_utils.h"

namespace app_list {

class ImageAnnotationWorker;
class SqlDatabase;

// The annotation information, used by ICA.
struct AnnotationInfo {
  // The confidence score of the annotation, in range [0,1].
  float score;
  // The x-axis and y-axis of the annotation location.
  std::optional<float> x;
  std::optional<float> y;
  // The area of the annotation within the image.
  std::optional<float> area;
};

// Image metadata retrieved from the database.
struct ImageInfo {
  // Set of annotations attributed to the image. Used by OCR.
  std::set<std::string> annotations;
  // Map of annotation attributed to the image, with confidence score and
  // bounding box info. Used by ICA.
  std::map<std::string, AnnotationInfo> annotation_map;
  // The full path to the image.
  base::FilePath path;
  // The image's last modified time.
  base::Time last_modified;
  // File size.
  int64_t file_size;
  // Width and height of image.
  int width;
  int height;

  ImageInfo(const std::set<std::string>& annotations,
            const base::FilePath& path,
            const base::Time& last_modified,
            int64_t file_size);

  ~ImageInfo();
  ImageInfo(const ImageInfo&);
  ImageInfo& operator=(const ImageInfo&) = delete;
};

// The current status of Image retrieved from the database.
struct ImageStatus {
  // The image's last modified time. If not set, it indicates the image is not
  // found from the database.
  std::optional<base::Time> last_modified;
  // OCR indexing version. 0 if not indexed.
  int ocr_version = 0;
  // ICA indexing version. 0 if not indexed.
  int ica_version = 0;

  ImageStatus() = default;
  ImageStatus(base::Time last_modified, int ocr_version, int ica_version)
      : last_modified(last_modified),
        ocr_version(ocr_version),
        ica_version(ica_version) {}
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

  // Adds a new image to the storage. By default, the indexing source is OCR.
  void Insert(const ImageInfo& image_info,
              IndexingSource indexing_source = IndexingSource::kOcr);

  // Removes an image from the storage. It does nothing if the file does not
  void Remove(const base::FilePath& image_path);

  // Returns all the stored annotations. Each annotation is stored in a separate
  // ImageInfo instance for testing purpose.
  std::vector<ImageInfo> GetAllAnnotationsForTest();

  // Returns all the stored file paths.
  std::vector<base::FilePath> GetAllFiles();

  // Find all the files in a directory.
  std::vector<base::FilePath> SearchByDirectory(
      const base::FilePath& directory) const;

  // Searches the database for a desired `image_path`.
  std::vector<ImageInfo> FindImagePath(const base::FilePath& image_path);

  // Get the current status of the image by `image_path` from the database.
  // Currently, we care about the last modified time, and the OCR&ICA indexed
  // version. If the image is not found in the database, return `base::Time()`
  // as last modified time and 0 as the ICA&OCR indexed version instead.
  const ImageStatus GetImageStatus(const base::FilePath& image_path);

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
