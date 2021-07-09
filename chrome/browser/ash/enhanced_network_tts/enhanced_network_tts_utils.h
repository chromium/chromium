// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_UTILS_H_
#define CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_UTILS_H_

#include <string>
#include <vector>

#include "base/values.h"

namespace ash {
namespace enhanced_network_tts {

// Format the |input_text| to a JSON string that will be accepted by the
// ReadAloud server.
std::string FormatJsonRequest(const std::string& input_text);

// Generate a result when encountering errors (e.g., server error, parsing
// error). We return an empty vector for now.
// TODO(crbug.com/1217301): Provide more specific error information.
std::vector<uint8_t> GetResultOnError();

// Unpack the JSON audio data from the server response to a vector.
std::vector<uint8_t> UnpackJsonResponse(const base::Value& json_data);

}  // namespace enhanced_network_tts
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_UTILS_H_
