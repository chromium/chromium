// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_managed_photo_controller.h"

#include <utility>
#include <vector>

#include "ash/ambient/metrics/managed_screensaver_metrics.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/public/cpp/image_util.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {
constexpr size_t kMinImagesRequired = 2u;

}  // namespace

AmbientManagedPhotoController::AmbientManagedPhotoController(
    AmbientViewDelegate& view_delegate,
    AmbientPhotoConfig photo_config)
    : ambient_backend_model_(std::move(photo_config)) {
  scoped_view_delegate_observation_.Observe(&view_delegate);
}

AmbientManagedPhotoController::~AmbientManagedPhotoController() = default;

void AmbientManagedPhotoController::StartScreenUpdate() {
  if (IsScreenUpdateActive()) {
    LOG(ERROR) << "AmbientManagedPhotoController is already active. Ignoring "
                  "StartScreenUpdate().";
    return;
  }

  is_active_ = true;
  image_attempt_no_ = 0;

  LoadImages();
}

void AmbientManagedPhotoController::UpdateImageFilePaths(
    const std::vector<base::FilePath>& images) {
  RecordManagedScreensaverImageCount(images.size());

  // Reset `error_state_` when a sufficient number of new images are received
  if (images.size() < kMinImagesRequired) {
    // TODO(b/269579804): Add Metrics
    SetErrorState(ErrorState::kInsufficientImages);
    return;
  }
  SetErrorState(ErrorState::kNone);

  images_file_paths_ = images;
  image_attempt_no_ = 0;

  if (IsScreenUpdateActive()) {
    // Invalidate the weak pointers to make sure that any in-flight decoding
    // operations become no-ops.
    weak_factory_.InvalidateWeakPtrs();
    current_image_index_ = 0;

    // Note: We do not clear the backend model here but rather just load
    // the next topic buffer size images from disk, this will automatically
    // fill the backend model with only the latest images.
    LoadImages();
  }
}

bool AmbientManagedPhotoController::HasScreenUpdateErrors() const {
  return error_state_ != ErrorState::kNone;
}

void AmbientManagedPhotoController::SetErrorState(ErrorState error_state) {
  if (error_state == error_state_) {
    return;
  }
  error_state_ = error_state;
  if (observer_) {
    observer_->OnErrorStateChanged();
  }
}

void AmbientManagedPhotoController::StopScreenUpdate() {
  is_active_ = false;
  images_file_paths_.clear();
  ambient_backend_model_.Clear();
  weak_factory_.InvalidateWeakPtrs();
  current_image_index_ = 0;
  image_attempt_no_ = 0;
}

bool AmbientManagedPhotoController::IsScreenUpdateActive() const {
  return is_active_;
}

void AmbientManagedPhotoController::OnMarkerHit(
    AmbientPhotoConfig::Marker marker) {
  if (!ambient_backend_model_.photo_config().refresh_topic_markers.contains(
          marker)) {
    DVLOG(3) << "UI event " << marker
             << " does not trigger a image refresh. Ignoring...";
    return;
  }
  if (error_state_ == ErrorState::kPhotoLoadFailure) {
    LOG(WARNING) << "Not loading the next image for the UI marker " << marker
                 << " as maximum photo loading attempts reached";
    return;
  }

  DVLOG(3) << "UI event " << marker << " triggering image load";
  if (!is_active_) {
    LOG(DFATAL) << "Received unexpected UI marker " << marker
                << " while inactive";
    return;
  }

  LoadImagesInternal(/*images_to_load=*/1, /*success=*/true);
}

void AmbientManagedPhotoController::LoadImages() {
  if (images_file_paths_.size() < kMinImagesRequired) {
    // TODO(b/269579804): Log metrics to detect if the images are not enough.
    LOG(WARNING)
        << "AmbientManagedPhotoController does not have enough images.";
    return;
  }

  // Initially load a total of backend model buffer size images in the backend
  // model. Note: This should not be lower than 2 because the photo view code
  // right now loads the current and the next image simultaneously, and in case
  // of a buffer size equal to 1 it never triggers the images ready callback.
  LoadImagesInternal(
      ambient_backend_model_.photo_config().GetNumDecodedTopicsToBuffer(),
      true);
}

void AmbientManagedPhotoController::LoadImagesInternal(size_t images_to_load,
                                                       bool success) {
  if (images_to_load == 0 || !success) {
    return;
  }
  // Use a partial function application to store the no of remaining images to
  // load.
  base::OnceCallback<void(bool)> done_callback = base::BindOnce(
      &AmbientManagedPhotoController::LoadImagesInternal,
      weak_factory_.GetWeakPtr(), /*images_to_load=*/images_to_load - 1);
  LoadNextImage(std::move(done_callback));
}

void AmbientManagedPhotoController::LoadNextImage(
    base::OnceCallback<void(bool success)> done_callback) {
  CHECK(images_file_paths_.size() >= kMinImagesRequired);
  image_attempt_no_++;
  current_image_index_ = (current_image_index_ + 1) % images_file_paths_.size();
  image_util::DecodeImageFile(
      base::BindOnce(&AmbientManagedPhotoController::OnPhotoDecoded,
                     weak_factory_.GetWeakPtr(), std::move(done_callback)),
      images_file_paths_.at(current_image_index_),
      data_decoder::mojom::ImageCodec::kDefault);
}

void AmbientManagedPhotoController::HandlePhotoDecodingFailure(
    base::OnceCallback<void(bool success)> done_callback) {
  if (image_attempt_no_ >= GetMaxImageAttempts()) {
    LOG(ERROR) << "Image decoding failed, no valid image was decoded";
    SetErrorState(ErrorState::kPhotoLoadFailure);
    std::move(done_callback).Run(false);
    return;
  }
  LOG(WARNING) << "Image decoding failed, attempting to load next image";
  LoadNextImage(std::move(done_callback));
}

void AmbientManagedPhotoController::OnPhotoDecoded(
    base::OnceCallback<void(bool success)> done_callback,
    const gfx::ImageSkia& image) {
  DVLOG(3) << __func__;
  if (image.isNull()) {
    HandlePhotoDecodingFailure(std::move(done_callback));
    return;
  }

  image_attempt_no_ = 0;
  PhotoWithDetails detailed_photo;
  detailed_photo.photo = image;
  // Note: There is no surefire way of determining the orientation of the image
  // so for now we assume and document that all admin provided images are
  // landscape.
  detailed_photo.is_portrait = false;

  ambient_backend_model_.AddNextImage(std::move(detailed_photo));

  // Notify the caller that photo loading was successful.
  std::move(done_callback).Run(true);
}

size_t AmbientManagedPhotoController::GetMaxImageAttempts() const {
  CHECK_GE(images_file_paths_.size(), kMinImagesRequired);
  return images_file_paths_.size() - 1;
}

void AmbientManagedPhotoController::SetObserver(Observer* observer) {
  CHECK(!observer_);
  observer_ = observer;
}

}  // namespace ash
