// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_IMAGE_ANNOTATION_WORKER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_IMAGE_ANNOTATION_WORKER_H_

#include <memory>
#include <set>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"

namespace base {
class FilePathWatcher;
}

namespace app_list {
class AnnotationStorage;
struct ImageInfo;

// The worker watches `root_path_` for any image changes, runs ICA on every
// change, and saves the annotation to the AnnotationStorage.
// It maintains and runs tasks on its own background task runner.
// TODO(b/260646344): Revisit the use of a FilePathWatcher for My Files
//  if needed. (It may hit the folder limit.)
class ImageAnnotationWorker {
 public:
  explicit ImageAnnotationWorker(const base::FilePath& root_path);
  ~ImageAnnotationWorker();
  ImageAnnotationWorker(const ImageAnnotationWorker&) = delete;
  ImageAnnotationWorker& operator=(const ImageAnnotationWorker&) = delete;

  // Spawns the worker in a low-priority sequence and attaches it to the
  // storage. Can be called from any sequence.
  void Run(AnnotationStorage* const annotation_storage);

 private:
  // Setups file watchers.
  void StartWatching();
  void OnFileChange(const base::FilePath& path,
                    bool is_exist,
                    std::unique_ptr<base::File::Info> file_info);

  // Gets an annotations from the `image_path`.
  void ProcessImage(base::FilePath image_path,
                    std::unique_ptr<base::File::Info> file_info,
                    std::vector<ImageInfo> stored_annotations_with_this_path);

  // Obtains stored image paths and checks for deleted images
  void CheckForDeletedImages();

  // Removes deleted images from the annotation storage.
  void FindAndRemoveDeletedImages(const std::vector<ImageInfo> images);

  // Removes deleted images from the annotation storage.
  void RemovePathsFromDb(const std::set<base::FilePath>& paths);

  std::unique_ptr<base::FilePathWatcher> file_watcher_;
  base::FilePath root_path_;

  // Owned by the caller.
  AnnotationStorage* annotation_storage_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ImageAnnotationWorker> weak_ptr_factory_{this};
};
}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_IMAGE_ANNOTATION_WORKER_H_
