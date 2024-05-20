// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_metadata_model.h"

#include <string_view>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {
namespace {

constexpr std::array<char, 3> kSentenceEndPunctuations = {'.', '?', '!'};
constexpr std::array<char16_t, 6> kCJKSentenceEndPunctuations = {
    u'。', u'？', u'！', u'.', u'?', u'!'};
constexpr std::string_view kStartOffsetKey = "startOffset";
constexpr std::string_view kEndOffsetKey = "endOffset";
constexpr std::string_view kTextKey = "text";
constexpr std::string_view kHypothesisPartsKey = "hypothesisParts";
constexpr std::string_view kCaptionLanguage = "captionLanguage";
constexpr std::string_view kCaptionsKey = "captions";
constexpr std::string_view kKeyIdeasKey = "tableOfContent";
constexpr std::string_view kOffset = "offset";
constexpr std::string_view kRecognitionStatus = "recognitionStatus";
constexpr std::string_view kMetadataVersionNumber = "version";
constexpr std::string_view kGroupIdKey = "groupId";

// Source of common English abbreviations: icu's sentence break exception list
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/icu/source/data/brkitr/en.txt.
constexpr auto kEnglishAbbreviationsInLowerCase =
    base::MakeFixedFlatSet<std::string>(
        {"l.p.",    "alt.", "approx.", "e.g.",     "o.",    "maj.",   "misc.",
         "p.o.",    "j.d.", "jam.",    "card.",    "dec.",  "sept.",  "mr.",
         "long.",   "hat.", "g.",      "link.",    "dc.",   "d.c.",   "m.t.",
         "hz.",     "mrs.", "by.",     "act.",     "var.",  "n.v.",   "aug.",
         "b.",      "s.a.", "up.",     "job.",     "num.",  "m.i.t.", "ok.",
         "org.",    "ex.",  "cont.",   "u.",       "mart.", "fn.",    "abs.",
         "lt.",     "z.",   "e.",      "kb.",      "est.",  "a.m.",   "l.a.",
         "prof.",   "u.s.", "nov.",    "ph.d.",    "mar.",  "i.t.",   "exec.",
         "jan.",    "n.y.", "x.",      "md.",      "op.",   "vs.",    "d.a.",
         "a.d.",    "r.l.", "p.m.",    "or.",      "m.r.",  "cap.",   "pc.",
         "feb.",    "i.e.", "sep.",    "gb.",      "k.",    "u.s.c.", "mt.",
         "s.",      "a.s.", "c.o.d.",  "capt.",    "col.",  "in.",    "c.f.",
         "adj.",    "ad.",  "i.d.",    "mgr.",     "r.t.",  "b.v.",   "m.",
         "conn.",   "yr.",  "rev.",    "phys.",    "pp.",   "ms.",    "to.",
         "sgt.",    "j.k.", "nr.",     "jun.",     "fri.",  "s.a.r.", "lev.",
         "lt.cdr.", "def.", "f.",      "do.",      "joe.",  "id.",    "dept.",
         "is.",     "pvt.", "diff.",   "hon.b.a.", "q.",    "mb.",    "on.",
         "min.",    "j.b.", "ed.",     "ab.",      "a.",    "s.p.a.", "i.",
         "comm.",   "go.",  "l.",      "all.",     "p.v.",  "t.",     "k.r.",
         "etc.",    "d.",   "adv.",    "lib.",     "pro.",  "u.s.a.", "s.e.",
         "aa.",     "rep.", "sq.",     "as."});

base::Value::Dict HypothesisPartsToDict(
    const media::HypothesisParts& hypothesis_parts) {
  base::Value::List text_list;
  for (auto& part : hypothesis_parts.text)
    text_list.Append(part);

  base::Value::Dict hypothesis_part_dict;
  hypothesis_part_dict.Set(kTextKey, std::move(text_list));
  hypothesis_part_dict.Set(
      kOffset, static_cast<int>(
                   hypothesis_parts.hypothesis_part_offset.InMilliseconds()));
  return hypothesis_part_dict;
}

std::vector<media::HypothesisParts> recalculateHypothesisPartTimeStamps(
    std::vector<media::HypothesisParts> sentence) {
  if (sentence.empty()) {
    return sentence;
  }
  const base::TimeDelta start_timestamp = sentence.at(0).hypothesis_part_offset;
  for (auto& hypothesisPart : sentence) {
    hypothesisPart.hypothesis_part_offset -= start_timestamp;
  }
  return sentence;
}

bool isCJKLanguage(const std::string& caption_language) {
  // CJK languages use different sentence end punctuations.
  return caption_language.starts_with("zh") ||
         caption_language.starts_with("ja") ||
         caption_language.starts_with("ko");
}

bool isEndOfSentence(const std::string& word,
                     const std::string& caption_language) {
  if (word.empty()) {
    return false;
  }

  if (base::Contains(kSentenceEndPunctuations, word.back())) {
    if (caption_language.starts_with("en") &&
        kEnglishAbbreviationsInLowerCase.contains(base::ToLowerASCII(word))) {
      // This is an English abbreviation, not end of a sentence.
      return false;
    }
    return true;
  }
  return false;
}

bool isEndOfCJKSentence(std::u16string word) {
  return !word.empty() &&
         base::Contains(kCJKSentenceEndPunctuations, word.back());
}

std::vector<std::vector<media::HypothesisParts>>
GetSentenceLevelHypothesisParts(
    std::vector<media::HypothesisParts> paragraph_hypothesis_parts,
    const std::string& caption_language) {
  // Split HypothesisParts of a paragraph into sentences.
  std::vector<std::vector<media::HypothesisParts>> sentence_hypothesis_parts;
  bool new_sentence = true;
  for (media::HypothesisParts& hypothesisPart : paragraph_hypothesis_parts) {
    if (new_sentence) {
      sentence_hypothesis_parts.emplace_back();
      new_sentence = false;
    }
    const std::string& original_word = hypothesisPart.text[0];
    new_sentence = isCJKLanguage(caption_language)
                       ? isEndOfCJKSentence(base::UTF8ToUTF16(original_word))
                       : isEndOfSentence(original_word, caption_language);
    sentence_hypothesis_parts.back().push_back(std::move(hypothesisPart));
  }
  return sentence_hypothesis_parts;
}

std::vector<std::unique_ptr<ProjectorTranscript>> SplitTranscriptIntoSentences(
    std::unique_ptr<ProjectorTranscript> paragraph_transcript,
    const std::string& caption_language) {
  std::vector<std::unique_ptr<ProjectorTranscript>> sentence_transcripts;
  const base::TimeDelta& paragraph_start_time =
      paragraph_transcript->start_time();
  const base::TimeDelta& paragraph_end_time = paragraph_transcript->end_time();
  std::vector<media::HypothesisParts>& paragraph_hypothesis_parts =
      paragraph_transcript->hypothesis_parts();
  if (paragraph_hypothesis_parts.empty()) {
    // No timing information, return a single transcript.
    sentence_transcripts.push_back(std::move(paragraph_transcript));
    return sentence_transcripts;
  }

  std::vector<std::vector<media::HypothesisParts>> sentence_hypothesis_parts =
      GetSentenceLevelHypothesisParts(std::move(paragraph_hypothesis_parts),
                                      caption_language);
  base::TimeDelta sentence_start_time = paragraph_start_time;
  base::TimeDelta sentence_end_time;
  const std::u16string full_text =
      base::UTF8ToUTF16(paragraph_transcript->text());
  size_t previous_sentence_end_pos = 0;
  for (uint i = 0; i < sentence_hypothesis_parts.size(); ++i) {
    std::vector<media::HypothesisParts> current_sentence_hypothesis_parts =
        recalculateHypothesisPartTimeStamps(
            std::move(sentence_hypothesis_parts[i]));
    // End timestamp for current sentence is:
    // 1. Start timestamp of next sentence plus paragraph start time if there is
    // a next sentence;
    // 2. End timestamp of the paragraph if it is the last sentence.
    sentence_end_time =
        i < sentence_hypothesis_parts.size() - 1
            ? sentence_hypothesis_parts[i + 1][0].hypothesis_part_offset +
                  paragraph_start_time
            : paragraph_end_time;
    std::u16string sentence_text = u"";
    if (current_sentence_hypothesis_parts.size() > 0) {
      std::u16string sentence_end_word =
          base::UTF8ToUTF16(current_sentence_hypothesis_parts.back().text[0]);

      // Remove the delimiter character sometimes added by the speech service.
      base::RemoveChars(sentence_end_word, u"\u2581", &sentence_end_word);
      const size_t current_sentence_end_pos =
          full_text.find(sentence_end_word, previous_sentence_end_pos) +
          sentence_end_word.length();
      sentence_text = full_text.substr(
          previous_sentence_end_pos,
          (current_sentence_end_pos - previous_sentence_end_pos));
      base::TrimString(sentence_text, u" ", &sentence_text);
      previous_sentence_end_pos = current_sentence_end_pos;
    }
    sentence_transcripts.push_back(std::make_unique<ProjectorTranscript>(
        sentence_start_time, sentence_end_time,
        /*group_id=*/paragraph_start_time.InMilliseconds(),
        base::UTF16ToUTF8(sentence_text), current_sentence_hypothesis_parts));
    // Next sentence's start timestamp is current sentence's end timestamp.
    sentence_start_time = sentence_end_time;
  }
  return sentence_transcripts;
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
base::Value::Dict ProjectorKeyIdea::ToJson() {
  auto transcript =
      base::Value::Dict()
          .Set(kStartOffsetKey, static_cast<int>(start_time_.InMilliseconds()))
          .Set(kEndOffsetKey, static_cast<int>(end_time_.InMilliseconds()))
          .Set(kTextKey, text_);
  return transcript;
}

ProjectorTranscript::ProjectorTranscript(
    const base::TimeDelta start_time,
    const base::TimeDelta end_time,
    const int group_id,
    const std::string& text,
    const std::vector<media::HypothesisParts>& hypothesis_parts)
    : MetadataItem(start_time, end_time, text),
      group_id_(group_id),
      hypothesis_parts_(hypothesis_parts) {}

ProjectorTranscript::~ProjectorTranscript() = default;

// The JSON we generate looks like this:
//  {
//      "startOffset": 100
//      "endOffset": 2100
//      "text": "Today I would like to teach..."
//      "groupId": 100
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
//   "groupId": INT
//   "hypothesisParts": DICT LIST
//
//
base::Value::Dict ProjectorTranscript::ToJson() {
  base::Value::Dict transcript;
  transcript.Set(kStartOffsetKey,
                 static_cast<int>(start_time_.InMilliseconds()));
  transcript.Set(kEndOffsetKey, static_cast<int>(end_time_.InMilliseconds()));
  transcript.Set(kTextKey, text_);

  base::Value::List hypothesis_parts_list;
  for (auto& hypothesis_part : hypothesis_parts_)
    hypothesis_parts_list.Append(HypothesisPartsToDict(hypothesis_part));

  transcript.Set(kHypothesisPartsKey, std::move(hypothesis_parts_list));
  transcript.Set(kGroupIdKey, group_id_);
  return transcript;
}

ProjectorMetadata::ProjectorMetadata() = default;
ProjectorMetadata::~ProjectorMetadata() = default;

void ProjectorMetadata::SetCaptionLanguage(const std::string& language) {
  caption_language_ = language;
}

void ProjectorMetadata::AddTranscript(
    std::unique_ptr<ProjectorTranscript> transcript) {
  std::vector<std::unique_ptr<ProjectorTranscript>> sentence_transcripts =
      SplitTranscriptIntoSentences(std::move(transcript), caption_language_);
  AddSentenceTranscripts(std::move(sentence_transcripts));
}

void ProjectorMetadata::AddSentenceTranscripts(
    std::vector<std::unique_ptr<ProjectorTranscript>> sentence_transcripts) {
  for (std::unique_ptr<ProjectorTranscript>& sentence_transcript :
       sentence_transcripts) {
    transcripts_.push_back(std::move(sentence_transcript));
  }
}

void ProjectorMetadata::SetSpeechRecognitionStatus(RecognitionStatus status) {
  speech_recognition_status_ = status;
}

void ProjectorMetadata::SetMetadataVersionNumber(
    MetadataVersionNumber version) {
  metadata_version_number_ = version;
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
//    "version": 2,
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
base::Value::Dict ProjectorMetadata::ToJson() {
  base::Value::Dict metadata;
  metadata.Set(kCaptionLanguage, caption_language_);

  base::Value::List captions_list;
  for (auto& transcript : transcripts_)
    captions_list.Append(transcript->ToJson());
  metadata.Set(kCaptionsKey, std::move(captions_list));

  base::Value::List key_ideas_list;
  for (auto& key_idea : key_ideas_)
    key_ideas_list.Append(key_idea->ToJson());
  metadata.Set(kKeyIdeasKey, std::move(key_ideas_list));
  metadata.Set(kRecognitionStatus,
               static_cast<int>(speech_recognition_status_));
  metadata.Set(kMetadataVersionNumber,
               static_cast<int>(metadata_version_number_));
  return metadata;
}

}  // namespace ash
