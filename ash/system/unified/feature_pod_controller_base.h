// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_FEATURE_POD_CONTROLLER_BASE_H_
#define ASH_SYSTEM_UNIFIED_FEATURE_POD_CONTROLLER_BASE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/constants/quick_settings_catalogs.h"

namespace ash {

class FeatureTile;

// Base class for controllers of feature pod buttons.
// To add a new feature pod button, implement this class, and add to the list in
// UnifiedSystemTrayController::InitFeatureTiles().
class ASH_EXPORT FeaturePodControllerBase {
 public:
  virtual ~FeaturePodControllerBase() {}

  // Creates FeatureTile view. `compact` determines whether to present a Primary
  // or Compact tile.
  virtual std::unique_ptr<FeatureTile> CreateTile(bool compact) = 0;

  // Returns the feature catalog name which is used for UMA tracking. Please
  // remember to call the corresponding tracking method (`TrackToggleUMA` and
  // `TrackDiveInUMA`) in the `OnIconPressed` and `OnLabelPressed`
  // implementation.
  virtual QsFeatureCatalogName GetCatalogName() = 0;

  // Called when the icon of the feature pod button is clicked.
  // If the feature pod is togglable, it is expected to toggle the feature.
  virtual void OnIconPressed() = 0;

  // Called when the label hover area of the feature pod button is clicked.
  // If the feature pod has a detailed view, it is expected to show the detailed
  // view. Defaults to OnIconPressed().
  virtual void OnLabelPressed();

  // Tracks the toggling behavior, usually happens `OnIconPressed`. But this
  // method can also be called in the `OnLabelPressed` method, when pressing on
  // the label has the same behavior as pressing on the icon. If the feature has
  // no `target_toggle_state` state, such as the screen capture feaure, pass
  // `true` to this method.
  void TrackToggleUMA(bool target_toggle_state);

  // Tracks the navigating to detailed page behavior, usually happens
  // `OnLabelPressed`, sometimes also happens `OnIconPressed`.
  void TrackDiveInUMA();

  // Tracks the visibility of this feature pod. Call this method if the
  // visibility is set from `false` to `true`.
  void TrackVisibilityUMA();
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_FEATURE_POD_CONTROLLER_BASE_H_
