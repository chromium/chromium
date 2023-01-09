// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_images/image_annotation_worker.h"

#include <memory>
#include <set>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/app_list/search/local_images/annotation_storage.h"

namespace app_list {
namespace {

bool IsImage(const base::FilePath& path) {
  DVLOG(1) << "IsImage? " << path.Extension();
  const std::string extension = path.Extension();
  // TODO(b/260646344): Decide on the supported extensions.
  return extension == ".jpeg" || extension == ".jpg" || extension == ".png";
}

// Check files for existence, so needs to be called on a blocking task runner.
std::set<base::FilePath> GetDeletedPaths(const std::vector<ImageInfo>& images) {
  std::set<base::FilePath> deleted_paths;
  for (const auto& image : images) {
    if (!base::PathExists(image.path)) {
      deleted_paths.insert(image.path);
    }
  }
  return deleted_paths;
}

using OnFileChangeCallback = base::RepeatingCallback<
    void(const base::FilePath&, bool, std::unique_ptr<base::File::Info>)>;

// Reposts file change callbacks run by a file watcher to task_runner,
// adapting callback arguments to provide more information about the file.
// Obtains file info, so needs to be called on a blocking task runner.
void RelayPathChangedCallback(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const OnFileChangeCallback& on_file_change_callback,
    const base::FilePath& path,
    bool error) {
  if (DirectoryExists(path) || !IsImage(path)) {
    return;
  }

  auto info = std::make_unique<base::File::Info>();
  const bool is_file_exist = base::GetFileInfo(path, info.get());

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(on_file_change_callback, path, is_file_exist,
                                std::move(info)));
}

// Setups a file watcher and lists all the images in the watched folder, so
// needs to be called on a blocking task runner.
void StartWatchOnWorkerThread(
    base::FilePathWatcher* watcher,
    base::FilePath watcher_root_path,
    const base::FilePathWatcher::Callback& on_file_change_callback) {
  DCHECK(watcher);
  DVLOG(1) << "Start WatchWithOptions";
  watcher->WatchWithOptions(watcher_root_path,
                            {.type = base::FilePathWatcher::Type::kRecursive,
                             .report_modified_path = true},
                            on_file_change_callback);

  // TODO(b/260646344): make it as a 10 sec delayed task if needed.
  base::FileEnumerator images(watcher_root_path,
                              /*recursive=*/true, base::FileEnumerator::FILES,
                              FILE_PATH_LITERAL("*.jpg"),
                              base::FileEnumerator::FolderSearchPolicy::ALL);

  for (base::FilePath file = images.Next(); !file.empty();
       file = images.Next()) {
    DVLOG(1) << "Found files: " << file;
    on_file_change_callback.Run(file, /*error=*/false);
  }
}

// Lets the `watcher` get out of scope.
void DeleteFileWatcher(std::unique_ptr<base::FilePathWatcher> watcher) {}

}  // namespace

ImageAnnotationWorker::ImageAnnotationWorker(const base::FilePath& root_path)
    : root_path_(root_path),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

ImageAnnotationWorker::~ImageAnnotationWorker() {
  // `file_watcher_` needs to be deleted in the same sequence it was created.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeleteFileWatcher, std::move(file_watcher_)));
}

void ImageAnnotationWorker::Run(AnnotationStorage* const annotation_storage) {
  DCHECK(annotation_storage);

  annotation_storage_ = annotation_storage;
  file_watcher_ = std::make_unique<base::FilePathWatcher>();
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &StartWatchOnWorkerThread, file_watcher_.get(),
          // TODO(b/260646344): change to `root_path_`
          base::FilePath("/root/test"),
          base::BindRepeating(
              &RelayPathChangedCallback,
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindRepeating(&ImageAnnotationWorker::OnFileChange,
                                  weak_ptr_factory_.GetWeakPtr()))),
      base::BindOnce(&ImageAnnotationWorker::CheckForDeletedImages,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageAnnotationWorker::CheckForDeletedImages() {
  annotation_storage_->GetAllAnnotationsAsync(
      base::BindOnce(&ImageAnnotationWorker::FindAndRemoveDeletedImages,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageAnnotationWorker::OnFileChange(
    const base::FilePath& path,
    bool is_exist,
    std::unique_ptr<base::File::Info> file_info) {
  if (!is_exist) {
    annotation_storage_->RemoveAsync(path);
    return;
  }

  DCHECK(file_info);
  if (file_info->size == 0) {
    annotation_storage_->RemoveAsync(path);
    return;
  }

  annotation_storage_->FindImagePathAsync(
      path, base::BindOnce(&ImageAnnotationWorker::ProcessImage,
                           weak_ptr_factory_.GetWeakPtr(), path,
                           std::move(file_info)));
}

void ImageAnnotationWorker::ProcessImage(
    base::FilePath image_path,
    std::unique_ptr<base::File::Info> file_info,
    std::vector<ImageInfo> stored_annotations_with_this_path) {
  if (!stored_annotations_with_this_path.empty()) {
    DVLOG(1) << "CompareModifiedTime: "
             << stored_annotations_with_this_path.size() << " same? "
             << (file_info->last_modified ==
                 stored_annotations_with_this_path[0].last_modified);
    // Annotations are updated on a file change and have the file's last
    // modified time. So skip inserting the image annotations if the file
    // has not changed since the last update.
    if (file_info->last_modified ==
        stored_annotations_with_this_path[0].last_modified) {
      return;
    }
  }

  DVLOG(1) << "Processing new " << image_path << " " << file_info->last_modified
           << " " << image_path.BaseName().RemoveFinalExtension();
  // TODO(b/260646344): use mojo::ica::GetLabel(path);
  const std::string annotation =
      image_path.BaseName().RemoveFinalExtension().value();
  ImageInfo image_info({annotation, "test_" + annotation}, image_path,
                       file_info->last_modified);

  // Annotations have many-to-many mapping to file paths, so it is easier to
  // remove and insert than replace.
  annotation_storage_->RemoveAsync(image_path);
  annotation_storage_->InsertOrReplaceAsync(image_info);
}

void ImageAnnotationWorker::FindAndRemoveDeletedImages(
    const std::vector<ImageInfo> images) {
  DVLOG(1) << "FindAndRemoveDeletedImages";
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetDeletedPaths, std::move(images)),
      base::BindOnce(&ImageAnnotationWorker::RemovePathsFromDb,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ImageAnnotationWorker::RemovePathsFromDb(
    const std::set<base::FilePath>& paths) {
  for (const auto& path : paths) {
    annotation_storage_->RemoveAsync(path);
  }
}

}  // namespace app_list
