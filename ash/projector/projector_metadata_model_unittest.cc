// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_metadata_model.h"

#include <memory>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
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
  "wordAlignment": %s
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

std::string BuildTranscriptJson(int start_offset,
                                int end_offset,
                                const std::string& text,
                                const std::string& words_alignments_str) {
  return base::StringPrintf(kSerializedTranscriptTemplate, end_offset,
                            start_offset, text.c_str(),
                            words_alignments_str.c_str());
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
      /*start_time=*/base::TimeDelta::FromMilliseconds(1000),
      /*end_time=*/base::TimeDelta::FromMilliseconds(3000));

  std::string key_idea_str;
  base::JSONWriter::Write(key_idea.ToJson(), &key_idea_str);

  AssertSerializedString(BuildKeyIdeaJson(1000, 3000, std::string()),
                         key_idea_str);
}

TEST_F(ProjectorKeyIdeaTest, ToJsonWithText) {
  ProjectorKeyIdea key_idea(
      /*start_time=*/base::TimeDelta::FromMilliseconds(1000),
      /*end_time=*/base::TimeDelta::FromMilliseconds(3000), "Key idea text");

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
  ProjectorTranscript transcript(
      /*start_time=*/base::TimeDelta::FromMilliseconds(1000),
      /*end_time=*/base::TimeDelta::FromMilliseconds(3000), "transcript text",
      {base::TimeDelta::FromMilliseconds(1000),
       base::TimeDelta::FromMilliseconds(2000)});

  std::string transcript_str;
  base::JSONWriter::Write(transcript.ToJson(), &transcript_str);

  AssertSerializedString(
      BuildTranscriptJson(1000, 3000, "transcript text", "[1000,2000]"),
      transcript_str);
}

class ProjectorMetadataTest : public testing::Test {
 public:
  ProjectorMetadataTest() = default;

  ProjectorMetadataTest(const ProjectorMetadataTest&) = delete;
  ProjectorMetadataTest& operator=(const ProjectorMetadataTest&) = delete;
};

TEST_F(ProjectorMetadataTest, Serialize) {
  const char kExpectedMetaData[] = R"({
    "name": "Screen Recording 1",
    "captions": [
      {
        "endOffset": 3000,
        "startOffset": 1000,
        "text": "transcript text",
        "wordAlignment": [1000, 2000]
      },
      {
        "endOffset": 5000,
        "startOffset": 3000,
        "text": "transcript text 2",
        "wordAlignment":[3200, 4200, 4500]
      }
    ],
    "tableOfContent":[
      {
        "endOffset": 5000,
        "startOffset": 3000,
        "text": ""
      }
    ]
  })";

  ProjectorMetadata metadata;

  metadata.SetName("Screen Recording 1");

  metadata.AddTranscript(std::make_unique<ProjectorTranscript>(
      /*start_time=*/base::TimeDelta::FromMilliseconds(1000),
      /*end_time=*/base::TimeDelta::FromMilliseconds(3000), "transcript text",
      std::initializer_list<base::TimeDelta>(
          {base::TimeDelta::FromMilliseconds(1000),
           base::TimeDelta::FromMilliseconds(2000)})));

  metadata.MarkKeyIdea();

  metadata.AddTranscript(std::make_unique<ProjectorTranscript>(
      /*start_time=*/base::TimeDelta::FromMilliseconds(3000),
      /*end_time=*/base::TimeDelta::FromMilliseconds(5000), "transcript text 2",
      std::initializer_list<base::TimeDelta>(
          {base::TimeDelta::FromMilliseconds(3200),
           base::TimeDelta::FromMilliseconds(4200),
           base::TimeDelta::FromMilliseconds(4500)})));

  AssertSerializedString(kExpectedMetaData, metadata.Serialize());
}

}  // namespace ash
