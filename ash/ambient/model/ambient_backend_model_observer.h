// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_BACKEND_MODEL_OBSERVER_H_
#define ASH_AMBIENT_MODEL_AMBIENT_BACKEND_MODEL_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

// A checked observer which receives notification of changes to the
// |AmbientBackendModel|.
class ASH_PUBLIC_EXPORT AmbientBackendModelObserver
    : public base::CheckedObserver {
 public:
  // Invoked when |topics| has been changed.
  virtual void OnTopicsChanged() {}

  // Invoked when prev/current/next images changed.
  virtual void OnImagesChanged() {}

  // Invoked when the weather info (condition icon or temperature) stored in the
  // model has been updated.
  virtual void OnWeatherInfoUpdated() {}

 protected:
  ~AmbientBackendModelObserver() override = default;
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_BACKEND_MODEL_OBSERVER_H_
