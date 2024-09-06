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
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/local_image_search/annotation_storage.h"
#include "chrome/browser/ash/app_list/search/local_image_search/image_content_annotator.h"
#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"

class Profile;

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
// TODO(b/260646344): Revisit the use of a FilePathWatcher for MyFiles
//  if needed. (It may hit the folder limit.)
class ImageAnnotationWorker {
 public:
  explicit ImageAnnotationWorker(
      const base::FilePath& root_path,
      const std::vector<base::FilePath>& excluded_paths,
      Profile* profile,
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

  void set_image_processing_delay_for_testing(
      base::TimeDelta image_processing_delay) {
    image_processing_delay_for_test_ = image_processing_delay;
  }

 private:
  void OnFileChange(const base::FilePath& path, bool error);

  // Processes the items from the `files_to_process_` queue. Do it in a
  // non-recursive way as recursion can lead to one stack frame per file and
  // result in chrome crash if there a long list of non-image files in the
  // queue.
  void ProcessItems();

  // This function should be called from the image processing callbacks, and
  // item processing has stopped at this time. Restarts the item processing from
  // the `files_to_process_` queue if the `file_path` matches the head of the
  // queue.
  // If `timeout_timer_` has started, sets `use_timer` to true and it will also
  // stop it if the `file_path` matches the head of the queue. Image processing
  // callback can return after `timeout_timer_` gets timeout, which starts a new
  // sequence. It results in multiple sequences executing on a single queue with
  // certain files get skipped and certain files are computed multiple times.
  // Thus, we should check if the callback is still up-to-date before we restart
  // the item processing.
  void MaybeProcessNextItem(const base::FilePath& file_path,
                            bool use_timer = false);

  // Processes the next directory from the `files_to_process_` queue.
  void ProcessNextDirectory();

  // Processes the next image from the `files_to_process_` queue. Return true if
  // the image needs to be decoded, and return false otherwise.
  bool ProcessNextImage();

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

  void OnImageProcessTimeout(const base::FilePath& file_path);

  std::unique_ptr<base::FilePathWatcher> file_watcher_;
  base::FilePath root_path_;
  // Excludes any path matching the prefixes.
  std::vector<base::FilePath> excluded_paths_;

  base::FilePathWatcher::Callback on_file_change_callback_;

  // AnnotationStorage owns this ImageAnnotationWorker. All the methods must
  // be called from the main sequence.
  raw_ptr<AnnotationStorage, DanglingUntriaged> annotation_storage_;

  // ML models used as DLCs.
  ImageContentAnnotator image_content_annotator_;
  scoped_refptr<screen_ai::OpticalCharacterRecognizer>
      optical_character_recognizer_;

  const bool use_file_watchers_;
  const bool use_ica_;
  const bool use_ocr_;
  base::queue<base::FilePath> files_to_process_;
  int num_retries_passed_ = 0;

  // Indexing limit params.
  const int indexing_limit_;
  int num_indexing_images_ = 0;

  // Fake delay for image processing callback. Used in tests only.
  std::optional<base::TimeDelta> image_processing_delay_for_test_ =
      std::nullopt;

  base::OneShotTimer timeout_timer_;
  base::TimeTicks queue_processing_start_time_;
  // Owned by this class.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ImageAnnotationWorker> weak_ptr_factory_{this};
};
}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_IMAGE_ANNOTATION_WORKER_H_
