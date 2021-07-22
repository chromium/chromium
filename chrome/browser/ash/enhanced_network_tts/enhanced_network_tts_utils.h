// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_UTILS_H_
#define CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_UTILS_H_

#include <string>
#include <vector>

#include "ash/components/enhanced_network_tts/mojom/enhanced_network_tts.mojom.h"
#include "base/values.h"

namespace ash {
namespace enhanced_network_tts {

// Format the |tts_request| to a JSON string that will be accepted by the
// ReadAloud server.
std::string FormatJsonRequest(const mojom::TtsRequestPtr tts_request);

// Generate a response when encountering errors (e.g., server error, unexpected
// data).
mojom::TtsResponsePtr GetResultOnError(const mojom::TtsRequestError error_code);

// Unpack the JSON audio data from the server response.
mojom::TtsResponsePtr UnpackJsonResponse(const base::Value& json_data);

}  // namespace enhanced_network_tts
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_UTILS_H_
