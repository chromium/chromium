// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/enhanced_network_tts/enhanced_network_tts_constants.h"

namespace ash {
namespace enhanced_network_tts {

const char kGoogApiKeyHeader[] = "X-Goog-Api-Key";

const char kReadAloudServerUrl[] =
    "https://readaloud.googleapis.com//v1:generateAudioDocStream";

const char kNetworkRequestUploadType[] = "application/json";

const char kDefaultVoiceKey[] = "default_voice";

const char kLanguageKey[] = "language";

const char kSelectionKey[] = "selection";

const char kCriteriaKey[] = "criteria";

const char kTextPartsPath[] = "text.text_parts";

const char kSpeechFactorPath[] =
    "advanced_options.audio_generation_options.speed_factor";

const char kForceLanguagePath[] = "advanced_options.force_language";

const char kVoiceCriteriaAndSelectionsPath[] =
    "voice_settings.voice_criteria_and_selections";

}  // namespace enhanced_network_tts
}  // namespace ash
