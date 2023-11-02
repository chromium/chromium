// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_CONSTANTS_H_
#define ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_CONSTANTS_H_

namespace ash {

// User-selectable speech rates. Note that these are also recorded in
// the histogram |CrosSelectToSpeak.OverrideSpeechRateMultiplier|. If new speeds
// are added, please update CrosSelectToSpeakOverrideSpeechRateMultiplier in
// enums.xml. If speed changes are no longer discrete values (i.e. if a future
// change makes speed changes continuous), please deprecate the histogram.
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

#endif  // ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_SELECT_TO_SPEAK_CONSTANTS_H_
