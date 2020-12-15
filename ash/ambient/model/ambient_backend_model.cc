// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/ambient/model/ambient_backend_model.h"

#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "base/logging.h"

namespace ash {

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
}

bool PhotoWithDetails::IsNull() const {
  return photo.isNull();
}

// AmbientBackendModel---------------------------------------------------------
AmbientBackendModel::AmbientBackendModel() = default;
AmbientBackendModel::~AmbientBackendModel() = default;

void AmbientBackendModel::AddObserver(AmbientBackendModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AmbientBackendModel::RemoveObserver(
    AmbientBackendModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AmbientBackendModel::AppendTopics(
    const std::vector<AmbientModeTopic>& topics) {
  topics_.insert(topics_.end(), topics.begin(), topics.end());
  NotifyTopicsChanged();
}

bool AmbientBackendModel::ImagesReady() const {
  return !current_image_.IsNull() && !next_image_.IsNull();
}

void AmbientBackendModel::AddNextImage(
    const PhotoWithDetails& photo_with_details) {
  DCHECK(!photo_with_details.IsNull());

  ResetImageFailures();

  bool should_notify_ready = false;

  if (current_image_.IsNull()) {
    // If |current_image_| is null, |photo_with_details| should be the first
    // image stored. |next_image_| should also be null.
    DCHECK(next_image_.IsNull());
    current_image_ = photo_with_details;
  } else if (next_image_.IsNull()) {
    // |current_image_| and |next_image_| are set.
    next_image_ = photo_with_details;
    should_notify_ready = true;
  } else {
    // Cycle out the old |current_image_|.
    current_image_ = next_image_;
    next_image_ = photo_with_details;
  }

  NotifyImageAdded();

  // Observers expect |OnImagesReady| after |OnImageAdded|.
  if (should_notify_ready)
    NotifyImagesReady();
}

bool AmbientBackendModel::IsHashDuplicate(const std::string& hash) const {
  // Make sure that a photo does not appear twice in a row. If |next_image_| is
  // not null, the new image must not be identical to |next_image_|.
  const auto& image_to_compare =
      next_image_.IsNull() ? current_image_ : next_image_;
  return image_to_compare.hash == hash;
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

void AmbientBackendModel::Clear() {
  topics_.clear();
  current_image_.Clear();
  next_image_.Clear();
}

float AmbientBackendModel::GetTemperatureInCelsius() const {
  return (temperature_fahrenheit_ - 32) * 5 / 9;
}

void AmbientBackendModel::UpdateWeatherInfo(
    const gfx::ImageSkia& weather_condition_icon,
    float temperature_fahrenheit,
    bool show_celsius) {
  weather_condition_icon_ = weather_condition_icon;
  temperature_fahrenheit_ = temperature_fahrenheit;
  show_celsius_ = show_celsius;

  if (!weather_condition_icon.isNull())
    NotifyWeatherInfoUpdated();
}

void AmbientBackendModel::NotifyTopicsChanged() {
  for (auto& observer : observers_)
    observer.OnTopicsChanged();
}

void AmbientBackendModel::NotifyImageAdded() {
  for (auto& observer : observers_)
    observer.OnImageAdded();
}

void AmbientBackendModel::NotifyImagesReady() {
  for (auto& observer : observers_)
    observer.OnImagesReady();
}

void AmbientBackendModel::NotifyWeatherInfoUpdated() {
  for (auto& observer : observers_)
    observer.OnWeatherInfoUpdated();
}

}  // namespace ash
