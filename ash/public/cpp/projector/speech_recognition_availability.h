// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PROJECTOR_SPEECH_RECOGNITION_AVAILABILITY_H_
#define ASH_PUBLIC_CPP_PROJECTOR_SPEECH_RECOGNITION_AVAILABILITY_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

enum class ASH_PUBLIC_EXPORT OnDeviceRecognitionAvailability : int {
  // Device does not support SODA (Speech on Device API)
  kSodaNotAvailable = 0,
  // User's language is not supported by SODA.
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
  kAvailable,
};

enum class ASH_PUBLIC_EXPORT ServerBasedRecognitionAvailability : int {
  // Server based feature is not available.
  kServerBasedRecognitionNotAvailable = 0,
  // User's language is not supported by server based recognition.
  kUserLanguageNotAvailable,
  // Server based speech recognition is available.
  kAvailable,
};

struct ASH_PUBLIC_EXPORT SpeechRecognitionAvailability {
  bool use_on_device = true;
  OnDeviceRecognitionAvailability on_device_availability;
  ServerBasedRecognitionAvailability server_based_availability;

  bool IsAvailable() const;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PROJECTOR_SPEECH_RECOGNITION_AVAILABILITY_H_
