// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/local_image_search/image_content_annotator.h"

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"

namespace app_list {
namespace {

using ImageAnnotationResultPtr =
    ::chromeos::machine_learning::mojom::ImageAnnotationResultPtr;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class Status {
  kOk = 0,
  kModelSpecError = 1,
  kLoadModelError = 2,
  kFeatureNotSupportedError = 3,
  kLanguageNotSupportedError = 4,
  kMaxValue = kLanguageNotSupportedError,
};

void LogStatusUma(Status status) {
  base::UmaHistogramEnumeration(
      "Apps.AppList.AnnotationStorage.ImageAnnotationWorker."
      "ImageContentAnnotator.Status",
      status);
}

}  // namespace

ImageContentAnnotator::ImageContentAnnotator() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ImageContentAnnotator::~ImageContentAnnotator() = default;

bool ImageContentAnnotator::IsDlcInitialized() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ica_dlc_initialized_;
}

void ImageContentAnnotator::EnsureAnnotatorIsConnected() {
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

void ImageContentAnnotator::ConnectToImageAnnotator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto config = chromeos::machine_learning::mojom::ImageAnnotatorConfig::New();
  config->locale = "en-US";

  DVLOG(1) << "Binding ICA.";
  ml_service_->LoadImageAnnotator(
      std::move(config), image_content_annotator_.BindNewPipeAndPassReceiver(),
      base::BindOnce(
          [](bool* ica_dlc_initialized, int* num_retries_passed,
             const chromeos::machine_learning::mojom::LoadModelResult result) {
            DVLOG(1) << result;
            if (result ==
                chromeos::machine_learning::mojom::LoadModelResult::OK) {
              DVLOG(1) << "ICA bind is done.";
              LogStatusUma(Status::kOk);
              // Tracks how many retries have been done before the DLC is ready.
              base::UmaHistogramExactLinear(
                  "Apps.AppList.AnnotationStorage.ImageAnnotationWorker."
                  "ImageContentAnnotator.NumberOfRetries",
                  *num_retries_passed, 20);
              *ica_dlc_initialized = true;
              return;
            } else if (result == chromeos::machine_learning::mojom::
                                     LoadModelResult::MODEL_SPEC_ERROR) {
              LOG(ERROR) << "Model spec error.";
              LogStatusUma(Status::kModelSpecError);
            } else if (result == chromeos::machine_learning::mojom::
                                     LoadModelResult::LOAD_MODEL_ERROR) {
              LOG(ERROR) << "Load model error.";
              LogStatusUma(Status::kLoadModelError);
            } else if (result ==
                       chromeos::machine_learning::mojom::LoadModelResult::
                           LANGUAGE_NOT_SUPPORTED_ERROR) {
              LOG(ERROR) << "Language not supported error.";
              LogStatusUma(Status::kLanguageNotSupportedError);
            } else {
              NOTREACHED_IN_MIGRATION()
                  << "Implement logging for new error codes.";
            }
            *ica_dlc_initialized = false;
          },
          &ica_dlc_initialized_, &num_retries_passed_));
}

void ImageContentAnnotator::DisconnectAnnotator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  image_content_annotator_.reset();
}

void ImageContentAnnotator::AnnotateEncodedImage(
    const base::FilePath& image_path,
    base::OnceCallback<void(ImageAnnotationResultPtr)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Making a MemoryMappedFile.";
  base::MemoryMappedFile data;
  if (!data.Initialize(image_path)) {
    LOG(ERROR) << "Could not create a memory mapped file for an "
                  "image file to generate annotations";
  }
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(data.length());
  // It's safe to early return here as the caller function
  // `ImageAnnotationWorker::OnDecodeImageFile()` has started the
  // `timeout_timer_` and it will continue the process when the timer gets
  // timeout.
  if (!mapped_region.IsValid()) {
    return;
  }
  base::span(mapped_region.mapping).copy_from(data.bytes());

  EnsureAnnotatorIsConnected();
  image_content_annotator_->AnnotateEncodedImage(
      std::move(mapped_region.region), std::move(callback));
}

}  // namespace app_list
