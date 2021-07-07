// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/ambient/model/ambient_backend_model.h"

#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"

namespace ash {

namespace {
int TypeToIndex(::ambient::TopicType topic_type) {
  int index = static_cast<int>(topic_type);
  DCHECK_GE(index, 0);
  return index;
}

::ambient::TopicType IndexToType(int index) {
  ::ambient::TopicType topic_type = static_cast<::ambient::TopicType>(index);
  return topic_type;
}

std::vector<AmbientModeTopic> CreatePairedTopics(
    const std::vector<AmbientModeTopic>& topics) {
  // We pair two topics if:
  // 1. They are in the landscape orientation.
  // 2. They are in the same category.
  // 3. They are not Geo photos.
  base::flat_map<int, std::vector<int>> topics_by_type;
  std::vector<AmbientModeTopic> paired_topics;
  int topic_idx = -1;
  for (const auto& topic : topics) {
    topic_idx++;

    // Do not pair Geo photos, which will be rotate to fill the screen.
    // If a photo is portrait, it is from Google Photos and should have a paired
    // photo already.
    if (topic.topic_type == ::ambient::TopicType::kGeo || topic.is_portrait) {
      paired_topics.emplace_back(topic);
      continue;
    }

    int type_index = TypeToIndex(topic.topic_type);
    auto it = topics_by_type.find(type_index);
    if (it == topics_by_type.end()) {
      topics_by_type.insert({type_index, {topic_idx}});
    } else {
      it->second.emplace_back(topic_idx);
    }
  }

  // We merge two unpaired topics to create a new topic with related images.
  for (auto it = topics_by_type.begin(); it < topics_by_type.end(); ++it) {
    size_t idx = 0;
    while (idx < it->second.size() - 1) {
      AmbientModeTopic paired_topic;
      const auto& topic_1 = topics[it->second[idx]];
      const auto& topic_2 = topics[it->second[idx + 1]];
      paired_topic.url = topic_1.url;
      paired_topic.related_image_url = topic_2.url;

      paired_topic.details = topic_1.details;
      paired_topic.related_details = topic_2.details;
      paired_topic.topic_type = IndexToType(it->first);
      paired_topic.is_portrait = topic_1.is_portrait;
      paired_topics.emplace_back(paired_topic);

      idx += 2;
    }
  }
  return paired_topics;
}
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
  std::vector<AmbientModeTopic> related_topics = CreatePairedTopics(topics);
  topics_.insert(topics_.end(), related_topics.begin(), related_topics.end());
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
