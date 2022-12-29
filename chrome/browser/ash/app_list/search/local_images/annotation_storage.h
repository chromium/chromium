// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_ANNOTATION_STORAGE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_ANNOTATION_STORAGE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/app_list/search/local_images/local_image_search_provider.h"
#include "net/extras/sqlite/sqlite_persistent_store_backend_base.h"

namespace base {
class FilePath;
}  // namespace base

namespace app_list {

class ImageAnnotationWorker;

// A persistent storage to efficiently store, retrieve and search annotations.
// It maintains and runs tasks on its own background task runner.
// Constructor and all *Async() methods can be called on any sequence.
// TODO(b/260646344): Pass SQL review.
class AnnotationStorage : public net::SQLitePersistentStoreBackendBase {
 public:
  enum class TableColumnName {
    kLabel,
    kImagePath,
    kLastModifiedTime,
  };

  AnnotationStorage(const base::FilePath& path,
                    const std::string& histogram_tag,
                    int current_version_number,
                    int compatible_version_number,
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

  // Searches annotations using FuzzyTokenizedStringMatch.
  // Can be called from any sequence.
  bool LinearSearchAnnotationsAsync(
      std::u16string query,
      base::OnceCallback<void(std::vector<ImageInfo>)> callback);

  // SQLitePersistentStoreBackendBase overrides:
  absl::optional<int> DoMigrateDatabaseSchema() override;
  bool CreateDatabaseSchema() override;
  void DoCommit() override;

 protected:
  ~AnnotationStorage() override;

 private:
  // Runs the worker after the db initialization.
  void OnInitializationComplete(bool status);
  bool InsertOnBackgroundSequence(ImageInfo image_info);
  bool RemoveOnBackgroundSequence(base::FilePath image_path);

  // Searches the database for a desired `value` in the `column_name`.
  // Yields all annotations if `column_name` and `value` are empty.
  std::vector<ImageInfo> FindAnnotationsOnBackgroundSequence(
      absl::optional<TableColumnName> column_name,
      absl::optional<std::string> value);

  // Searches annotations using FuzzyTokenizedStringMatch.
  std::vector<ImageInfo> LinearSearchAnnotationsOnBackgroundSequence(
      std::u16string query);

  std::unique_ptr<ImageAnnotationWorker> annotation_worker_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_ANNOTATION_STORAGE_H_
