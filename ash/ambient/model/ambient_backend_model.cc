// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_backend_model.h"

#include <algorithm>
#include <random>
#include <utility>
#include <vector>

#include "ash/ambient/metrics/ambient_metrics.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/time/time.h"

namespace ash {

namespace {

// Note this does not start until the minimum number of topics required in the
// AmbientPhotoConfig is reached.
constexpr base::TimeDelta kImagesReadyTimeout = base::Seconds(10);

}  // namespace

// PhotoWithDetails------------------------------------------------------------
PhotoWithDetails::PhotoWithDetails() = default;

PhotoWithDetails::PhotoWithDetails(const PhotoWithDetails&) = default;

PhotoWithDetails& PhotoWithDetails::operator=(const PhotoWithDetails&) =
    default;

PhotoWithDetails::PhotoWithDetails(PhotoWithDetails&&) = default;

PhotoWithDetails& PhotoWithDetails::operator=(PhotoWithDetails&&) = default;

PhotoWithDetails::~PhotoWithDetails() = default;

void PhotoWithDetails::Clear() {
  photo = gfx::ImageSkia();
  details = std::string();
  related_photo = gfx::ImageSkia();
  related_details = std::string();
  is_portrait = false;
}

bool PhotoWithDetails::IsNull() const {
  return photo.isNull();
}

// AmbientBackendModel---------------------------------------------------------
AmbientBackendModel::AmbientBackendModel(AmbientPhotoConfig photo_config) {
  SetPhotoConfig(std::move(photo_config));
}

AmbientBackendModel::~AmbientBackendModel() = default;

void AmbientBackendModel::AddObserver(AmbientBackendModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AmbientBackendModel::RemoveObserver(
    AmbientBackendModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool AmbientBackendModel::ImagesReady() const {
  DCHECK_LE(all_decoded_topics_.size(),
            photo_config_.GetNumDecodedTopicsToBuffer());
  return all_decoded_topics_.size() ==
             photo_config_.GetNumDecodedTopicsToBuffer() ||
         images_ready_timed_out_;
}

void AmbientBackendModel::OnImagesReadyTimeoutFired() {
  if (ImagesReady())
    return;

  DCHECK_GE(all_decoded_topics_.size(),
            photo_config_.min_total_topics_required);
  // TODO(esum): Add metrics for how often this case happens.
  LOG(WARNING) << "Timed out trying to prepare "
               << photo_config_.GetNumDecodedTopicsToBuffer()
               << " topics. Starting UI with " << all_decoded_topics_.size();
  images_ready_timed_out_ = true;
  NotifyImagesReady();
}

void AmbientBackendModel::AddNextImage(
    const PhotoWithDetails& photo_with_details) {
  DCHECK(!photo_with_details.IsNull());
  DCHECK(!photo_config_.IsEmpty())
      << "Photos should not be getting added to the model";

  ResetImageFailures();

  bool old_images_ready = ImagesReady();

  all_decoded_topics_.push_back(photo_with_details);
  while (all_decoded_topics_.size() >
         photo_config_.GetNumDecodedTopicsToBuffer()) {
    DCHECK(!all_decoded_topics_.empty());
    all_decoded_topics_.pop_front();
  }

  NotifyImageAdded();

  // Observers expect |OnImagesReady| after |OnImageAdded|.
  bool new_images_ready = ImagesReady();
  if (!old_images_ready && new_images_ready) {
    NotifyImagesReady();
    if (photo_with_details.topic_type == ::ambient::TopicType::kPersonal) {
      ambient::RecordAmbientModeTopicSource(
          personalization_app::mojom::TopicSource::kGooglePhotos);
    } else {
      ambient::RecordAmbientModeTopicSource(
          personalization_app::mojom::TopicSource::kArtGallery);
    }
  } else if (!new_images_ready &&
             all_decoded_topics_.size() >=
                 photo_config_.min_total_topics_required &&
             !images_ready_timeout_timer_.IsRunning()) {
    images_ready_timeout_timer_.Start(
        FROM_HERE, kImagesReadyTimeout, this,
        &AmbientBackendModel::OnImagesReadyTimeoutFired);
  }
}

bool AmbientBackendModel::IsHashDuplicate(const std::string& hash) const {
  // Make sure that a photo does not appear twice in a row.
  return all_decoded_topics_.empty() ? false
                                     : all_decoded_topics_.back().hash == hash;
}

void AmbientBackendModel::AddImageFailure() {
  failures_++;
  if (ImageLoadingFailed()) {
    DVLOG(3) << "image loading failed";
    for (auto& observer : observers_)
      observer.OnImagesFailed();
  }
}

void AmbientBackendModel::ResetImageFailures() {
  failures_ = 0;
}

bool AmbientBackendModel::ImageLoadingFailed() {
  return !ImagesReady() && failures_ >= kMaxConsecutiveReadPhotoFailures;
}

base::TimeDelta AmbientBackendModel::GetPhotoRefreshInterval() const {
  if (!ImagesReady())
    return base::TimeDelta();

  return AmbientUiModel::Get()->photo_refresh_interval();
}

void AmbientBackendModel::SetPhotoConfig(AmbientPhotoConfig photo_config) {
  photo_config_ = std::move(photo_config);
  DCHECK_LE(photo_config_.min_total_topics_required,
            photo_config_.GetNumDecodedTopicsToBuffer());
  Clear();
}

void AmbientBackendModel::Clear() {
  all_decoded_topics_.clear();
  images_ready_timeout_timer_.Stop();
  images_ready_timed_out_ = false;
}

void AmbientBackendModel::GetCurrentAndNextImages(
    PhotoWithDetails* current_image_out,
    PhotoWithDetails* next_image_out) const {
  auto fill_image_out = [&](size_t idx, PhotoWithDetails* image_out) {
    if (!image_out)
      return;

    image_out->Clear();
    if (idx < all_decoded_topics_.size()) {
      *image_out = all_decoded_topics_[idx];
    }
  };
  fill_image_out(/*idx=*/0, current_image_out);
  fill_image_out(/*idx=*/1, next_image_out);
}

void AmbientBackendModel::NotifyImageAdded() {
  for (auto& observer : observers_)
    observer.OnImageAdded();
}

void AmbientBackendModel::NotifyImagesReady() {
  for (auto& observer : observers_)
    observer.OnImagesReady();
}

}  // namespace ash
