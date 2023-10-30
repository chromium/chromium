// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/image_annotation_worker.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/image_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/local_image_search/annotation_storage.h"
#include "chrome/browser/ash/app_list/search/local_image_search/search_utils.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace app_list {
namespace {

using TokenizedString = ::ash::string_matching::TokenizedString;
using Mode = ::ash::string_matching::TokenizedString::Mode;

constexpr int kMaxFileSizeBytes = 2e+7;    // ~ 20MiB
constexpr int kConfidenceThreshold = 128;  // 50% of 255 (max of ICA)
constexpr int kOcrMinWordLength = 3;
constexpr base::TimeDelta kInitialIndexingDelay = base::Seconds(1);

// Exclude animated WebPs.
bool IsStaticWebp(const base::FilePath& path) {
  std::ifstream file(path.value(), std::ios::binary);
  if (!file) {
    LOG(ERROR) << "Unable to open file: " << path;
    return false;
  }

  char buffer[30];
  file.read(buffer, sizeof(buffer));
  file.close();

  // Checking for RIFF header and WebP identifier as in the
  // https://developers.google.com/speed/webp/docs/riff_container
  if (std::string(buffer, 4) == "RIFF" &&
      std::string(buffer + 8, 4) == "WEBP") {
    // Checking the VP8X chunk for animation
    if (std::string(buffer + 12, 4) == "VP8X") {
      // VP8X header is 8 bytes then the flags byte.
      const char flags = buffer[20];
      // The second bit indicates if it's animated.
      return !static_cast<bool>(flags & 0x02);
    }

    return true;
  }

  return false;
}

bool IsJpeg(const base::FilePath& path) {
  std::ifstream file(path.value(), std::ios::binary);
  if (!file) {
    LOG(ERROR) << "Unable to open file: " << path;
    return false;
  }

  char buffer[4];
  file.read(buffer, sizeof(buffer));
  file.close();

  // Check for JPEG magic numbers
  return (buffer[0] == (char)0xFF && buffer[1] == (char)0xD8 &&
          buffer[2] == (char)0xFF &&
          (buffer[3] == (char)0xE0 || buffer[3] == (char)0xE1));
}

bool IsPng(const base::FilePath& path) {
  std::ifstream file(path.value(), std::ios::binary);
  if (!file) {
    LOG(ERROR) << "Unable to open file: " << path;
    return false;
  }

  uint8_t buffer[8];
  file.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
  file.close();

  const uint8_t pngSignature[8] = {0x89, 0x50, 0x4E, 0x47,
                                   0x0D, 0x0A, 0x1A, 0x0A};
  for (int i = 0; i < 8; ++i) {
    if (buffer[i] != pngSignature[i]) {
      return false;
    }
  }

  return true;
}

// Checks for supported extensions.
bool IsImage(const base::FilePath& path) {
  DVLOG(1) << "IsImage? " << path.Extension();
  const std::string extension = base::ToLowerASCII(path.Extension());
  // Note: The UI design stipulates jpg, png, gif, and svg, but we use
  // the subset that ICA can handle
  return extension == ".jpeg" || extension == ".jpg" || extension == ".png" ||
         extension == ".webp";
}

// Check headers for correctness.
bool IsSupportedImage(const base::FilePath& path) {
  DVLOG(1) << "IsSupportedImage? " << path.Extension();
  const std::string extension = base::ToLowerASCII(path.Extension());
  if (extension == ".jpeg" || extension == ".jpg") {
    return IsJpeg(path);
  } else if (extension == ".png") {
    return IsPng(path);
  } else if (extension == ".webp") {
    return IsStaticWebp(path);
  } else {
    return false;
  }
}

bool IsPathExcluded(const base::FilePath& path,
                    const std::vector<base::FilePath>& excluded_paths) {
  DVLOG(1) << "IsPathExcluded: " << path;
  return std::any_of(excluded_paths.begin(), excluded_paths.end(),
                     [&path](const base::FilePath& prefix) {
                       return base::StartsWith(path.value(), prefix.value(),
                                               base::CompareCase::SENSITIVE);
                     });
}

}  // namespace

ImageAnnotationWorker::ImageAnnotationWorker(
    const base::FilePath& root_path,
    const std::vector<base::FilePath>& excluded_paths,
    bool use_file_watchers,
    bool use_ocr,
    bool use_ica)
    : root_path_(root_path),
      excluded_paths_(excluded_paths),
      use_file_watchers_(use_file_watchers),
      use_ica_(use_ica),
      use_ocr_(use_ocr),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ImageAnnotationWorker::~ImageAnnotationWorker() = default;

void ImageAnnotationWorker::Initialize(AnnotationStorage* annotation_storage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(annotation_storage);
  annotation_storage_ = annotation_storage;

  on_file_change_callback_ = base::BindRepeating(
      &ImageAnnotationWorker::OnFileChange, weak_ptr_factory_.GetWeakPtr());

  if (use_ocr_) {
    DVLOG(1) << "Initializing OCR DLC.";
    optical_character_recognizer_.InitializeComponent();
  }

  if (use_ica_) {
    DVLOG(1) << "Initializing ICA DLC.";
    image_content_annotator_.EnsureAnnotatorIsConnected();
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ImageAnnotationWorker::OnDlcInstalled,
                     weak_ptr_factory_.GetWeakPtr()),
      kInitialIndexingDelay);
}

void ImageAnnotationWorker::OnDlcInstalled() {
  bool is_ica_dlc_installed = image_content_annotator_.IsDlcInitialized();
  bool is_ocr_dlc_installed = optical_character_recognizer_.IsServiceReady();
  if ((use_ocr_ && !is_ocr_dlc_installed) ||
      (use_ica_ && !is_ica_dlc_installed)) {
    DVLOG(1) << "DLCs are not ready. OCR: " << is_ocr_dlc_installed << "/"
             << use_ocr_ << " ICA: " << is_ica_dlc_installed << "/" << use_ica_
             << ". Waiting.";
    // It is expected to be ready on a first try. Also, it is not a time
    // sensitive task, so we do not need to implement a full-fledged observer.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ImageAnnotationWorker::OnDlcInstalled,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(1));
    return;
  }

  if (use_file_watchers_) {
    DVLOG(1) << "DLCs are ready. Watching for file changes: " << root_path_;
    file_watcher_ = std::make_unique<base::FilePathWatcher>();

    // `file_watcher_` needs to be deleted in the same sequence it was
    // initialized.
    file_watcher_->WatchWithOptions(
        root_path_,
        {.type = base::FilePathWatcher::Type::kRecursive,
         .report_modified_path = true},
        on_file_change_callback_);
  }

  OnFileChange(root_path_, /*error=*/false);
  FindAndRemoveDeletedFiles(annotation_storage_->GetAllFiles());
}

void ImageAnnotationWorker::OnFileChange(const base::FilePath& path,
                                         bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "OnFileChange: " << path;
  if (error || IsPathExcluded(path, excluded_paths_)) {
    DVLOG(1) << "Skipping.";
    return;
  }

  DVLOG(1) << "Adding to a queue";
  files_to_process_.push(std::move(path));
  if (files_to_process_.size() == 1) {
    return ProcessNextItem();
  }
  return;
}

void ImageAnnotationWorker::ProcessNextItem() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (files_to_process_.empty()) {
    DVLOG(1) << "The queue is empty.";
    image_content_annotator_.DisconnectAnnotator();
    optical_character_recognizer_.DisconnectAnnotator();
    return;
  }

  const base::FilePath path = files_to_process_.front();
  DVLOG(1) << "ProcessNextItem " << path;
  if (base::DirectoryExists(path)) {
    return ProcessNextDirectory();
  } else if (IsImage(path)) {
    return ProcessNextImage();
  } else if (!base::PathExists(path)) {
    return RemoveOldDirectory();
  } else {
    files_to_process_.pop();
    return ProcessNextItem();
  }
}

void ImageAnnotationWorker::ProcessNextDirectory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath directory_path = files_to_process_.front();
  DVLOG(1) << "ProcessNextDirectory " << directory_path;

  // We need to re-index all the files in the directory.
  auto file_enumerator = base::FileEnumerator(
      directory_path,
      /*recursive=*/false, base::FileEnumerator::NAMES_ONLY, "*",
      base::FileEnumerator::FolderSearchPolicy::ALL);

  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    DVLOG(1) << "Found file: " << file_path;
    OnFileChange(std::move(file_path), /*error=*/false);
  }
  files_to_process_.pop();
  return ProcessNextItem();
}

void ImageAnnotationWorker::RemoveOldDirectory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath directory_path = files_to_process_.front();
  DVLOG(1) << "RemoveOldDirectory " << directory_path;

  auto files = annotation_storage_->SearchByDirectory(directory_path);
  for (const auto& file : files) {
    OnFileChange(file, /*error=*/false);
  }

  files_to_process_.pop();
  return ProcessNextItem();
}

void ImageAnnotationWorker::ProcessNextImage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::FilePath image_path = files_to_process_.front();
  DVLOG(1) << "ProcessNextImage " << image_path;

  auto file_info = std::make_unique<base::File::Info>();
  if (!base::GetFileInfo(image_path, file_info.get()) || file_info->size == 0 ||
      file_info->size > kMaxFileSizeBytes || !IsSupportedImage(image_path)) {
    annotation_storage_->Remove(image_path);
    files_to_process_.pop();
    return ProcessNextItem();
  }
  DCHECK(file_info);

  auto stored_annotations = annotation_storage_->FindImagePath(image_path);
  if (!stored_annotations.empty()) {
    DVLOG(1) << "CompareModifiedTime: " << stored_annotations.size()
             << " same? "
             << (file_info->last_modified ==
                 stored_annotations.front().last_modified);
    // Annotations are updated on a file change and have the file's last
    // modified time. So skip inserting the image annotations if the file
    // has not changed since the last update.
    if (file_info->last_modified == stored_annotations.front().last_modified) {
      files_to_process_.pop();
      return ProcessNextItem();
    }
  }

  DVLOG(1) << "Processing new " << image_path << " "
           << file_info->last_modified;
  annotation_storage_->Remove(image_path);
  ImageInfo image_info({}, image_path, file_info->last_modified,
                       file_info->size);

  if (use_ocr_ || use_ica_) {
    ash::image_util::DecodeImageFile(
        base::BindOnce(&ImageAnnotationWorker::OnDecodeImageFile,
                       weak_ptr_factory_.GetWeakPtr(), image_info),
        image_info.path);
  } else {
    RunFakeImageAnnotator(std::move(image_info));
  }
}

void ImageAnnotationWorker::OnDecodeImageFile(
    ImageInfo image_info,
    const gfx::ImageSkia& image_skia) {
  DVLOG(1) << "OnDecodeImageFile. Is decoded " << !image_skia.size().IsEmpty();
  if (use_ocr_ && use_ica_) {
    optical_character_recognizer_.ReadImage(
        *image_skia.bitmap(),
        base::BindOnce(&ImageAnnotationWorker::OnPerformOcr,
                       weak_ptr_factory_.GetWeakPtr(), image_info)
            .Then(base::BindOnce(
                &ImageContentAnnotator::AnnotateEncodedImage,
                base::Unretained(&image_content_annotator_), image_info.path,
                base::BindOnce(&ImageAnnotationWorker::OnPerformIca,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(image_info)))));
    return;
  }

  if (use_ocr_) {
    optical_character_recognizer_.ReadImage(
        *image_skia.bitmap(),
        base::BindOnce(&ImageAnnotationWorker::OnPerformOcr,
                       weak_ptr_factory_.GetWeakPtr(), std::move(image_info)));
    return;
  }

  if (use_ica_) {
    image_content_annotator_.AnnotateEncodedImage(
        image_info.path,
        base::BindOnce(&ImageAnnotationWorker::OnPerformIca,
                       weak_ptr_factory_.GetWeakPtr(), std::move(image_info)));
    return;
  }
  NOTREACHED();
}

void ImageAnnotationWorker::OnPerformOcr(
    ImageInfo image_info,
    screen_ai::mojom::VisualAnnotationPtr visual_annotation) {
  DVLOG(1) << "OnPerformOcr";
  for (const auto& text_line : visual_annotation->lines) {
    TokenizedString tokens(base::UTF8ToUTF16(text_line->text_line),
                           Mode::kWords);
    for (const auto& word : tokens.tokens()) {
      std::string lower_case_word = base::UTF16ToUTF8(word);
      if (word.size() >= kOcrMinWordLength && !IsStopWord(lower_case_word) &&
          base::IsAsciiAlpha(lower_case_word[0])) {
        image_info.annotations.insert(std::move(lower_case_word));
      }
    }
  }
  if (!image_info.annotations.empty()) {
    annotation_storage_->Insert(std::move(image_info));
  }

  // OCR is the first in the pipeline.
  if (!use_ica_) {
    files_to_process_.pop();
    ProcessNextItem();
  }
}

void ImageAnnotationWorker::OnPerformIca(
    ImageInfo image_info,
    chromeos::machine_learning::mojom::ImageAnnotationResultPtr ptr) {
  DVLOG(1) << "OnPerformIca. Status: " << ptr->status
           << " Size: " << ptr->annotations.size();
  for (const auto& a : ptr->annotations) {
    if (a->confidence < kConfidenceThreshold || !a->name.has_value() ||
        a->name->empty()) {
      continue;
    }

    TokenizedString tokens(base::UTF8ToUTF16(a->name.value()), Mode::kWords);
    for (const auto& word : tokens.tokens()) {
      DVLOG(1) << "Id: " << a->id << " MId: " << a->mid
               << " Confidence: " << (int)a->confidence << " Name: " << word;
      image_info.annotations.insert(base::UTF16ToUTF8(word));
    }
  }
  if (!image_info.annotations.empty()) {
    annotation_storage_->Insert(image_info);
  }

  // ICA is the last in the pipeline.
  files_to_process_.pop();
  ProcessNextItem();
}

void ImageAnnotationWorker::FindAndRemoveDeletedFiles(
    const std::vector<base::FilePath> files) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "FindAndRemoveDeletedImages.";
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const std::vector<base::FilePath>& files) {
            std::vector<base::FilePath> deleted_paths;
            for (const auto& file_path : files) {
              if (!base::PathExists(file_path)) {
                deleted_paths.push_back(file_path);
              }
            }
            return deleted_paths;
          },
          std::move(files)),
      base::BindOnce(
          [](AnnotationStorage* const annotation_storage,
             std::vector<base::FilePath> paths) {
            std::for_each(paths.begin(), paths.end(),
                          [&](auto path) { annotation_storage->Remove(path); });
          },
          annotation_storage_));
}

void ImageAnnotationWorker::RunFakeImageAnnotator(ImageInfo image_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Run FilePathAnnotator.";
  const std::string annotation =
      image_info.path.BaseName().RemoveFinalExtension().value();
  image_info.annotations.insert(std::move(annotation));
  annotation_storage_->Insert(std::move(image_info));
  files_to_process_.pop();
  ProcessNextItem();
}

void ImageAnnotationWorker::TriggerOnFileChangeForTests(
    const base::FilePath& path,
    bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_file_change_callback_.Run(path, error);
}

}  // namespace app_list
