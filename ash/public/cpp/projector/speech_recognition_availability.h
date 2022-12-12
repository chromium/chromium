// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PROJECTOR_SPEECH_RECOGNITION_AVAILABILITY_H_
#define ASH_PUBLIC_CPP_PROJECTOR_SPEECH_RECOGNITION_AVAILABILITY_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// Enum class used to represent the availability of speech recognition.
enum class ASH_PUBLIC_EXPORT SpeechRecognitionAvailability {
  // Device does not support SODA (Speech on Device API)
  kSodaNotAvailable,
  // Server based feature is not supported.
  kServerBasedRecognitionNotAvailable,
  // User's language is not supported.
  kUserLanguageNotAvailable,
  // SODA binary is not yet installed.
  kSodaNotInstalled,
  // SODA binary and language packs are downloading.
  kSodaInstalling,
  // SODA installation failed.
  kSodaInstallationErrorUnspecified,
  // SODA installation error needs reboot
  kSodaInstallationErrorNeedsReboot,
  // SODA is available to be used.
  kSodaAvailable,
  // Server based recognition is available.
  kServerBasedRecognitionAvailable
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PROJECTOR_SPEECH_RECOGNITION_AVAILABILITY_H_
