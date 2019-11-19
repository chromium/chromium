// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/photo_model.h"

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/model/photo_model_observer.h"

namespace ash {

PhotoModel::PhotoModel() = default;

PhotoModel::~PhotoModel() = default;

void PhotoModel::AddObserver(PhotoModelObserver* observer) {
  observers_.AddObserver(observer);
}

void PhotoModel::RemoveObserver(PhotoModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool PhotoModel::ShouldFetchImmediately() const {
  // If currently shown image is close to the end of images cache, we prefetch
  // more image.
  const int next_load_image_index = GetImageBufferLength() / 2;
  return images_.empty() ||
         current_image_index_ >
             static_cast<int>(images_.size() - 1 - next_load_image_index);
}

void PhotoModel::ShowNextImage() {
  // Do not show next if have not downloaded enough images.
  if (ShouldFetchImmediately())
    return;

  const int max_current_image_index = GetImageBufferLength() / 2;
  if (current_image_index_ >= max_current_image_index) {
    // Pop the first image and keep |current_image_index_| unchanged, will be
    // equivalent to show next image.
    images_.pop_front();
  } else {
    ++current_image_index_;
  }
  NotifyImagesChanged();
}

void PhotoModel::AddNextImage(const gfx::ImageSkia& image) {
  images_.emplace_back(image);

  // Update the first image.
  if (images_.size() == 1)
    NotifyImagesChanged();
}

gfx::ImageSkia PhotoModel::GetPrevImage() const {
  if (current_image_index_ == 0)
    return gfx::ImageSkia();

  return images_[current_image_index_ - 1];
}

gfx::ImageSkia PhotoModel::GetCurrImage() const {
  if (images_.empty())
    return gfx::ImageSkia();

  return images_[current_image_index_];
}

gfx::ImageSkia PhotoModel::GetNextImage() const {
  if (images_.empty() ||
      static_cast<int>(images_.size() - current_image_index_) == 1) {
    return gfx::ImageSkia();
  }

  return images_[current_image_index_ + 1];
}

void PhotoModel::NotifyImagesChanged() {
  for (auto& observer : observers_)
    observer.OnImagesChanged();
}

int PhotoModel::GetImageBufferLength() const {
  return buffer_length_for_testing_ == -1 ? kImageBufferLength
                                          : buffer_length_for_testing_;
}

}  // namespace ash
