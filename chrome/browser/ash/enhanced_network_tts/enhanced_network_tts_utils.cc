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

std::string FormatJsonRequest(const mojom::TtsRequestPtr tts_request) {
  // utterance is sent as {'text': {'text_parts': [<utterance>]} }
  base::Value text_parts(base::Value::Type::LIST);
  text_parts.Append(std::move(tts_request->utterance));
  base::Value text(base::Value::Type::DICTIONARY);
  text.SetKey("text_parts", std::move(text_parts));
  base::Value request(base::Value::Type::DICTIONARY);
  request.SetKey("text", std::move(text));

  // Voice and language are sent as
  // {
  //   {'advanced_options':
  //     {'force_language':<lang>}
  //   },
  //   {'voice_settings':
  //     {'voice_criteria_and_selections':
  //       [{
  //          'selection': {'default_voice':<voice>}},
  //          'criteria': {'language':<lang>}}
  //       }]
  //     }
  //   }
  // }
  // The voice and language have to be set together to be valid.
  // See https://goto.google.com/readaloud-proto for more information.
  if (tts_request->voice.has_value() && tts_request->lang.has_value()) {
    const std::string lang = tts_request->lang.value();

    // Force the server to produce audio based on the current lang.
    base::Value advanced_options(base::Value::Type::DICTIONARY);
    advanced_options.SetKey("force_language", base::Value(lang));
    request.SetKey("advanced_options", std::move(advanced_options));

    // Produce 'voice_settings'.
    base::Value selection(base::Value::Type::DICTIONARY);
    selection.SetKey("default_voice",
                     base::Value(std::move(tts_request->voice.value())));
    base::Value criteria(base::Value::Type::DICTIONARY);
    criteria.SetKey("language", base::Value(lang));
    base::Value voice_selection(base::Value::Type::DICTIONARY);
    voice_selection.SetKey("selection", std::move(selection));
    voice_selection.SetKey("criteria", std::move(criteria));
    base::Value voice_criteria_and_selections(base::Value::Type::LIST);
    voice_criteria_and_selections.Append(std::move(voice_selection));
    base::Value voice_settings(base::Value::Type::DICTIONARY);
    voice_settings.SetKey("voice_criteria_and_selections",
                          std::move(voice_criteria_and_selections));
    request.SetKey("voice_settings", std::move(voice_settings));
  }

  std::string json_request;
  base::JSONWriter::Write(request, &json_request);
  return json_request;
}

mojom::TtsResponsePtr GetResultOnError(
    const mojom::TtsRequestError error_code) {
  // TODO(crbug.com/1217301): Log errors.
  return mojom::TtsResponse::NewErrorCode(error_code);
}

mojom::TtsResponsePtr UnpackJsonResponse(const base::Value& json_data) {
  base::Value::ConstListView list_data = json_data.GetList();

  // Depending on the size of input text (n), the list size should be 1 + 2n.
  // The first item in the list is "metadata", then each input text has one
  // dictionary for "text" and another dictionary for "audio". Since we only
  // have one input text (assuming one paragraph only), we should only have a
  // list with a size of three.
  if (list_data.size() != 3) {
    DVLOG(1)
        << "HTTP response for Enhance Network TTS has unexpected JSON data.";
    return GetResultOnError(mojom::TtsRequestError::kReceivedUnexpectedData);
  }

  // Decode timing information. Inside the "text" dictionary, the "timingInfo"
  // is encoded as:
  // "timingInfo":[
  //   {
  //      "text":<string>
  //      "location":{
  //        "textLocation": {"length": <int32>, "offset": <int32>},
  //        "timeLocation": { "timeOffset": <string>, "duration": <string> },
  //        "paragraphTextLocation": {"offset": <int32>, "length": <int32>},
  //   },
  //   ...
  // ]
  std::vector<mojom::TimingInfoPtr> timing_infos;
  const base::Value& text_dict = list_data[1];
  const base::Value* timing_info_ptr =
      text_dict.FindListPath("text.timingInfo");
  if (timing_info_ptr == nullptr) {
    DVLOG(1) << "HTTP response for Enhance Network TTS has unexpected timing "
                "info data.";
    return GetResultOnError(mojom::TtsRequestError::kReceivedUnexpectedData);
  }

  base::Value::ConstListView timing_info_list = timing_info_ptr->GetList();
  for (size_t i = 0; i < timing_info_list.size(); ++i) {
    const base::Value& timing_info = timing_info_list[i];
    const std::string* timing_info_text_ptr = timing_info.FindStringKey("text");
    const std::string* timing_info_timeoffset_ptr =
        timing_info.FindStringPath("location.timeLocation.timeOffset");
    const std::string* timing_info_duration_ptr =
        timing_info.FindStringPath("location.timeLocation.duration");
    // The first item in the timing_info_list does not have a text offset, we
    // default that to 0.
    const absl::optional<int> timing_info_text_offset =
        i == 0 ? 0 : timing_info.FindIntPath("location.textLocation.offset");

    if (timing_info_text_offset == absl::nullopt || !timing_info_text_ptr ||
        !timing_info_timeoffset_ptr || !timing_info_duration_ptr) {
      continue;
    }
    timing_infos.push_back(mojom::TimingInfo::New(
        *timing_info_text_ptr, timing_info_text_offset.value(),
        *timing_info_timeoffset_ptr, *timing_info_duration_ptr));
  }

  // Decode audio data.
  const base::Value& audio_dict = list_data[2];
  const std::string* audio_bytes_ptr = audio_dict.FindStringPath("audio.bytes");
  if (audio_bytes_ptr == nullptr) {
    DVLOG(1) << "HTTP response for Enhance Network TTS has unexpected audio "
                "bytes data.";
    return GetResultOnError(mojom::TtsRequestError::kReceivedUnexpectedData);
  }
  std::string audio_bytes = *audio_bytes_ptr;
  if (!base::Base64Decode(audio_bytes, &audio_bytes)) {
    DVLOG(1) << "Failed to decode the audio data for Enhance Network TTS.";
    return GetResultOnError(mojom::TtsRequestError::kReceivedUnexpectedData);
  }

  std::vector<uint8_t> audio =
      std::vector<uint8_t>(audio_bytes.begin(), audio_bytes.end());
  mojom::TtsDataPtr tts_data =
      mojom::TtsData::New(std::move(audio), std::move(timing_infos));
  // Send the decoded data to the caller.
  return mojom::TtsResponse::NewData(std::move(tts_data));
}

}  // namespace enhanced_network_tts
}  // namespace ash
