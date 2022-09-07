// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_FEATURE_POD_CONTROLLER_BASE_H_
#define ASH_SYSTEM_UNIFIED_FEATURE_POD_CONTROLLER_BASE_H_

#include "ash/ash_export.h"
#include "ash/system/tray/system_tray_item_uma_type.h"

namespace ash {

class FeaturePodButton;

// Base class for controllers of feature pod buttons.
// To add a new feature pod button, implement this class, and add to the list in
// UnifiedSystemTrayController::InitFeaturePods().
class ASH_EXPORT FeaturePodControllerBase {
 public:
  virtual ~FeaturePodControllerBase() {}

  // Create the view. Subclasses instantiate FeaturePodButton.
  // The view will be owned by views hierarchy. The view will be always deleted
  // after the controller is destructed (UnifiedSystemTrayBubble guarantees
  // this).
  virtual FeaturePodButton* CreateButton() = 0;

  // Called when the icon of the feature pod button is clicked.
  // If the feature pod is togglable, it is expected to toggle the feature.
  virtual void OnIconPressed() = 0;

  // Called when the label hover area of the feature pod button is clicked.
  // If the feature pod has a detailed view, it is expected to show the detailed
  // view. Defaults to OnIconPressed().
  virtual void OnLabelPressed();

  // Return histogram value for Ash.SystemMenu.DefaultView.VisibleRows. If the
  // button is not recorded, UMA_NOT_RECORDED will be used.
  virtual SystemTrayItemUmaType GetUmaType() const = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_FEATURE_POD_CONTROLLER_BASE_H_
