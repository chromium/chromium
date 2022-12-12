// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_DELEGATE_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_DELEGATE_H_

#include "ash/ash_export.h"

namespace ash {

// A delegate class to control and query accessibility features.
class ASH_EXPORT AccessibilityDelegate {
 public:
  virtual ~AccessibilityDelegate() {}

  // Invoked to enable the screen magnifier.
  virtual void SetMagnifierEnabled(bool enabled) = 0;

  // Returns true if the screen magnifier is enabled.
  virtual bool IsMagnifierEnabled() const = 0;

  // Returns true when the accessibility menu should be shown.
  virtual bool ShouldShowAccessibilityMenu() const = 0;

  // Saves the zoom scale of the full screen magnifier.
  virtual void SaveScreenMagnifierScale(double scale) = 0;

  // Gets a saved value of the zoom scale of full screen magnifier. If a value
  // is not saved, return a negative value.
  virtual double GetSavedScreenMagnifierScale() = 0;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_DELEGATE_H_
