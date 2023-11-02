// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_DEFAULT_ACCESSIBILITY_DELEGATE_H_
#define ASH_ACCESSIBILITY_DEFAULT_ACCESSIBILITY_DELEGATE_H_

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT DefaultAccessibilityDelegate : public AccessibilityDelegate {
 public:
  DefaultAccessibilityDelegate();

  DefaultAccessibilityDelegate(const DefaultAccessibilityDelegate&) = delete;
  DefaultAccessibilityDelegate& operator=(const DefaultAccessibilityDelegate&) =
      delete;

  ~DefaultAccessibilityDelegate() override;

  void SetMagnifierEnabled(bool enabled) override;
  bool IsMagnifierEnabled() const override;
  bool ShouldShowAccessibilityMenu() const override;
  void SaveScreenMagnifierScale(double scale) override;
  double GetSavedScreenMagnifierScale() override;

 private:
  bool screen_magnifier_enabled_ = false;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_DEFAULT_ACCESSIBILITY_DELEGATE_H_
