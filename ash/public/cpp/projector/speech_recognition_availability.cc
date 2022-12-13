// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/projector/speech_recognition_availability.h"

namespace ash {

bool SpeechRecognitionAvailability::IsAvailable() const {
  if (use_on_device) {
    return on_device_availability ==
           OnDeviceRecognitionAvailability::kAvailable;
  }

  return server_based_availability ==
         ServerBasedRecognitionAvailability::kAvailable;
}

}  // namespace ash
