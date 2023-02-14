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
#include "base/files/file_path_watcher.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/services/machine_learning/public/mojom/image_content_annotation.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

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
  void Run(scoped_refptr<AnnotationStorage> annotation_storage);

  // Disables mojo bindings and file watchers.
  void UseFakeAnnotatorForTests();

  // Deterministically triggers the event instead of using file watchers, which
  // cannot be awaited by `RunUntilIdle()` and introduce unwanted flakiness.
  void TriggerOnFileChangeForTests(const base::FilePath& path, bool error);

 protected:
  // Setups file watchers.
  void StartWatching();
  void OnFileChange(const base::FilePath& path, bool error);

  // Gets an annotations from the `image_path`.
  void ProcessImage(base::FilePath image_path,
                    std::unique_ptr<base::File::Info> file_info,
                    std::vector<ImageInfo> stored_annotations_with_this_path);

  // Removes deleted images from the annotation storage.
  void FindAndRemoveDeletedImages(const std::vector<ImageInfo> images);

  void ConnectToImageAnnotator();
  void RunImageAnnotator(ImageInfo image_info,
                         base::MappedReadOnlyRegion mapped_region);
  // For testing.
  void RunFakeImageAnnotator(ImageInfo image_info,
                             base::MappedReadOnlyRegion mapped_region);

  void EnsureAnnotatorIsConnected();

  std::unique_ptr<base::FilePathWatcher> file_watcher_;
  base::FilePath root_path_;
  scoped_refptr<AnnotationStorage> annotation_storage_;

  mojo::Remote<chromeos::machine_learning::mojom::MachineLearningService>
      ml_service_;
  mojo::Remote<chromeos::machine_learning::mojom::ImageContentAnnotator>
      image_content_annotator_;

  base::FilePathWatcher::Callback on_file_change_callback_;

  bool use_fake_annotator_for_tests_ = false;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<ImageAnnotationWorker> weak_ptr_factory_{this};
};
}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGES_IMAGE_ANNOTATION_WORKER_H_
