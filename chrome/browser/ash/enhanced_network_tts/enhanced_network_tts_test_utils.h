// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_TEST_UTILS_H_
#define CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_TEST_UTILS_H_

#include <stddef.h>
#include <string>
#include <vector>

#include "base/values.h"

namespace ash {
namespace enhanced_network_tts {

// The accuracy used to compare two doubles.
constexpr double kDoubleCompareAccuracy = 0.000001;

// The template for a full request that contains utterance, rate, voice name,
// and language. See https://goto.google.com/readaloud-proto for more
// information.
extern const char kFullRequestTemplate[];

// The template for a simple request that only contains utterance and rate.
// See https://goto.google.com/readaloud-proto for more information.
extern const char kSimpleRequestTemplate[];

// Template for a server response.
extern const char kTemplateResponse[];

// Create a correct request based on the |kFullRequestTemplate|.
std::string CreateCorrectRequest(const std::string& input_text,
                                 float rate,
                                 const std::string& voice_name,
                                 const std::string& lang);

// Create a correct request based on the |kSimpleRequestTemplate|.
std::string CreateCorrectRequest(const std::string& input_text, float rate);

// Create a server response based on the |kTemplateResponse|.
std::string CreateServerResponse(const std::vector<uint8_t>& expected_output);

// Check if two request strings are equal. Use the |kDoubleCompareAccuracy| when
// checking speech rates. This assumes the two strings follow
// |kFullRequestTemplate| or |kSimpleRequestTemplate|, and speech rates have one
// decimal digit only.
bool AreRequestsEqual(const std::string& json_a, const std::string& json_b);

}  // namespace enhanced_network_tts
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ENHANCED_NETWORK_TTS_ENHANCED_NETWORK_TTS_TEST_UTILS_H_
