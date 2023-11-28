// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/projector_metadata_model.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
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
  "hypothesisParts": %s
})";

constexpr char kSerializedHypothesisPartTemplate[] = R"({
  "text": %s,
  "offset": %i
})";

constexpr char kCompleteMetadataTemplate[] = R"({
    "captions": [
      {
        "endOffset": 3000,
        "hypothesisParts": [
          {
            "offset": 0,
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
            "offset": 0,
            "text": [
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text"
            ]
          },
          {
            "offset": 1500,
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
    "recognitionStatus": %i,
    "tableOfContent": [
      {
        "endOffset": 5000,
        "startOffset": 3000,
        "text": ""
      }
    ]
  })";

constexpr char

    kCompleteMetadataV2Template[] = R"({
    "captions": [
      {
        "endOffset": 3000,
        "hypothesisParts": [
          {
            "offset": 0,
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
        "groupId": 1000,
        "text": "transcript text"
      },
      {
        "endOffset": 5000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text"
            ]
          },
          {
            "offset": 1500,
            "text": [
              "2"
            ]
          }
        ],
        "startOffset": 3000,
        "groupId": 3000,
        "text": "transcript text 2"
      }
    ],
    "captionLanguage": "en",
    "recognitionStatus": 1,
    "version": 2,
    "tableOfContent": []
  })";

constexpr char kCompleteMetadataV2MultipleSentenceTemplate[] = R"({
    "captions": [
      {
        "endOffset": 2000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "Transcript",
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text.",
              "text"
            ]
          }
        ],
        "startOffset": 0,
        "groupId": 0,
        "text": "Transcript text."
      },
      {
        "endOffset": 4000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "Transcript",
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text?",
              "text"
            ]
          }
        ],
        "startOffset": 2000,
        "groupId": 0,
        "text": "Transcript text?"
      },
      {
        "endOffset": 6000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "Transcript",
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text!",
              "text"
            ]
          }
        ],
        "startOffset": 4000,
        "groupId": 0,
        "text": "Transcript text!"
      },
      {
        "endOffset": 8000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "Transcript",
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text.",
              "text"
            ]
          }
        ],
        "startOffset": 6000,
        "groupId": 0,
        "text": "Transcript text."
      },

      {
        "endOffset": 10000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "Transcript",
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text.",
              "text"
            ]
          }
        ],
        "startOffset": 8000,
        "groupId": 8000,
        "text": "Transcript text."
      },
      {
        "endOffset": 12000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "Transcript",
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text?",
              "text"
            ]
          }
        ],
        "startOffset": 10000,
        "groupId": 8000,
        "text": "Transcript text?"
      },
      {
        "endOffset": 14000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "Transcript",
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text!",
              "text"
            ]
          }
        ],
        "startOffset": 12000,
        "groupId": 8000,
        "text": "Transcript text!"
      },
      {
        "endOffset": 16000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "Transcript",
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text.",
              "text"
            ]
          }
        ],
        "startOffset": 14000,
        "groupId": 8000,
        "text": "Transcript text."
      },

      {
        "endOffset": 25000,
        "hypothesisParts": [
          {
            "offset": 0,
            "text": [
              "transcript",
              "transcript"
            ]
          },
          {
            "offset": 1000,
            "text": [
              "text",
              "text"
            ]
          },
          {
            "offset": 1500,
            "text": [
              "2",
              "2"
            ]
          }
        ],
        "startOffset": 19000,
        "groupId": 19000,
        "text": "transcript text 2"
      }
    ],
    "captionLanguage": "en",
    "recognitionStatus": 1,
    "version": 2,
    "tableOfContent": []
  })";

void AssertSerializedString(const std::string& expected,
                            const std::string& actual) {
  std::optional<base::Value> expected_value = base::JSONReader::Read(expected);
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

std::unique_ptr<ProjectorMetadata> populateMetadata() {
  std::unique_ptr<ProjectorMetadata> metadata =
      std::make_unique<ProjectorMetadata>();
  metadata->SetCaptionLanguage("en");
  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);

  std::vector<media::HypothesisParts> first_transcript;
  first_transcript.emplace_back(std::vector<std::string>({"transcript"}),
                                base::Milliseconds(0));
  first_transcript.emplace_back(std::vector<std::string>({"text"}),
                                base::Milliseconds(2000));

  metadata->AddTranscript(std::make_unique<ProjectorTranscript>(
      /*start_time=*/base::Milliseconds(1000),
      /*end_time=*/base::Milliseconds(3000), 1000, "transcript text",
      std::move(first_transcript)));

  metadata->MarkKeyIdea();

  std::vector<media::HypothesisParts> second_transcript;
  second_transcript.emplace_back(std::vector<std::string>({"transcript"}),
                                 base::Milliseconds(0));
  second_transcript.emplace_back(std::vector<std::string>({"text"}),
                                 base::Milliseconds(1000));
  second_transcript.emplace_back(std::vector<std::string>({"2"}),
                                 base::Milliseconds(1500));

  metadata->AddTranscript(std::make_unique<ProjectorTranscript>(
      /*start_time=*/base::Milliseconds(3000),
      /*end_time=*/base::Milliseconds(5000), 3000, "transcript text 2",
      std::move(second_transcript)));
  return metadata;
}

std::unique_ptr<ProjectorMetadata> populateMetadataWithSentences() {
  std::unique_ptr<ProjectorMetadata> metadata =
      std::make_unique<ProjectorMetadata>();
  metadata->SetCaptionLanguage("en");
  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);

  const std::vector<std::string> paragraph_words = {
      "Transcript", "text.", "Transcript", "text?",
      "Transcript", "text!", "Transcript", "text."};
  const std::vector<std::string> noromalized_paragraph_words = {
      "transcript", "text", "transcript", "text",
      "transcript", "text", "transcript", "text"};
  std::string paragraph_text =
      "Transcript text. Transcript text? Transcript text! Transcript text.";
  std::vector<media::HypothesisParts> paragraph_hypothesis_parts;
  for (uint i = 0; i < paragraph_words.size(); i++) {
    paragraph_hypothesis_parts.emplace_back(
        std::vector<std::string>(
            {paragraph_words[i], noromalized_paragraph_words[i]}),
        base::Milliseconds(i * 1000));
  }
  const base::TimeDelta paragraph_start_offset = base::Milliseconds(0);
  const base::TimeDelta paragraph_end_offset =
      base::Milliseconds(paragraph_words.size() * 1000);

  metadata->AddTranscript(std::make_unique<ProjectorTranscript>(
      paragraph_start_offset, paragraph_end_offset,
      paragraph_start_offset.InMilliseconds(),
      base::JoinString(paragraph_words, " "), paragraph_hypothesis_parts));

  // Add another paragraph with the same text and length.
  // The group id for the new paragraph should be paragraph_end_offset (8000),
  // start timestamp should be 8000 + hypothesiePart offset.
  metadata->AddTranscript(std::make_unique<ProjectorTranscript>(
      paragraph_end_offset, paragraph_end_offset + paragraph_end_offset,
      paragraph_end_offset.InMilliseconds(),
      base::JoinString(paragraph_words, " "), paragraph_hypothesis_parts));

  metadata->MarkKeyIdea();

  std::vector<media::HypothesisParts> second_transcript;
  second_transcript.emplace_back(
      std::vector<std::string>({"transcript", "transcript"}),
      base::Milliseconds(0));
  second_transcript.emplace_back(std::vector<std::string>({"text", "text"}),
                                 base::Milliseconds(1000));
  second_transcript.emplace_back(std::vector<std::string>({"2", "2"}),
                                 base::Milliseconds(1500));

  metadata->AddTranscript(std::make_unique<ProjectorTranscript>(
      /*start_time=*/base::Milliseconds(19000),
      /*end_time=*/base::Milliseconds(25000), 9000, "transcript text 2",
      std::move(second_transcript)));
  return metadata;
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
      /*end_time=*/base::Milliseconds(3000), 1000, "transcript text",
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

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProjectorMetadataTest, Serialize) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kProjectorV2});
  std::unique_ptr<ProjectorMetadata> metadata = populateMetadata();

  metadata->SetSpeechRecognitionStatus(RecognitionStatus::kIncomplete);
  AssertSerializedString(
      base::StringPrintf(kCompleteMetadataTemplate,
                         static_cast<int>(RecognitionStatus::kIncomplete)),
      metadata->Serialize());

  metadata->SetSpeechRecognitionStatus(RecognitionStatus::kComplete);
  AssertSerializedString(
      base::StringPrintf(kCompleteMetadataTemplate,
                         static_cast<int>(RecognitionStatus::kComplete)),
      metadata->Serialize());

  metadata->SetSpeechRecognitionStatus(RecognitionStatus::kError);
  AssertSerializedString(
      base::StringPrintf(kCompleteMetadataTemplate,
                         static_cast<int>(RecognitionStatus::kError)),
      metadata->Serialize());

  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);
  // V2 feature flag not enabled, setting version number has no effort.
  AssertSerializedString(
      base::StringPrintf(kCompleteMetadataTemplate,
                         static_cast<int>(RecognitionStatus::kError)),
      metadata->Serialize());
}

TEST_F(ProjectorMetadataTest, SerializeV2) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{ash::features::kProjectorV2},
      /*disabled_features=*/{});
  std::unique_ptr<ProjectorMetadata> metadata = populateMetadata();
  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);

  metadata->SetSpeechRecognitionStatus(RecognitionStatus::kComplete);
  AssertSerializedString(kCompleteMetadataV2Template, metadata->Serialize());
}

TEST_F(ProjectorMetadataTest, AddSingleSentenceTranscriptForV2) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{ash::features::kProjectorV2},
      /*disabled_features=*/{});
  std::unique_ptr<ProjectorMetadata> metadata = populateMetadata();
  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);

  metadata->SetSpeechRecognitionStatus(RecognitionStatus::kComplete);
  AssertSerializedString(kCompleteMetadataV2Template, metadata->Serialize());
}

TEST_F(ProjectorMetadataTest, AddMultiSentenceTranscriptForV2) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{ash::features::kProjectorV2},
      /*disabled_features=*/{});
  std::unique_ptr<ProjectorMetadata> metadata = populateMetadataWithSentences();
  metadata->SetMetadataVersionNumber(MetadataVersionNumber::kV2);
  metadata->SetSpeechRecognitionStatus(RecognitionStatus::kComplete);
  // There are 4 sentences in first and second paragraph transcript, 1 in third
  // making total count 4*2 + 1 = 9.
  EXPECT_EQ(metadata->GetTranscriptsCount(), 9ul);
  AssertSerializedString(kCompleteMetadataV2MultipleSentenceTemplate,
                         metadata->Serialize());
}

}  // namespace ash
