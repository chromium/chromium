// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNITION_UTIL_H_
#define CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNITION_UTIL_H_

#include <string>

#include "media/mojo/mojom/speech_recognizer.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace speech {

// Returns the on-device speech recognition availability status for a given
// language code (i.e. "en-US"). Speech recognition considered available if the
// Speech On-Device API is supported and the language pack for the corresponding
// language is installed. This utility function must be run in the browser
// process.
media::mojom::AvailabilityStatus GetOnDeviceSpeechRecognitionAvailabilityStatus(
    content::BrowserContext* context,
    const std::string& language);

}  // namespace speech

#endif  // CHROME_BROWSER_SPEECH_ON_DEVICE_SPEECH_RECOGNITION_UTIL_H_
