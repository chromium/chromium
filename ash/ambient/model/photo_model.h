// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_PHOTO_MODEL_H_
#define ASH_AMBIENT_MODEL_PHOTO_MODEL_H_

#include "ash/ash_export.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

class PhotoModelObserver;

// The model belonging to AmbientController which tracks photo state and
// notifies a pool of observers.
class ASH_EXPORT PhotoModel {
 public:
  PhotoModel();
  ~PhotoModel();

  void AddObserver(PhotoModelObserver* observer);
  void RemoveObserver(PhotoModelObserver* observer);

  // Prefetch one more image for ShowNextImage animations.
  bool ShouldFetchImmediately() const;

  // Show the next downloaded image.
  void ShowNextImage();

  // Add image to local storage.
  void AddNextImage(const gfx::ImageSkia& image);

  // Get images from local storage. Could be null image.
  gfx::ImageSkia GetPrevImage() const;
  gfx::ImageSkia GetCurrImage() const;
  gfx::ImageSkia GetNextImage() const;

  void set_buffer_length_for_testing(int length) {
    buffer_length_for_testing_ = length;
  }

 private:
  void NotifyImagesChanged();
  int GetImageBufferLength() const;

  // A local cache for downloaded images. This buffer is split into two equal
  // length of kImageBufferLength / 2 for previous seen and next unseen images.
  base::circular_deque<gfx::ImageSkia> images_;

  // The index of currently shown image.
  int current_image_index_ = 0;

  int buffer_length_for_testing_ = -1;

  base::ObserverList<ash::PhotoModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(PhotoModel);
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_PHOTO_MODEL_H_
