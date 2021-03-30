// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_metadata_model.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace ash {
namespace {

using base::ListValue;
using base::Value;

constexpr base::StringPiece kStartOffsetKey = "startOffset";
constexpr base::StringPiece kEndOffsetKey = "endOffset";
constexpr base::StringPiece kTextKey = "text";
constexpr base::StringPiece kWordAlignmentKey = "wordAlignment";
constexpr base::StringPiece kNameKey = "name";
constexpr base::StringPiece kCaptionsKey = "captions";
constexpr base::StringPiece kKeyIdeasKey = "tableOfContent";

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
    const std::vector<base::TimeDelta>& word_alignments)
    : MetadataItem(start_time, end_time, text),
      word_alignments_(word_alignments) {}

ProjectorTranscript::~ProjectorTranscript() = default;

// The JSON we generate looks like this:
//  {
//      "startOffset": 100
//      "endOffset": 2100
//      "text": "Today I'd like to teach..."
//      "wordAlignments": [
//         100,
//         1500,
//      ]
//  }
//
// Which is:
// DICT
//   "startOffset": INT
//   "endOffset": INT
//   "text": STRING
//   "wordAlignments": LIST
base::Value ProjectorTranscript::ToJson() {
  base::Value transcript(base::Value::Type::DICTIONARY);
  transcript.SetIntKey(kStartOffsetKey, start_time_.InMilliseconds());
  transcript.SetIntKey(kEndOffsetKey, end_time_.InMilliseconds());
  transcript.SetStringKey(kTextKey, text_);

  base::Value word_alignments_value(base::Value::Type::LIST);
  for (auto& word_alignment : word_alignments_)
    word_alignments_value.Append((int)word_alignment.InMilliseconds());
  transcript.SetKey(kWordAlignmentKey, std::move(word_alignments_value));
  return transcript;
}

ProjectorMetadata::ProjectorMetadata() = default;
ProjectorMetadata::~ProjectorMetadata() = default;

void ProjectorMetadata::AddTranscript(
    std::unique_ptr<ProjectorTranscript> transcript) {
  if (should_mark_key_idea_) {
    key_ideas_.push_back(std::make_unique<ProjectorKeyIdea>(
        transcript->start_time(), transcript->end_time()));
  }
  transcripts_.push_back(std::move(transcript));
  should_mark_key_idea_ = false;
}

void ProjectorMetadata::MarkKeyIdea() {
  should_mark_key_idea_ = true;
}

void ProjectorMetadata::SetName(const std::string& name) {
  name_ = name;
}

std::string ProjectorMetadata::Serialize() {
  std::string metadata_str;
  base::JSONWriter::Write(ToJson(), &metadata_str);
  return metadata_str;
}

// The JSON we generate looks like this:
//  {
//    "name": "Constructivist Learning Theory"
//    "captions": [{
//      "startOffset": 100
//      "endOffset": 2100
//      "text": "Today I'd like to teach you about a central pillar of a
//      construction learning theory it's called the debugging Loop...",
//      "wordAlignments": [
//         100,
//         1500,
//      ]
//    }],
//    "tableOfContent": [
//      {
//        "text": "Making a creation",
//        "startOffset": 4400,
//        "encodingFormat": "text/markdown",
//      },
//    ]
//  }
//
// Which is:
// DICT
//   "@type": STRING
//   "text": STRING
//   "captions": LIST
//   "tableOfContent": LIST
base::Value ProjectorMetadata::ToJson() {
  base::Value metadata(base::Value::Type::DICTIONARY);
  metadata.SetStringKey(kNameKey, name_);

  base::Value captions_value(base::Value::Type::LIST);
  for (auto& transcript : transcripts_)
    captions_value.Append(transcript->ToJson());
  metadata.SetKey(kCaptionsKey, std::move(captions_value));

  base::Value key_ideas_value(base::Value::Type::LIST);
  for (auto& key_idea : key_ideas_)
    key_ideas_value.Append(key_idea->ToJson());
  metadata.SetKey(kKeyIdeasKey, std::move(key_ideas_value));
  return metadata;
}

}  // namespace ash
