// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Rect;
}

namespace ash {

struct AccessibilityFocusRingInfo;

// Interface to control accessibility focus ring features.
class ASH_PUBLIC_EXPORT AccessibilityFocusRingController {
 public:
  static AccessibilityFocusRingController* Get();

  AccessibilityFocusRingController(const AccessibilityFocusRingController&) =
      delete;
  AccessibilityFocusRingController& operator=(
      const AccessibilityFocusRingController&) = delete;

  // Sets the focus ring with the given ID to the specifications of focus_ring.
  virtual void SetFocusRing(
      const std::string& focus_ring_id,
      std::unique_ptr<AccessibilityFocusRingInfo> focus_ring) = 0;

  // Hides focus ring on screen with the given ID.
  virtual void HideFocusRing(const std::string& focus_ring_id) = 0;

  // Draws a highlight at the given rects in screen coordinates. Rects may be
  // overlapping and will be merged into one layer. This looks similar to
  // selecting a region with the cursor, except it is drawn in the foreground
  // rather than behind a text layer.
  // TODO(katie): Add |caller_id| to highlights as well if other Accessibility
  // tools or extensions want to use this API.
  virtual void SetHighlights(const std::vector<gfx::Rect>& rects_in_screen,
                             SkColor skcolor) = 0;

  // Hides highlight on screen.
  // TODO(katie): Add |caller_id| to highlights as well.
  virtual void HideHighlights() = 0;

  // Callback used when SetFocusRing is called, for testing.
  virtual void SetFocusRingObserverForTesting(
      base::RepeatingCallback<void()> observer) = 0;

 protected:
  AccessibilityFocusRingController();
  virtual ~AccessibilityFocusRingController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCESSIBILITY_FOCUS_RING_CONTROLLER_H_
