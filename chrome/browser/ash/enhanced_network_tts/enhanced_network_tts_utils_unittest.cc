// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/enhanced_network_tts/enhanced_network_tts_utils.h"

#include <memory>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace enhanced_network_tts {

namespace {
// Template for a request that contains all variables.
constexpr char kTemplateRequest[] =
    R"({
        "advanced_options": {
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

// Template for a request that only contains an utterance.
constexpr char kTemplateUtteranceOnlyRequest[] =
    R"({"text": {"text_parts": ["%s"]}})";

// Template for a server response.
constexpr char kTemplateResponse[] =
    R"([
        {"metadata": {}},
        {"text": {
          "timingInfo": [
            {
              "text": "test1",
              "location": {
                "textLocation": {"length": 5},
                "timeLocation": {
                  "timeOffset": "0.01s",
                  "duration": "0.14s"
                }
              }
            }
          ]}
        },
        {"audio": {"bytes": "%s"}}
      ])";

// Function to remove all spaces and line breaks from a given string
std::string RemoveSpaceAndLineBreak(std::string str) {
  str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
  str.erase(std::remove(str.begin(), str.end(), '\n'), str.end());
  return str;
}

}  // namespace

using EnhancedNetworkTtsUtilsTest = testing::Test;

TEST_F(EnhancedNetworkTtsUtilsTest, FormatJsonRequest) {
  const std::string utterance = "Hello, World!";
  const std::string voice = "test_name";
  const std::string language = "en-US";
  const std::string expected_text =
      base::StringPrintf(kTemplateRequest, language.c_str(), utterance.c_str(),
                         language.c_str(), voice.c_str());
  const std::string formated_text =
      FormatJsonRequest(mojom::TtsRequest::New(utterance, voice, language));

  EXPECT_EQ(RemoveSpaceAndLineBreak(formated_text),
            RemoveSpaceAndLineBreak(expected_text));
}

TEST_F(EnhancedNetworkTtsUtilsTest, FormatJsonRequestWithUtteranceOnly) {
  const std::string utterance = "Hello, World!";
  const std::string expected_text =
      base::StringPrintf(kTemplateUtteranceOnlyRequest, utterance.c_str());
  const std::string formated_text = FormatJsonRequest(
      mojom::TtsRequest::New(utterance, absl::nullopt, absl::nullopt));

  EXPECT_EQ(RemoveSpaceAndLineBreak(formated_text),
            RemoveSpaceAndLineBreak(expected_text));
}

TEST_F(EnhancedNetworkTtsUtilsTest, FindTextBreaks) {
  std::string utterance = "";
  int length_limit = 10;
  std::vector<uint16_t> expected_output = {};
  EXPECT_EQ(FindTextBreaks(utterance, length_limit), expected_output);

  utterance = "utterance is shorter than length_limit";
  length_limit = 1000;
  expected_output = {37};
  EXPECT_EQ(FindTextBreaks(utterance, length_limit), expected_output);

  utterance = "limit is 1";
  length_limit = 1;
  expected_output = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(FindTextBreaks(utterance, length_limit), expected_output);

  // Index ref:012345678901234
  utterance = "Sent 1! Sent 2!";
  length_limit = 4;
  // 3 = word end of "Sent"
  // 7 = first sentence end
  // 11 = word end of "Sent"
  // 14 = second sentence end
  expected_output = {3, 7, 11, 14};
  EXPECT_EQ(FindTextBreaks(utterance, length_limit), expected_output);

  // Index ref:01234567890123456789012
  utterance = "Sent 1! Sent 2. Sent 3!";
  length_limit = 8;
  // 7 = first sentence end
  // 15 = second sentence end
  // 22 = third sentence end
  expected_output = {7, 15, 22};
  EXPECT_EQ(FindTextBreaks(utterance, length_limit), expected_output);

  // Index ref:01234567890123456
  utterance = "Sent 1! Sent two!";
  length_limit = 3;
  // 2 = over length limit at char 'n'
  // 5 = word end of "1"
  // 7 = first sentence end
  // 10 = over length limit at char 'n'
  // 11 = word end of "Sent"
  // 14 = over length limit at char 'w'
  // 16 = second sentence end
  expected_output = {2, 5, 7, 10, 11, 14, 16};
  EXPECT_EQ(FindTextBreaks(utterance, length_limit), expected_output);
}

TEST_F(EnhancedNetworkTtsUtilsTest, GetResultOnError) {
  mojom::TtsResponsePtr result =
      GetResultOnError(mojom::TtsRequestError::kReceivedUnexpectedData);
  EXPECT_TRUE(result->is_error_code());
  EXPECT_EQ(result->get_error_code(),
            mojom::TtsRequestError::kReceivedUnexpectedData);

  result = GetResultOnError(mojom::TtsRequestError::kServerError);
  EXPECT_TRUE(result->is_error_code());
  EXPECT_EQ(result->get_error_code(), mojom::TtsRequestError::kServerError);

  result = GetResultOnError(mojom::TtsRequestError::kRequestOverride);
  EXPECT_TRUE(result->is_error_code());
  EXPECT_EQ(result->get_error_code(), mojom::TtsRequestError::kRequestOverride);

  result = GetResultOnError(mojom::TtsRequestError::kEmptyUtterance);
  EXPECT_TRUE(result->is_error_code());
  EXPECT_EQ(result->get_error_code(), mojom::TtsRequestError::kEmptyUtterance);
}

TEST_F(EnhancedNetworkTtsUtilsTest, UnpackJsonResponseSucceed) {
  const std::vector<uint8_t> response_data = {1, 2, 5};
  std::string encoded_data(response_data.begin(), response_data.end());
  base::Base64Encode(encoded_data, &encoded_data);
  const std::string encoded_response =
      base::StringPrintf(kTemplateResponse, encoded_data.c_str());
  const std::unique_ptr<base::Value> json =
      base::JSONReader::ReadDeprecated(encoded_response);

  mojom::TtsResponsePtr result = UnpackJsonResponse(*json);

  EXPECT_TRUE(result->is_data());
  EXPECT_EQ(result->get_data()->audio, std::vector<uint8_t>({1, 2, 5}));
}

TEST_F(EnhancedNetworkTtsUtilsTest,
       UnpackJsonResponseFailsWithWrongResponseFormat) {
  const std::string encoded_response = "[{}, {}, {}]";
  const std::unique_ptr<base::Value> json =
      base::JSONReader::ReadDeprecated(encoded_response);

  mojom::TtsResponsePtr result = UnpackJsonResponse(*json);

  EXPECT_TRUE(result->is_error_code());
  EXPECT_EQ(result->get_error_code(),
            mojom::TtsRequestError::kReceivedUnexpectedData);
}

TEST_F(EnhancedNetworkTtsUtilsTest,
       UnpackJsonResponseFailsWithWrongDataFormat) {
  // The data is not Base64 encoded.
  const std::vector<uint8_t> response_data = {1, 2, 5};
  const std::string encoded_data(response_data.begin(), response_data.end());
  const std::string encoded_response =
      base::StringPrintf(kTemplateResponse, encoded_data.c_str());
  const std::unique_ptr<base::Value> json =
      base::JSONReader::ReadDeprecated(encoded_response);

  mojom::TtsResponsePtr result = UnpackJsonResponse(*json);

  EXPECT_TRUE(result->is_error_code());
  EXPECT_EQ(result->get_error_code(),
            mojom::TtsRequestError::kReceivedUnexpectedData);
}

}  // namespace enhanced_network_tts
}  // namespace ash
