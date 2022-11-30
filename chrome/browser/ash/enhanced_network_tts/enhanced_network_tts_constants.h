// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_CONSTANTS_H_
#define CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_CONSTANTS_H_

#include <stddef.h>

namespace ash {
namespace enhanced_network_tts {

// The max size for a response. Set to 5MB to provide more buffer. The mp3 file
// has 32Kbps bitrate. One minute of audio is about 240KB.
constexpr size_t kEnhancedNetworkTtsMaxResponseSize = 5 * 1024 * 1024;

// The max speech rate.
constexpr float kMaxRate = 4.0f;

// The min speech rate.
constexpr float kMinRate = 0.3f;

// The HTTP request header in which the API key should be transmitted.
extern const char kGoogApiKeyHeader[];

// The server URL for the ReadAloud API.
extern const char kReadAloudServerUrl[];

// Upload type for the network request.
extern const char kNetworkRequestUploadType[];

// Keys and paths in the request.
// See https://goto.google.com/readaloud-proto for more information.
extern const char kDefaultVoiceKey[];

extern const char kLanguageKey[];

extern const char kSelectionKey[];

extern const char kCriteriaKey[];

extern const char kTextPartsPath[];

extern const char kSpeechFactorPath[];

extern const char kForceLanguagePath[];

extern const char kVoiceCriteriaAndSelectionsPath[];

}  // namespace enhanced_network_tts
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_CONSTANTS_H_
