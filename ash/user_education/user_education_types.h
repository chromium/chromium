// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_USER_EDUCATION_TYPES_H_
#define ASH_USER_EDUCATION_USER_EDUCATION_TYPES_H_

namespace ash {

// Each value uniquely identifies a help bubble. Used to gate creation of new
// help bubbles to avoid spamming the user.
enum class HelpBubbleId {
  kMinValue,
  kHoldingSpaceTour = kMinValue,
  kTest,
  kWelcomeTourExploreApp,
  kWelcomeTourHomeButton,
  kWelcomeTourSearchBox,
  kWelcomeTourSettingsApp,
  kWelcomeTourShelf,
  kWelcomeTourStatusArea,
  kMaxValue = kWelcomeTourStatusArea,
};

// Each value uniquely identifies a style of help bubble. Help bubbles of
// different styles may differ both in terms of appearance as well as behavior.
enum class HelpBubbleStyle {
  kMinValue,
  kDialog = kMinValue,
  kNudge,
  kMaxValue = kNudge,
};

// Each value uniquely identifies a ping. Used to gate creation of new pings to
// avoid spamming the user.
enum class PingId {
  kMinValue,
  kHoldingSpaceTour = kMinValue,
  kTest1,
  kTest2,
  kMaxValue = kTest2,
};

// Each value uniquely identifies a feature tutorial. Used to gate creation of
// new feature tutorials to avoid spamming the user.
enum class TutorialId {
  kMinValue,
  kCaptureModeTourPrototype1 = kMinValue,
  kCaptureModeTourPrototype2,
  kHoldingSpaceTourPrototype1,
  kHoldingSpaceTourPrototype2,
  kTest,
  kWelcomeTourPrototype1,
  kMaxValue = kWelcomeTourPrototype1,
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_USER_EDUCATION_TYPES_H_
