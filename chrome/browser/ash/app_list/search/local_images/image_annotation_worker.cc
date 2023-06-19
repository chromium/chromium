// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_images/image_annotation_worker.h"

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "ash/public/cpp/image_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/local_images/annotation_storage.h"
#include "chrome/browser/ash/app_list/search/local_images/search_utils.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/image_content_annotation.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "content/public/browser/browser_thread.h"

namespace app_list {
namespace {

// ~ 20MiB
constexpr int kMaxFileSizeBytes = 2e+7;
constexpr int kConfidenceThreshold = 128;  // 50% of 255 (max of ICA)
constexpr base::TimeDelta kInitialIndexingDelay = base::Seconds(1);

bool IsImage(const base::FilePath& path) {
  DVLOG(1) << "IsImage? " << path.Extension();
  const std::string extension = path.Extension();
  // Note: The UI design stipulates jpg, png, gif, and svg, but we use
  // the subset that ICA can handle.
  return extension == ".jpeg" || extension == ".jpg" || extension == ".png" ||
         extension == ".JPEG" || extension == ".JPG" || extension == ".PNG";
}

// Returns deleted files. Needs to be done in background.
std::set<base::FilePath> GetDeletedPaths(const std::vector<ImageInfo>& images) {
  std::set<base::FilePath> deleted_paths;
  for (const auto& image : images) {
    if (!base::PathExists(image.path)) {
      deleted_paths.insert(image.path);
    }
  }
  return deleted_paths;
}

bool IsOcrServiceReady() {
  return (
      screen_ai::ScreenAIInstallState::GetInstance() &&
      screen_ai::ScreenAIInstallState::GetInstance()->IsComponentAvailable());
}

}  // namespace

ImageAnnotationWorker::ImageAnnotationWorker(const base::FilePath& root_path,
                                             bool use_ocr,
                                             bool use_ica)
    : root_path_(root_path),
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

  VLOG(1) << "Initializing DLCs.";
  if (use_ocr_) {
    DVLOG(1) << "Initializing OCR DLC.";
    if (IsOcrServiceReady()) {
      EnsureOcrAnnotatorIsConnected();
    } else {
      // DLC downloader cannot run from current sequence.
      content::GetUIThreadTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce([]() {
            // Screen AI Install State may be unavailable for tests.
            if (screen_ai::ScreenAIInstallState::GetInstance()) {
              screen_ai::ScreenAIInstallState::GetInstance()
                  ->DownloadComponent();
            }
          }));
    }
  }

  if (use_ica_) {
    DVLOG(1) << "Initializing ICA DLC.";
    EnsureIcaAnnotatorIsConnected();
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ImageAnnotationWorker::OnDlcInstalled,
                     weak_ptr_factory_.GetWeakPtr()),
      kInitialIndexingDelay);
}

void ImageAnnotationWorker::OnDlcInstalled() {
  bool ocr_dlc_installed = IsOcrServiceReady();
  if ((use_ocr_ && !ocr_dlc_installed) || (use_ica_ && !ica_dlc_initialized_)) {
    DVLOG(1) << "DLC is not ready. OCR: " << ocr_dlc_installed << "/"
             << use_ocr_ << " ICA: " << ica_dlc_initialized_ << "/" << use_ica_
             << " Waiting.";
    // It is expected to be ready on a first try. Also, it is not a time
    // sensitive task, so we do not need to implement a full-fledged observer.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ImageAnnotationWorker::OnDlcInstalled,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(1));
    return;
  }

  if (use_ica_ || use_ocr_) {
    VLOG(1) << "DLCs are ready. Watching for file changes.";
    file_watcher_ = std::make_unique<base::FilePathWatcher>();

    DVLOG(1) << "Start WatchWithOptions " << root_path_;
    // `file_watcher_` needs to be deleted in the same sequence it was
    // initialized.
    file_watcher_->WatchWithOptions(
        root_path_,
        {.type = base::FilePathWatcher::Type::kRecursive,
         .report_modified_path = true},
        on_file_change_callback_);
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePath root_path)
              -> std::unique_ptr<base::FileEnumerator> {
            DVLOG(1) << "Commencing start up indexing. ";
            return std::make_unique<base::FileEnumerator>(
                root_path,
                /*recursive=*/true, base::FileEnumerator::FILES,
                // There is an image extension test down the pipe.
                "*.[j,p,J,P][p,n,P,N]*[g,G]",
                base::FileEnumerator::FolderSearchPolicy::ALL);
          },
          root_path_),
      base::BindOnce(
          [](base::FilePathWatcher::Callback on_file_change_callback,
             std::unique_ptr<base::FileEnumerator> file_enumerator) {
            for (base::FilePath file = file_enumerator->Next(); !file.empty();
                 file = file_enumerator->Next()) {
              DVLOG(1) << "Found files: " << file;
              on_file_change_callback.Run(std::move(file), /*error=*/false);
            }
          },
          on_file_change_callback_));

  FindAndRemoveDeletedImages(annotation_storage_->GetAllAnnotations());
}

void ImageAnnotationWorker::EnsureIcaAnnotatorIsConnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ml_service_.is_bound() && image_content_annotator_.is_bound()) {
    return;
  }

  if (!ml_service_.is_bound()) {
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->BindMachineLearningService(ml_service_.BindNewPipeAndPassReceiver());
    ml_service_.reset_on_disconnect();
  }

  if (!image_content_annotator_.is_bound()) {
    ConnectToImageAnnotator();
    image_content_annotator_.reset_on_disconnect();
  }
}

void ImageAnnotationWorker::EnsureOcrAnnotatorIsConnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (screen_ai_annotator_.is_bound()) {
    return;
  }

  DCHECK(IsOcrServiceReady());
  screen_ai_service_router_.BindScreenAIAnnotator(
      screen_ai_annotator_.BindNewPipeAndPassReceiver());
  screen_ai_annotator_.reset_on_disconnect();
}

void ImageAnnotationWorker::ConnectToImageAnnotator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto config = chromeos::machine_learning::mojom::ImageAnnotatorConfig::New();
  config->locale = "en-US";

  DVLOG(1) << "Binding ICA.";
  ml_service_->LoadImageAnnotator(
      std::move(config), image_content_annotator_.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](bool* ica_dlc_initialized,
             const chromeos::machine_learning::mojom::LoadModelResult result) {
            DVLOG(1) << result;
            if (result ==
                chromeos::machine_learning::mojom::LoadModelResult::OK) {
              *ica_dlc_initialized = true;
              DVLOG(1) << "ICA bind is done.";
            } else {
              LOG(ERROR) << "Failed to bind ICA.";
              *ica_dlc_initialized = false;
            }
          },
          &ica_dlc_initialized_));
}

void ImageAnnotationWorker::OnFileChange(const base::FilePath& path,
                                         bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (DirectoryExists(path) || !IsImage(path) || error) {
    return;
  }

  auto file_info = std::make_unique<base::File::Info>();
  if (!base::GetFileInfo(path, file_info.get())) {
    annotation_storage_->Remove(path);
    return;
  }

  DCHECK(file_info);

  // Ignore images bigger than the threshold.
  if (file_info->size > kMaxFileSizeBytes) {
    // TODO(b/260646344): Add a histogram for file sizes.
    return;
  }

  if (file_info->size == 0) {
    annotation_storage_->Remove(path);
    return;
  }

  auto stored_annotations = annotation_storage_->FindImagePath(path);
  ProcessImage(path, std::move(file_info), std::move(stored_annotations));
}

void ImageAnnotationWorker::ProcessImage(
    base::FilePath image_path,
    std::unique_ptr<base::File::Info> file_info,
    std::vector<ImageInfo> stored_annotations_with_this_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stored_annotations_with_this_path.empty()) {
    DVLOG(1) << "CompareModifiedTime: "
             << stored_annotations_with_this_path.size() << " same? "
             << (file_info->last_modified ==
                 stored_annotations_with_this_path[0].last_modified)
             << " is_ignored: "
             << stored_annotations_with_this_path[0].is_ignored;
    // Annotations are updated on a file change and have the file's last
    // modified time. So skip inserting the image annotations if the file
    // has not changed since the last update.
    if (stored_annotations_with_this_path[0].is_ignored ||
        file_info->last_modified ==
            stored_annotations_with_this_path[0].last_modified) {
      return;
    }
  }

  DVLOG(1) << "Processing new " << image_path << " "
           << file_info->last_modified;
  ImageInfo image_info({}, image_path, file_info->last_modified,
                       /*is_ignored=*/0);

  auto callback =
      use_ica_ || use_ocr_
          ? base::BindOnce(&ImageAnnotationWorker::RunImageAnnotator,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(image_info))
          : base::BindOnce(&ImageAnnotationWorker::RunFakeImageAnnotator,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(image_info));

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](base::FilePath image_path) -> base::MappedReadOnlyRegion {
            DVLOG(1) << "Making a MemoryMappedFile.";
            base::MemoryMappedFile data;
            if (!data.Initialize(image_path)) {
              LOG(ERROR) << "Could not create a memory mapped file for an "
                            "image file to generate annotations";
            }
            base::MappedReadOnlyRegion mapped_region =
                base::ReadOnlySharedMemoryRegion::Create(data.length());
            memcpy(mapped_region.mapping.memory(), data.data(), data.length());
            DCHECK(mapped_region.IsValid());
            DCHECK(mapped_region.region.IsValid());
            return mapped_region;
          },
          image_path),
      std::move(callback));
}

void ImageAnnotationWorker::RunImageAnnotator(
    ImageInfo image_info,
    base::MappedReadOnlyRegion mapped_region) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(mapped_region.IsValid());
  DCHECK(mapped_region.region.IsValid());

  annotation_storage_->Remove(image_info.path);

  if (use_ocr_) {
    EnsureOcrAnnotatorIsConnected();
    ash::image_util::DecodeImageFile(
        base::BindOnce(&ImageAnnotationWorker::OnDecodeImageFile,
                       weak_ptr_factory_.GetWeakPtr(), image_info),
        image_info.path);
  }

  if (use_ica_) {
    EnsureIcaAnnotatorIsConnected();
    image_content_annotator_->AnnotateEncodedImage(
        std::move(mapped_region.region),
        base::BindOnce(&ImageAnnotationWorker::OnPerformIca,
                       weak_ptr_factory_.GetWeakPtr(), std::move(image_info)));
  }
}

void ImageAnnotationWorker::OnPerformIca(
    ImageInfo image_info,
    chromeos::machine_learning::mojom::ImageAnnotationResultPtr ptr) {
  DVLOG(1) << "Status: " << ptr->status << " Size: " << ptr->annotations.size();
  for (const auto& a : ptr->annotations) {
    if (a->confidence < kConfidenceThreshold) {
      break;
    }
    DVLOG(1) << "Id: " << a->id << " MId: " << a->mid
             << " Confidence: " << (int)a->confidence
             << " Name: " << a->name.value_or("null");
    if (a->name.has_value() && !a->name->empty()) {
      image_info.annotations.insert(a->name.value());
    }
  }
  if (!image_info.annotations.empty()) {
    annotation_storage_->Insert(image_info);
  }
}

void ImageAnnotationWorker::FindAndRemoveDeletedImages(
    const std::vector<ImageInfo> images) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "FindAndRemoveDeletedImages.";
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetDeletedPaths, std::move(images)),
      base::BindOnce(
          [](AnnotationStorage* const annotation_storage,
             std::set<base::FilePath> paths) {
            std::for_each(paths.begin(), paths.end(),
                          [&](auto path) { annotation_storage->Remove(path); });
          },
          annotation_storage_));
}

void ImageAnnotationWorker::OnDecodeImageFile(
    ImageInfo image_info,
    const gfx::ImageSkia& image_skia) {
  DVLOG(1) << "Is decoded " << !image_skia.size().IsEmpty();
  screen_ai_annotator_->PerformOcrAndReturnAnnotation(
      *image_skia.bitmap(),
      base::BindOnce(&ImageAnnotationWorker::OnPerformOcr,
                     weak_ptr_factory_.GetWeakPtr(), std::move(image_info)));
}

void ImageAnnotationWorker::OnPerformOcr(
    ImageInfo image_info,
    screen_ai::mojom::VisualAnnotationPtr visual_annotation) {
  DVLOG(1) << "OnPerformOcr";
  for (const auto& text_line : visual_annotation->lines) {
    for (const auto& word : text_line->words) {
      DVLOG(1) << word->word;
      auto lower_case_word = base::ToLowerASCII(word->word);
      if (lower_case_word.size() > 3 && !IsStopWord(lower_case_word) &&
          base::IsAsciiAlpha(lower_case_word[0])) {
        image_info.annotations.insert(std::move(lower_case_word));
      }
    }
  }
  if (!image_info.annotations.empty()) {
    annotation_storage_->Insert(image_info);
  }
}

void ImageAnnotationWorker::RunFakeImageAnnotator(
    ImageInfo image_info,
    base::MappedReadOnlyRegion mapped_region) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::string annotation =
      image_info.path.BaseName().RemoveFinalExtension().value();
  image_info.annotations.insert(annotation);
  annotation_storage_->Remove(image_info.path);
  annotation_storage_->Insert(image_info);
}

void ImageAnnotationWorker::TriggerOnFileChangeForTests(
    const base::FilePath& path,
    bool error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_file_change_callback_.Run(path, error);
}

}  // namespace app_list
