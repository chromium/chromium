// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_PHOTO_CONFIG_H_
#define ASH_AMBIENT_MODEL_AMBIENT_PHOTO_CONFIG_H_

#include "ash/ash_export.h"

namespace ash {

struct PhotoWithDetails;

// Terminology:
// An "asset" in this API refers to a placeholder in the currently rendering UI
// where image(s) from the user's selected photo category should be displayed.
// The assets are dynamic in that new image(s) can be assigned to each asset
// when desired by the UI. Note that in some cases, both a topic's primary and
// related image (multiple images) can occupy a single asset; it depends on the
// UI's configuration.
//
// AmbientPhotoConfig tells the photo model and controller about the assets
// present in the current UI. A different implementation can be needed
// for each type of UI, depending on its requirements for handling photos.
//
// Implementations are not thread-safe.
class ASH_EXPORT AmbientPhotoConfig {
 public:
  virtual ~AmbientPhotoConfig() = default;

  // Returns the number of assets present in the currently rendering UI. This
  // must be an immutable value that is fixed for the duration of this object's
  // lifetime.
  virtual int GetNumAssets() const = 0;

  // Returns the number of sets of assets to keep buffered at any given time
  // while rendering. Example: If GetNumAssets() is 5 and
  // GetNumSetsOfAssetsToBuffer() is 2, the model/controller will make a best
  // effort to keep 5 X 2 = 10 assets buffered at any given time, and available
  // for the UI to retrieve. This must be an immutable value that is fixed for
  // the duration of this object's lifetime.
  virtual int GetNumSetsOfAssetsToBuffer() const = 0;

  // Returns the number of assets that can be occupied in the current UI by the
  // images in the |decoded_topic|.
  virtual int GetNumAssetsInTopic(
      const PhotoWithDetails& decoded_topic) const = 0;
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_PHOTO_CONFIG_H_
