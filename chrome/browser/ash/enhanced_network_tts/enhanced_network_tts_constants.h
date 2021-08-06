// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_CONSTANTS_H_
#define CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_CONSTANTS_H_

#include <stddef.h>

namespace ash {
namespace enhanced_network_tts {

// The max size for a response. Set to 1MB.
constexpr size_t kEnhancedNetworkTtsMaxResponseSize = 1024 * 1024;

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

// The template for a full request that contains utterance, rate, voice name,
// and language. See https://goto.google.com/readaloud-proto for more
// information.
extern const char kFullRequestTemplate[];

// The template for a simple request that only contains utterance and rate.
// See https://goto.google.com/readaloud-proto for more information.
extern const char kSimpleRequestTemplate[];

}  // namespace enhanced_network_tts
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_CONSTANTS_H_
