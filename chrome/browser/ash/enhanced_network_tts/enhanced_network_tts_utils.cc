// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/enhanced_network_tts/enhanced_network_tts_utils.h"

#include <utility>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/logging.h"

namespace ash {
namespace enhanced_network_tts {

std::string FormatJsonRequest(const std::string& input_text) {
  base::Value text_parts(base::Value::Type::LIST);
  text_parts.Append(std::move(input_text));

  base::Value text(base::Value::Type::DICTIONARY);
  text.SetKey("text_parts", std::move(text_parts));

  base::Value request(base::Value::Type::DICTIONARY);
  request.SetKey("text", std::move(text));

  std::string json_request;
  base::JSONWriter::Write(request, &json_request);

  return json_request;
}

std::vector<uint8_t> GetResultOnError() {
  return std::vector<uint8_t>();
}

std::vector<uint8_t> UnpackJsonResponse(const base::Value& json_data) {
  base::Value::ConstListView list_data = json_data.GetList();

  // Depending on the size of input text (n), the list size should be 1 + 2n.
  // The first item in the list is "metadata", then each input text has one
  // dictionary for "text" and another dictionary for "audio". Since we only
  // have one input text (assuming one paragraph only), we should only have a
  // list with a size of three. For now, we only send back the "audio" data.
  // TODO(crbug.com/1217301): Send the "text" back to the caller.
  if (list_data.size() != 3 || list_data[2].FindKey("audio") == nullptr ||
      list_data[2].FindKey("audio")->FindStringKey("bytes") == nullptr) {
    DVLOG(1) << "HTTP response for Enhance Network TTS has unexpected data.";
    return GetResultOnError();
  }
  const std::string* audio_bytes_ptr =
      list_data[2].FindKey("audio")->FindStringKey("bytes");
  std::string audio_bytes = *audio_bytes_ptr;

  if (!base::Base64Decode(audio_bytes, &audio_bytes)) {
    DVLOG(1) << "Failed to decode the audio data for Enhance Network TTS.";
    return GetResultOnError();
  }

  // Send the decoded data to the caller.
  return std::vector<uint8_t>(audio_bytes.begin(), audio_bytes.end());
}

}  // namespace enhanced_network_tts
}  // namespace ash
