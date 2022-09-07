// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_WEATHER_MODEL_OBSERVER_H_
#define ASH_AMBIENT_MODEL_AMBIENT_WEATHER_MODEL_OBSERVER_H_

#include "ash/ash_export.h"
#include "base/observer_list_types.h"

namespace ash {

class ASH_EXPORT AmbientWeatherModelObserver : public base::CheckedObserver {
 public:
  // Invoked when the weather info (condition icon or temperature) stored in the
  // model has been updated.
  virtual void OnWeatherInfoUpdated() {}

 protected:
  ~AmbientWeatherModelObserver() override = default;
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_WEATHER_MODEL_OBSERVER_H_
