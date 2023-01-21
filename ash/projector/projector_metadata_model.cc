// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_metadata_model.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"

namespace ash {
namespace {

constexpr base::StringPiece kStartOffsetKey = "startOffset";
constexpr base::StringPiece kEndOffsetKey = "endOffset";
constexpr base::StringPiece kTextKey = "text";
constexpr base::StringPiece kHypothesisPartsKey = "hypothesisParts";
constexpr base::StringPiece kCaptionLanguage = "captionLanguage";
constexpr base::StringPiece kCaptionsKey = "captions";
constexpr base::StringPiece kKeyIdeasKey = "tableOfContent";
constexpr base::StringPiece kOffset = "offset";
constexpr base::StringPiece kRecognitionStatus = "recognitionStatus";

base::Value HypothesisPartsToValue(
    const media::HypothesisParts& hypothesis_parts) {
  base::Value text_value(base::Value::Type::LIST);
  for (auto& part : hypothesis_parts.text)
    text_value.Append(part);

  base::Value hypothesis_part_value(base::Value::Type::DICTIONARY);
  hypothesis_part_value.SetKey(kTextKey, std::move(text_value));
  hypothesis_part_value.SetIntKey(
      kOffset, hypothesis_parts.hypothesis_part_offset.InMilliseconds());
  return hypothesis_part_value;
}

}  // namespace

MetadataItem::MetadataItem(const base::TimeDelta start_time,
                           const base::TimeDelta end_time,
                           const std::string& text)
    : start_time_(start_time), end_time_(end_time), text_(text) {}

MetadataItem::~MetadataItem() = default;

ProjectorKeyIdea::ProjectorKeyIdea(const base::TimeDelta start_time,
                                   const base::TimeDelta end_time,
                                   const std::string& text)
    : MetadataItem(start_time, end_time, text) {}

ProjectorKeyIdea::~ProjectorKeyIdea() = default;

// The JSON we generate looks like this:
//  {
//      "startOffset": 100
//      "endOffset": 2100
//      "text": "Today I'd like to teach..."
//  }
//
// Which is:
// DICT
//   "startOffset": INT
//   "endOffset": INT
//   "text": STRING
base::Value ProjectorKeyIdea::ToJson() {
  base::Value transcript(base::Value::Type::DICTIONARY);
  transcript.SetIntKey(kStartOffsetKey, start_time_.InMilliseconds());
  transcript.SetIntKey(kEndOffsetKey, end_time_.InMilliseconds());
  transcript.SetStringKey(kTextKey, text_);
  return transcript;
}

ProjectorTranscript::ProjectorTranscript(
    const base::TimeDelta start_time,
    const base::TimeDelta end_time,
    const std::string& text,
    const std::vector<media::HypothesisParts>& hypothesis_parts)
    : MetadataItem(start_time, end_time, text),
      hypothesis_parts_(hypothesis_parts) {}

ProjectorTranscript::~ProjectorTranscript() = default;

// The JSON we generate looks like this:
//  {
//      "startOffset": 100
//      "endOffset": 2100
//      "text": "Today I would like to teach..."
//      "hypothesisParts": [
//        {
//           "text": ["Today"]
//           "offset": 100
//         },
//         {
//           "text": ["I"]
//           "offset": 200
//         },
//         ...
//      ]
//  }
//
// Which is:
// DICT
//   "startOffset": INT
//   "endOffset": INT
//   "text": STRING
//   "hypothesisParts": DICT LIST
//
base::Value ProjectorTranscript::ToJson() {
  base::Value transcript(base::Value::Type::DICTIONARY);
  transcript.SetIntKey(kStartOffsetKey, start_time_.InMilliseconds());
  transcript.SetIntKey(kEndOffsetKey, end_time_.InMilliseconds());
  transcript.SetStringKey(kTextKey, text_);

  base::Value hypothesis_parts_value(base::Value::Type::LIST);
  for (auto& hypothesis_part : hypothesis_parts_)
    hypothesis_parts_value.Append(HypothesisPartsToValue(hypothesis_part));

  transcript.SetKey(kHypothesisPartsKey, std::move(hypothesis_parts_value));
  return transcript;
}

ProjectorMetadata::ProjectorMetadata() = default;
ProjectorMetadata::~ProjectorMetadata() = default;

void ProjectorMetadata::SetCaptionLanguage(const std::string& language) {
  caption_language_ = language;
}

void ProjectorMetadata::AddTranscript(
    std::unique_ptr<ProjectorTranscript> transcript) {
  if (should_mark_key_idea_) {
    key_ideas_.push_back(std::make_unique<ProjectorKeyIdea>(
        transcript->start_time(), transcript->end_time()));
  }
  transcripts_.push_back(std::move(transcript));
  should_mark_key_idea_ = false;
}

void ProjectorMetadata::SetSpeechRecognitionStatus(RecognitionStatus status) {
  speech_recognition_status_ = status;
}

void ProjectorMetadata::MarkKeyIdea() {
  should_mark_key_idea_ = true;
}

std::string ProjectorMetadata::Serialize() {
  std::string metadata_str;
  base::JSONWriter::Write(ToJson(), &metadata_str);
  return metadata_str;
}

// The JSON we generate looks like this:
//  {
//    "captionLanguage": "en"
//    "captions": [{
//      "startOffset": 100
//      "endOffset": 2100
//      "text": "Today I'd like to teach you about a central pillar of a
//      construction learning theory it's called the debugging Loop...",
//      "hypothesisParts": [
//          {
//            "text" : ["Today"],
//            "offset": 100,
//          },
//          {
//            "text": ["I"],
//            "offset": 1500,
//          }
//          ...
//      ]
//    }],
//    "tableOfContent": [
//      {
//        "endOffset" : 4500,
//        "startOffset": 4400,
//        "text": "Making a creation",
//      },
//    ],
//    "recognitionStatus": 0,
//  }
//
// Which is:
// DICT
//   "@type": STRING
//   "text": STRING
//   "captions": LIST
//   "captionLanguage": STRING
//   "tableOfContent": LIST
//   "recognitionStatus": INTEGER
base::Value ProjectorMetadata::ToJson() {
  base::Value metadata(base::Value::Type::DICTIONARY);
  metadata.SetStringKey(kCaptionLanguage, caption_language_);

  base::Value captions_value(base::Value::Type::LIST);
  for (auto& transcript : transcripts_)
    captions_value.Append(transcript->ToJson());
  metadata.SetKey(kCaptionsKey, std::move(captions_value));

  base::Value key_ideas_value(base::Value::Type::LIST);
  for (auto& key_idea : key_ideas_)
    key_ideas_value.Append(key_idea->ToJson());
  metadata.SetKey(kKeyIdeasKey, std::move(key_ideas_value));
  metadata.SetIntKey(kRecognitionStatus,
                     static_cast<int>(speech_recognition_status_));
  return metadata;
}

}  // namespace ash
