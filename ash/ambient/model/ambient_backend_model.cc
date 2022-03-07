// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_backend_model.h"

#include <algorithm>
#include <random>
#include <utility>
#include <vector>

#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/time/time.h"

namespace ash {

namespace {

// Note this does not start until the minimum number of topics required in the
// AmbientPhotoConfig is reached.
constexpr base::TimeDelta kImagesReadyTimeout = base::Seconds(10);

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

  std::shuffle(paired_topics.begin(), paired_topics.end(),
               std::default_random_engine());
  return paired_topics;
}

std::pair<AmbientModeTopic, AmbientModeTopic> SplitTopic(
    const AmbientModeTopic& paired_topic) {
  const auto clear_related_fields_from_topic = [](AmbientModeTopic& topic) {
    topic.related_image_url.clear();
    topic.related_details.clear();
  };

  AmbientModeTopic topic_with_primary(paired_topic);
  clear_related_fields_from_topic(topic_with_primary);

  AmbientModeTopic topic_with_related(paired_topic);
  topic_with_related.url = std::move(topic_with_related.related_image_url);
  topic_with_related.details = std::move(topic_with_related.related_details);
  clear_related_fields_from_topic(topic_with_related);
  return std::make_pair(topic_with_primary, topic_with_related);
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

void AmbientBackendModel::AppendTopics(
    const std::vector<AmbientModeTopic>& topics_in) {
  if (photo_config_.should_split_topics) {
    static constexpr int kMaxImagesPerTopic = 2;
    topics_.reserve(topics_.size() + kMaxImagesPerTopic * topics_in.size());
    for (const AmbientModeTopic& topic : topics_in) {
      if (topic.related_image_url.empty()) {
        topics_.push_back(topic);
      } else {
        std::pair<AmbientModeTopic, AmbientModeTopic> split_topic =
            SplitTopic(topic);
        topics_.push_back(std::move(split_topic.first));
        topics_.push_back(std::move(split_topic.second));
      }
    }
  } else {
    std::vector<AmbientModeTopic> related_topics =
        CreatePairedTopics(topics_in);
    topics_.insert(topics_.end(), related_topics.begin(), related_topics.end());
  }
  NotifyTopicsChanged();
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
  DCHECK(!photo_config_.should_split_topics ||
         photo_with_details.related_photo.isNull());

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
  DCHECK_GT(photo_config_.GetNumDecodedTopicsToBuffer(), 0u);
  DCHECK_GT(photo_config_.min_total_topics_required, 0u);
  DCHECK_LE(photo_config_.min_total_topics_required,
            photo_config_.GetNumDecodedTopicsToBuffer());
  DCHECK(!photo_config_.refresh_topic_markers.empty());
  Clear();
}

void AmbientBackendModel::Clear() {
  topics_.clear();
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
