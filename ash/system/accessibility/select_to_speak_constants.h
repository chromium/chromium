// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_CONSTANTS_H_
#define ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_CONSTANTS_H_

namespace ash {

// User-selectable speech rates.
constexpr double kSelectToSpeakSpeechRateSlow = 0.5;
constexpr double kSelectToSpeakSpeechRateNormal = 1.0;
constexpr double kSelectToSpeakSpeechRatePeppy = 1.2;
constexpr double kSelectToSpeakSpeechRateFast = 1.5;
constexpr double kSelectToSpeakSpeechRateFaster = 2.0;

const double kSelectToSpeakSpeechRates[] = {
    kSelectToSpeakSpeechRateSlow,   kSelectToSpeakSpeechRateNormal,
    kSelectToSpeakSpeechRatePeppy,  kSelectToSpeakSpeechRateFast,
    kSelectToSpeakSpeechRateFaster,
};

const char kSelectToSpeakSpeedBubbleWindowName[] = "SelectToSpeakSpeedBubble";

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_CONSTANTS_H_