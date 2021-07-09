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

// Template for a request.
constexpr char kTemplateRequest[] = R"({
                                        "text": {
                                          "text_parts": ["%s"]
                                        }
                                      })";

// Template for a server response.
constexpr char kTemplateResponse[] = R"([{
                                      "metadata": {}
                                    }
                                    ,
                                    {
                                      "text": {}
                                    }
                                    ,
                                    {
                                      "audio": {
                                        "bytes": "%s"
                                      }
                                    }
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
  const std::string input_text = "Hello, World!";
  const std::string expected_text =
      base::StringPrintf(kTemplateRequest, input_text.c_str());
  const std::string formated_text = FormatJsonRequest(input_text);

  EXPECT_EQ(RemoveSpaceAndLineBreak(formated_text),
            RemoveSpaceAndLineBreak(expected_text));
}

TEST_F(EnhancedNetworkTtsUtilsTest, GetResultOnError) {
  std::vector<uint8_t> result = GetResultOnError();

  EXPECT_EQ(result, std::vector<uint8_t>());
}

TEST_F(EnhancedNetworkTtsUtilsTest, UnpackJsonResponseSucceed) {
  const std::vector<uint8_t> response_data = {1, 2, 5};
  std::string encoded_data(response_data.begin(), response_data.end());
  base::Base64Encode(encoded_data, &encoded_data);
  const std::string encoded_response =
      base::StringPrintf(kTemplateResponse, encoded_data.c_str());
  const std::unique_ptr<base::Value> json =
      base::JSONReader::ReadDeprecated(encoded_response);

  std::vector<uint8_t> result = UnpackJsonResponse(*json);

  EXPECT_EQ(result, std::vector<uint8_t>({1, 2, 5}));
}

TEST_F(EnhancedNetworkTtsUtilsTest,
       UnpackJsonResponseFailsWithWrongResponseFormat) {
  const std::string encoded_response = "[{}, {}, {}]";
  const std::unique_ptr<base::Value> json =
      base::JSONReader::ReadDeprecated(encoded_response);

  std::vector<uint8_t> result = UnpackJsonResponse(*json);

  EXPECT_EQ(result, std::vector<uint8_t>());
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

  std::vector<uint8_t> result = UnpackJsonResponse(*json);

  EXPECT_EQ(result, std::vector<uint8_t>());
}

}  // namespace enhanced_network_tts
}  // namespace ash
