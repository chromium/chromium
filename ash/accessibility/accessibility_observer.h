// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_ACCESSIBILITY_OBSERVER_H_
#define ASH_ACCESSIBILITY_ACCESSIBILITY_OBSERVER_H_

#include "ash/ash_export.h"
#include "base/observer_list_types.h"

namespace ash {

class ASH_EXPORT AccessibilityObserver : public base::CheckedObserver {
 public:
  // Called when any accessibility status changes.
  virtual void OnAccessibilityStatusChanged() = 0;

  // Called when the accessibility controller is being shutdown. Provides an
  // opportunity for observers to remove themselves.
  virtual void OnAccessibilityControllerShutdown() {}

 protected:
  ~AccessibilityObserver() override = default;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_ACCESSIBILITY_OBSERVER_H_
