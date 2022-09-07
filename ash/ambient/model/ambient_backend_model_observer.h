// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_BACKEND_MODEL_OBSERVER_H_
#define ASH_AMBIENT_MODEL_AMBIENT_BACKEND_MODEL_OBSERVER_H_

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

// A checked observer which receives notification of changes to the
// |AmbientBackendModel|.
class ASH_PUBLIC_EXPORT AmbientBackendModelObserver
    : public base::CheckedObserver {
 public:
  // Invoked when a new image is added.
  virtual void OnImageAdded() {}

  // Invoked when two images have been added. Two images are necessary to
  // prevent screen burn in so that images can still change if further downloads
  // fail. When the second image is added, this method is invoked after
  // |OnImageAdded|.
  virtual void OnImagesReady() {}

  // Invoked when fetching images has failed and not enough images are present
  // to start ambient mode.
  virtual void OnImagesFailed() {}

 protected:
  ~AmbientBackendModelObserver() override = default;
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_BACKEND_MODEL_OBSERVER_H_
