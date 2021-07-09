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

// The HTTP request header in which the API key should be transmitted.
extern const char kGoogApiKeyHeader[];

// The server URL for the ReadAloud API.
extern const char kReadAloudServerUrl[];

// Upload type for the network request.
extern const char kNetworkRequestUploadType[];

}  // namespace enhanced_network_tts
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_CONSTANTS_H_
