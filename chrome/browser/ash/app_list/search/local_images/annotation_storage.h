// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_ANNOTATION_STORAGE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_ANNOTATION_STORAGE_H_

#include <memory>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  ImageInfo(const std::set<std::string>& annotations,
            const base::FilePath& path,
            const base::Time& last_modified);

  ~ImageInfo();
  ImageInfo(const ImageInfo&);
  ImageInfo& operator=(const ImageInfo&) = delete;
};

// A search result with `relevance` to the supplied query.
struct FileSearchResult {
  // THe full path to the file.
  base::FilePath path;
  // The file's last modified time.
  base::Time last_modified;
  // The file's relevance on the scale from 0-1. It represents how closely a
  // query matches the file's annotation.
  double relevance;

  FileSearchResult(const base::FilePath& path,
                   const base::Time& last_modified,
                   double relevance);

  ~FileSearchResult();
  FileSearchResult(const FileSearchResult&);
  FileSearchResult& operator=(const FileSearchResult&) = delete;
};

// A persistent storage to efficiently store, retrieve and search annotations.
// It maintains and runs tasks on its own background task runner.
// Constructor and all *Async() methods can be called on any sequence.
// TODO(b/260646344): Pass SQL review.
class AnnotationStorage : public base::RefCountedThreadSafe<AnnotationStorage> {
 public:
  enum class TableColumnName {
    kLabel,
    kImagePath,
    kLastModifiedTime,
  };

  AnnotationStorage(const base::FilePath& path,
                    const std::string& histogram_tag,
                    int current_version_number,
                    std::unique_ptr<ImageAnnotationWorker> annotation_worker);
  AnnotationStorage(const AnnotationStorage&) = delete;
  AnnotationStorage& operator=(const AnnotationStorage&) = delete;

  // Initializes the db. Must be called before any other method.
  // Can be called from any sequence.
  bool InitializeAsync();

  // Adds a new image to the storage. Can be called from any sequence.
  bool InsertOrReplaceAsync(ImageInfo image_info);

  // Removes an image from the storage. It does nothing if the file does not
  // exist. Can be called from any sequence.
  bool RemoveAsync(base::FilePath image_path);

  // TODO(b/260646344): Remove after implementing a more efficient search.
  // Returns all the stored annotations. Can be called from any sequence.
  bool GetAllAnnotationsAsync(
      base::OnceCallback<void(std::vector<ImageInfo>)> callback);

  // Searches the database for a desired `image_path`.
  // Can be called from any sequence.
  bool FindImagePathAsync(
      base::FilePath image_path,
      base::OnceCallback<void(std::vector<ImageInfo>)> callback);

  // Searches for annotations using FuzzyTokenizedStringMatch with relevance to
  // `query` above a fixed threshold. Can be called from any sequence.
  bool LinearSearchAnnotationsAsync(
      std::u16string query,
      base::OnceCallback<void(std::vector<FileSearchResult>)> callback);

 private:
  friend class base::RefCountedThreadSafe<AnnotationStorage>;
  ~AnnotationStorage();
  // Runs the worker in the background.
  void StartWorkerOnBackgroundSequence(bool is_error);
  bool InsertOnBackgroundSequence(ImageInfo image_info);
  bool RemoveOnBackgroundSequence(base::FilePath image_path);

  // Searches the database for a desired `value` in the `column_name`.
  // Yields all annotations if `column_name` and `value` are empty.
  std::vector<ImageInfo> FindAnnotationsOnBackgroundSequence(
      absl::optional<TableColumnName> column_name,
      absl::optional<std::string> value);

  // Searches annotations using FuzzyTokenizedStringMatch.
  std::vector<FileSearchResult> LinearSearchAnnotationsOnBackgroundSequence(
      std::u16string query);

  // Initialized and operates in the background sequence.
  std::unique_ptr<ImageAnnotationWorker> annotation_worker_;

  std::unique_ptr<SqlDatabase> sql_database_;

  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_ANNOTATION_STORAGE_H_
