// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/enhanced_network_tts/enhanced_network_tts_constants.h"

namespace ash {
namespace enhanced_network_tts {

const char kGoogApiKeyHeader[] = "X-Goog-Api-Key";

const char kReadAloudServerUrl[] =
    "https://readaloud.googleapis.com//v1:generateAudioDocStream";

const char kNetworkRequestUploadType[] = "application/json";

const char kFullRequestTemplate[] =
    R"({
        "advanced_options": {
          "audio_generation_options": {"speed_factor": %.1f},
          "force_language": "%s"
        },
        "text": {
          "text_parts": ["%s"]
        },
        "voice_settings": {
          "voice_criteria_and_selections": [{
            "criteria": {"language": "%s"},
            "selection": {"default_voice": "%s"}
          }]
        }
      })";

extern const char kSimpleRequestTemplate[] =
    R"({"advanced_options": {
          "audio_generation_options": {"speed_factor": %.1f}
        },
        "text": {"text_parts": ["%s"]}})";

}  // namespace enhanced_network_tts
}  // namespace ash
