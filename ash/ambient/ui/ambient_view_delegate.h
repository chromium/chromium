// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_VIEW_DELEGATE_H_
#define ASH_AMBIENT_UI_AMBIENT_VIEW_DELEGATE_H_

#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ash_export.h"
#include "base/observer_list_types.h"

namespace ash {

class AmbientBackendModel;
class AmbientWeatherModel;

// Handles UI state changes from the currently rendering view. The events below
// are common to all ambient UI modes.
class ASH_EXPORT AmbientViewDelegateObserver : public base::CheckedObserver {
 public:
  virtual void OnMarkerHit(AmbientPhotoConfig::Marker marker) = 0;
};

class ASH_EXPORT AmbientViewDelegate {
 public:
  virtual ~AmbientViewDelegate() = default;

  virtual void AddObserver(AmbientViewDelegateObserver* observer) = 0;
  virtual void RemoveObserver(AmbientViewDelegateObserver* observer) = 0;

  // Returns the model store stores all the information we get from the backdrop
  // server to render the photo frame and the glanceable weather information on
  // Ambient Mode.
  virtual AmbientBackendModel* GetAmbientBackendModel() = 0;

  // Returns the model that stores glanceable weather information.
  virtual AmbientWeatherModel* GetAmbientWeatherModel() = 0;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_VIEW_DELEGATE_H_
