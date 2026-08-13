// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note, this file is only ever used on Chrome OS.
#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CONSTANTS_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CONSTANTS_H_

#include "chrome/common/extensions/api/speech_recognition_private.h"

namespace speech {

enum class SpeechRecognitionType { kNetwork, kOnDevice };

extensions::api::speech_recognition_private::SpeechRecognitionType
SpeechRecognitionTypeToApiType(SpeechRecognitionType type);

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CONSTANTS_H_
