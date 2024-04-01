// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_METADATA_MODEL_H_
#define ASH_PROJECTOR_PROJECTOR_METADATA_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "base/values.h"
#include "media/mojo/mojom/speech_recognition_result.h"

namespace ash {

// The speech recognition status.
enum class ASH_EXPORT RecognitionStatus : int {
  // Speech recognition was incomplete by the time the metadata
  // was written.
  kIncomplete = 0,
  // Speech recognition was completed by the time the metadata was
  // written to file.
  kComplete = 1,
  // Speech recognition had encountered an error.
  kError = 2
};

enum class ASH_EXPORT MetadataVersionNumber : int {
  kUnknown = 0,
  kV1 = 1,
  kV2 = 2
};

// Base class to describe a metadata item.
class MetadataItem {
 public:
  MetadataItem(const base::TimeDelta start_time,
               const base::TimeDelta end_time,
               const std::string& text);
  MetadataItem(const MetadataItem&) = delete;
  MetadataItem& operator=(const MetadataItem&) = delete;
  virtual ~MetadataItem();

  base::TimeDelta& start_time() { return start_time_; }

  base::TimeDelta& end_time() { return end_time_; }

  std::string& text() { return text_; }

  // Return the serialized metadata item. This is used for storage.
  virtual base::Value::Dict ToJson() = 0;

 protected:
  // The start time of the metadata item from the start of the recording
  // session.
  base::TimeDelta start_time_;
  // The end time of the metadata item from the start of the recording session.
  base::TimeDelta end_time_;
  // Text data of the metadata item.
  std::string text_;
};

// Class to describe a key idea.
class ASH_EXPORT ProjectorKeyIdea : public MetadataItem {
 public:
  ProjectorKeyIdea(const base::TimeDelta start_time,
                   const base::TimeDelta end_time,
                   const std::string& text = std::string());
  ProjectorKeyIdea(const ProjectorKeyIdea&) = delete;
  ProjectorKeyIdea& operator=(const ProjectorKeyIdea&) = delete;
  ~ProjectorKeyIdea() override;

  base::Value::Dict ToJson() override;
};

// Class to describe a transcription.
class ASH_EXPORT ProjectorTranscript : public MetadataItem {
 public:
  ProjectorTranscript(
      const base::TimeDelta start_time,
      const base::TimeDelta end_time,
      int group_id,
      const std::string& text,
      const std::vector<media::HypothesisParts>& hypothesis_parts);
  ProjectorTranscript(const ProjectorTranscript&) = delete;
  ProjectorTranscript& operator=(const ProjectorTranscript&) = delete;
  ~ProjectorTranscript() override;

  base::Value::Dict ToJson() override;

  std::vector<media::HypothesisParts>& hypothesis_parts() {
    return hypothesis_parts_;
  }

 private:
  const int group_id_;
  std::vector<media::HypothesisParts> hypothesis_parts_;
};

// Class to describe a projector metadata of a screencast session, including
// name, transcriptions, key_ideas, etc
class ASH_EXPORT ProjectorMetadata {
 public:
  ProjectorMetadata();
  ProjectorMetadata(const ProjectorMetadata&) = delete;
  ProjectorMetadata& operator=(const ProjectorMetadata&) = delete;
  ~ProjectorMetadata();

  // Sets the language of the transcript.
  void SetCaptionLanguage(const std::string& language);

  // Adds the transcript to the metadata.
  void AddTranscript(std::unique_ptr<ProjectorTranscript> transcript);

  // Notifies the metadata that transcription has completed.
  void SetSpeechRecognitionStatus(RecognitionStatus status);
  // Marks a beginning of a key idea. The timing info of the next transcript
  // will be used as the timing of the key idea.
  void SetMetadataVersionNumber(MetadataVersionNumber version);
  void MarkKeyIdea();
  // Serializes the metadata for storage.
  std::string Serialize();
  // Returns the number of transcripts.
  size_t GetTranscriptsCount() const { return transcripts_.size(); }

 private:
  base::Value::Dict ToJson();
  // Add sentence transcripts to the metadata.
  void AddSentenceTranscripts(
      std::vector<std::unique_ptr<ProjectorTranscript>> sentence_transcripts);
  std::vector<std::unique_ptr<ProjectorTranscript>> transcripts_;
  std::vector<std::unique_ptr<ProjectorKeyIdea>> key_ideas_;
  std::string caption_language_;

  // True if user mark the transcript as a key idea. It will be reset to false
  // when the final recognition result is received and recorded as a key idea.
  bool should_mark_key_idea_ = false;

  // The speech recognition status.
  RecognitionStatus speech_recognition_status_ = RecognitionStatus::kIncomplete;
  MetadataVersionNumber metadata_version_number_;
};

}  // namespace ash
#endif  // ASH_PROJECTOR_PROJECTOR_METADATA_MODEL_H_
