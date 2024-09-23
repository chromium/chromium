// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_A11Y_FEATURE_TYPE_H_
#define ASH_ACCESSIBILITY_A11Y_FEATURE_TYPE_H_

#include <stddef.h>

namespace ash {

// The type of each accessibility feature.
enum class A11yFeatureType {
  kAutoclick = 0,
  kCaretHighlight,
  kColorCorrection,
  kCursorColor,
  kCursorHighlight,
  kDictation,
  kDisableTrackpad,
  kDockedMagnifier,
  kFaceGaze,
  kFlashNotifications,
  kFloatingMenu,
  kFocusHighlight,
  kFullscreenMagnifier,
  kHighContrast,
  kLargeCursor,
  kLiveCaption,
  kMonoAudio,
  kMouseKeys,
  kReducedAnimations,
  kSelectToSpeak,
  kSpokenFeedback,
  kStickyKeys,
  kSwitchAccess,
  kVirtualKeyboard,

  kFeatureCount,

  // A special "none" value meaning a feature has no conflicts with other
  // features. See AccessibilityControllerImpl::Feature.
  kNoConflictingFeature
};

// The number of accessibility features.
constexpr size_t kA11yFeatureTypeCount =
    static_cast<size_t>(A11yFeatureType::kFeatureCount);

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_A11Y_FEATURE_TYPE_H_
