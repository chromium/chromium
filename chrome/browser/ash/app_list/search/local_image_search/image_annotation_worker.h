// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_IMAGE_ANNOTATION_WORKER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_IMAGE_ANNOTATION_WORKER_H_

#include <memory>
#include <vector>

#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/app_list/search/local_image_search/annotation_storage.h"
#include "chrome/browser/ash/app_list/search/local_image_search/image_content_annotator.h"
#include "chrome/browser/ash/app_list/search/local_image_search/optical_character_recognizer.h"

namespace base {
class FilePathWatcher;
}

namespace gfx {
class ImageSkia;
}

namespace app_list {
struct ImageInfo;

// The worker watches `root_path_` for any image changes, runs ICA on every
// change, and saves the annotation to the AnnotationStorage.
// It can be created on any sequence but must be initialized on the same
// sequence as AnnotationStorage. It runs IO heavy tasks on a background
// task runner.
// The worker supports on-device Optical Character Recognition (OCR) and
// Image Content-based Annotation (ICA) via DLCs.
// TODO(b/260646344): Revisit the use of a FilePathWatcher for My Files
//  if needed. (It may hit the folder limit.)
class ImageAnnotationWorker {
 public:
  explicit ImageAnnotationWorker(
      const base::FilePath& root_path,
      const std::vector<base::FilePath>& excluded_paths,
      bool use_file_watchers,
      bool use_ocr,
      bool use_ica);
  ~ImageAnnotationWorker();
  ImageAnnotationWorker(const ImageAnnotationWorker&) = delete;
  ImageAnnotationWorker& operator=(const ImageAnnotationWorker&) = delete;

  // Initializes a file watcher, connects to ICA and performs a file system
  // scan for new images. It must be called on the same sequence as
  // AnnotationStorage is bound to.
  void Initialize(AnnotationStorage* annotation_storage);

  // Deterministically triggers the event instead of using file watchers, which
  // cannot be awaited by `RunUntilIdle()` and introduce unwanted flakiness.
  void TriggerOnFileChangeForTests(const base::FilePath& path, bool error);

 private:
  void OnFileChange(const base::FilePath& path, bool error);

  // Processes the next item from the `files_to_process_` queue.
  void ProcessNextItem();

  // Processes the next directory from the `files_to_process_` queue.
  void ProcessNextDirectory();

  // Processes the next image from the `files_to_process_` queue.
  void ProcessNextImage();

  // Remove all the files from a deleted directory.
  void RemoveOldDirectory();

  // Removes deleted images from the annotation storage.
  void FindAndRemoveDeletedFiles(const std::vector<base::FilePath> images);

  // For testing. File name annotator.
  void RunFakeImageAnnotator(ImageInfo image_info);

  void EnsureOcrAnnotatorIsConnected();

  // Initializes the `file_watcher_` and does initial data checks.
  void OnDlcInstalled();

  void OnDecodeImageFile(ImageInfo image_info,
                         const gfx::ImageSkia& image_skia);

  void OnPerformIca(
      ImageInfo image_info,
      chromeos::machine_learning::mojom::ImageAnnotationResultPtr ptr);

  void OnPerformOcr(ImageInfo image_info,
                    screen_ai::mojom::VisualAnnotationPtr visual_annotation);

  void OnImageProcessTimeout();

  std::unique_ptr<base::FilePathWatcher> file_watcher_;
  base::FilePath root_path_;
  // Excludes any path matching the prefixes.
  std::vector<base::FilePath> excluded_paths_;

  base::FilePathWatcher::Callback on_file_change_callback_;

  // AnnotationStorage owns this ImageAnnotationWorker. All the methods must
  // be called from the main sequence.
  raw_ptr<AnnotationStorage, DanglingUntriaged | ExperimentalAsh>
      annotation_storage_;

  // ML models used as DLCs.
  ImageContentAnnotator image_content_annotator_;
  OpticalCharacterRecognizer optical_character_recognizer_;

  const bool use_file_watchers_;
  const bool use_ica_;
  const bool use_ocr_;
  base::queue<base::FilePath> files_to_process_;
  int num_retries_left_ = 60;

  base::OneShotTimer timeout_timer_;
  // Owned by this class.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ImageAnnotationWorker> weak_ptr_factory_{this};
};
}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_IMAGE_ANNOTATION_WORKER_H_
