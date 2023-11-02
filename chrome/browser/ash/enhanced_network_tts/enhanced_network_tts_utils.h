// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_UTILS_H_
#define CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_UTILS_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "chromeos/ash/components/enhanced_network_tts/mojom/enhanced_network_tts.mojom.h"

namespace ash {
namespace enhanced_network_tts {

// Format the |tts_request| to a JSON string that will be accepted by the
// ReadAloud server.
std::string FormatJsonRequest(const mojom::TtsRequestPtr tts_request);

// Find text breaks for the |utterance|. The text breaks should chunk the
// utterance into text pieces shorter than |length_limit|. When finding the
// breaks, we should match text breaks with sentence ends to avoid splitting
// a sentence into two chunks. If that is not possible, we try to match text
// breaks with word ends. If that still fails, it indicates that the utterance
// contains a over-length word and we calculate text breaks using
// |length_limit| directly. The returned vector contains the indexes for the
// breaks. For example, a [2, 4] vector means we should pass the utterance
// "hello" as: "hel" and "lo". The |utterance| must not start with
// whitespaces, and the caller might need to pre-trim the |utterance| before
// calling this method.
std::vector<uint16_t> FindTextBreaks(const std::u16string& utterance,
                                     const int length_limit);

// Generate a response when encountering errors (e.g., server error, unexpected
// data).
mojom::TtsResponsePtr GetResultOnError(const mojom::TtsRequestError error_code);

// Unpack the JSON audio data from the server response. The data corresponds to
// the text piece that has the |start_index| in the original input utterance.
// |last_request| indicates if this is the last JSON data we expect.
mojom::TtsResponsePtr UnpackJsonResponse(const base::Value::List& list_data,
                                         const int start_index,
                                         const bool last_request);

}  // namespace enhanced_network_tts
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_UTILS_H_
