// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_PHOTO_MODEL_OBSERVER_H_
#define ASH_AMBIENT_MODEL_PHOTO_MODEL_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

// A checked observer which receives notification of changes to the PhotoModel
// in ambient mode.
class ASH_PUBLIC_EXPORT PhotoModelObserver : public base::CheckedObserver {
 public:
  // Invoked when prev/current/next images changed.
  virtual void OnImagesChanged() = 0;

 protected:
  ~PhotoModelObserver() override = default;
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_PHOTO_MODEL_OBSERVER_H_
