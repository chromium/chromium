// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_managed_photo_controller.h"

#include <utility>
#include <vector>

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
  if (is_active_) {
    LOG(ERROR) << "AmbientManagedPhotoController is already active. Ignoring "
                  "StartScreenUpdate().";
    return;
  }

  is_active_ = true;
  LoadImages();
}

void AmbientManagedPhotoController::UpdateImageFilePaths(
    const std::vector<base::FilePath>& images) {
  if (images.size() < kMinImagesRequired) {
    // TODO(b/269579804): Add Metrics
    // TODO(b/175142676): Consider stopping managed screensaver if started
    // with an insufficient no of images.

    LOG(WARNING) << "AmbientManagedPhotoController updated with an "
                    "insufficient number of images.";
    return;
  }
  images_file_paths_ = images;
  if (is_active_) {
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

void AmbientManagedPhotoController::StopScreenUpdate() {
  is_active_ = false;
  ambient_backend_model_.Clear();
  weak_factory_.InvalidateWeakPtrs();
  current_image_index_ = 0;
  images_file_paths_.clear();
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

  DVLOG(3) << "UI event " << marker << " triggering topic refresh";
  if (!is_active_) {
    LOG(DFATAL) << "Received unexpected UI marker " << marker
                << " while inactive";
    return;
  }
  LoadNextImage();
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
  for (size_t i = 0;
       i < ambient_backend_model_.photo_config().GetNumDecodedTopicsToBuffer();
       i++) {
    LoadNextImage();
  }
}

void AmbientManagedPhotoController::LoadNextImage() {
  CHECK(images_file_paths_.size() >= kMinImagesRequired);
  current_image_index_ = (current_image_index_ + 1) % images_file_paths_.size();
  image_util::DecodeImageFile(
      base::BindOnce(&AmbientManagedPhotoController::OnPhotoDecoded,
                     weak_factory_.GetWeakPtr()),
      images_file_paths_.at(current_image_index_),
      data_decoder::mojom::ImageCodec::kDefault);
}

void AmbientManagedPhotoController::OnPhotoDecoded(
    const gfx::ImageSkia& image) {
  DVLOG(3) << __func__;
  if (image.isNull()) {
    LOG(WARNING) << "Image decoding failed";
    // TODO(b/175142676): Improve behavior when photo decoding fails.
    return;
  }
  PhotoWithDetails detailed_photo;
  detailed_photo.photo = image;
  // Note: There is no surefire way of determining the orientation of the image
  // so for now we assume and document that all admin provided images are
  // landscape.
  detailed_photo.is_portrait = false;

  ambient_backend_model_.AddNextImage(std::move(detailed_photo));
}

}  // namespace ash
