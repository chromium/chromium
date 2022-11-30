// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_metadata_model.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kSerializedKeyIdeaTemplate[] = R"({
  "endOffset": %i,
  "startOffset": %i,
  "text": "%s"
})";

constexpr char kSerializedTranscriptTemplate[] = R"({
  "endOffset": %i,
  "startOffset": %i,
  "text": "%s",
  "hypothesisParts": %s
})";

constexpr char kSerializedHypothesisPartTemplate[] = R"({
  "text": %s,
  "offset": %i
})";

void AssertSerializedString(const std::string& expected,
                            const std::string& actual) {
  absl::optional<base::Value> expected_value = base::JSONReader::Read(expected);
  ASSERT_TRUE(expected_value);
  std::string expected_serialized_value;
  base::JSONWriter::Write(expected_value.value(), &expected_serialized_value);
  EXPECT_EQ(expected_serialized_value, actual);
}

std::string BuildKeyIdeaJson(int start_offset,
                             int end_offset,
                             const std::string& text) {
  return base::StringPrintf(kSerializedKeyIdeaTemplate, end_offset,
                            start_offset, text.c_str());
}

std::string BuildHypothesisParts(
    const media::HypothesisParts& hypothesis_parts) {
  std::stringstream ss;
  ss << "[";
  for (uint i = 0; i < hypothesis_parts.text.size(); i++) {
    ss << "\"" << hypothesis_parts.text[i] << "\"";
    if (i < hypothesis_parts.text.size() - 1)
      ss << ", ";
  }
  ss << "]";

  return base::StringPrintf(
      kSerializedHypothesisPartTemplate, ss.str().c_str(),
      int(hypothesis_parts.hypothesis_part_offset.InMilliseconds()));
}

std::string BuildHypothesisPartsList(
    const std::vector<media::HypothesisParts>& hypothesis_parts_vector) {
  std::stringstream ss;
  ss << "[";
  for (uint i = 0; i < hypothesis_parts_vector.size(); i++) {
    ss << BuildHypothesisParts(hypothesis_parts_vector[i]);
    if (i < hypothesis_parts_vector.size() - 1)
      ss << ", ";
  }
  ss << "]";
  return ss.str();
}

std::string BuildTranscriptJson(
    int start_offset,
    int end_offset,
    const std::string& text,
    const std::vector<media::HypothesisParts>& hypothesis_part) {
  return base::StringPrintf(kSerializedTranscriptTemplate, end_offset,
                            start_offset, text.c_str(),
                            BuildHypothesisPartsList(hypothesis_part).c_str());
}

}  // namespace

class ProjectorKeyIdeaTest : public testing::Test {
 public:
  ProjectorKeyIdeaTest() = default;

  ProjectorKeyIdeaTest(const ProjectorKeyIdeaTest&) = delete;
  ProjectorKeyIdeaTest& operator=(const ProjectorKeyIdeaTest&) = delete;
};

TEST_F(ProjectorKeyIdeaTest, ToJson) {
  ProjectorKeyIdea key_idea(
      /*start_time=*/base::Milliseconds(1000),
      /*end_time=*/base::Milliseconds(3000));

  std::string key_idea_str;
  base::JSONWriter::Write(key_idea.ToJson(), &key_idea_str);

  AssertSerializedString(BuildKeyIdeaJson(1000, 3000, std::string()),
                         key_idea_str);
}

TEST_F(ProjectorKeyIdeaTest, ToJsonWithText) {
  ProjectorKeyIdea key_idea(
      /*start_time=*/base::Milliseconds(1000),
      /*end_time=*/base::Milliseconds(3000), "Key idea text");

  std::string key_idea_str;
  base::JSONWriter::Write(key_idea.ToJson(), &key_idea_str);

  AssertSerializedString(BuildKeyIdeaJson(1000, 3000, "Key idea text"),
                         key_idea_str);
}

class ProjectorTranscriptTest : public testing::Test {
 public:
  ProjectorTranscriptTest() = default;

  ProjectorTranscriptTest(const ProjectorTranscriptTest&) = delete;
  ProjectorTranscriptTest& operator=(const ProjectorTranscriptTest&) = delete;
};

TEST_F(ProjectorTranscriptTest, ToJson) {
  std::vector<media::HypothesisParts> hypothesis_parts;
  hypothesis_parts.emplace_back(std::vector<std::string>({"transcript"}),
                                base::Milliseconds(1000));
  hypothesis_parts.emplace_back(std::vector<std::string>({"text"}),
                                base::Milliseconds(2000));

  const auto expected_transcript =
      BuildTranscriptJson(1000, 3000, "transcript text", hypothesis_parts);

  ProjectorTranscript transcript(
      /*start_time=*/base::Milliseconds(1000),
      /*end_time=*/base::Milliseconds(3000), "transcript text",
      std::move(hypothesis_parts));

  std::string transcript_str;
  base::JSONWriter::Write(transcript.ToJson(), &transcript_str);

  AssertSerializedString(expected_transcript, transcript_str);
}

class ProjectorMetadataTest : public testing::Test {
 public:
  ProjectorMetadataTest() = default;

  ProjectorMetadataTest(const ProjectorMetadataTest&) = delete;
  ProjectorMetadataTest& operator=(const ProjectorMetadataTest&) = delete;
};

TEST_F(ProjectorMetadataTest, Serialize) {
  const char kExpectedMetaData[] = R"({
    "captions": [
      {
        "endOffset": 3000,
        "hypothesisParts": [
          {
            "offset": 1000,
            "text": [
              "transcript"
            ]
          },
          {
            "offset": 2000,
            "text": [
              "text"
            ]
          }
        ],
        "startOffset": 1000,
        "text": "transcript text"
      },
      {
        "endOffset": 5000,
        "hypothesisParts": [
          {
            "offset": 3200,
            "text": [
              "transcript"
            ]
          },
          {
            "offset": 4200,
            "text": [
              "text"
            ]
          },
          {
            "offset": 4500,
            "text": [
              "2"
            ]
          }
        ],
        "startOffset": 3000,
        "text": "transcript text 2"
      }
    ],
    "captionLanguage": "en",
    "tableOfContent": [
      {
        "endOffset": 5000,
        "startOffset": 3000,
        "text": ""
      }
    ]
  })";

  ProjectorMetadata metadata;
  metadata.SetCaptionLanguage("en");

  std::vector<media::HypothesisParts> first_transcript;
  first_transcript.emplace_back(std::vector<std::string>({"transcript"}),
                                base::Milliseconds(1000));
  first_transcript.emplace_back(std::vector<std::string>({"text"}),
                                base::Milliseconds(2000));

  metadata.AddTranscript(std::make_unique<ProjectorTranscript>(
      /*start_time=*/base::Milliseconds(1000),
      /*end_time=*/base::Milliseconds(3000), "transcript text",
      std::move(first_transcript)));

  metadata.MarkKeyIdea();

  std::vector<media::HypothesisParts> second_transcript;
  second_transcript.emplace_back(std::vector<std::string>({"transcript"}),
                                 base::Milliseconds(3200));
  second_transcript.emplace_back(std::vector<std::string>({"text"}),
                                 base::Milliseconds(4200));
  second_transcript.emplace_back(std::vector<std::string>({"2"}),
                                 base::Milliseconds(4500));

  metadata.AddTranscript(std::make_unique<ProjectorTranscript>(
      /*start_time=*/base::Milliseconds(3000),
      /*end_time=*/base::Milliseconds(5000), "transcript text 2",
      std::move(second_transcript)));

  AssertSerializedString(kExpectedMetaData, metadata.Serialize());
}

}  // namespace ash
