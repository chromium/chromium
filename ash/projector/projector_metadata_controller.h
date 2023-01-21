// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PROJECTOR_PROJECTOR_METADATA_CONTROLLER_H_
#define ASH_PROJECTOR_PROJECTOR_METADATA_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/projector/projector_metadata_model.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

// The controller in charge of recording and saving screencast metadata.
class ASH_EXPORT ProjectorMetadataController {
 public:
  ProjectorMetadataController();
  ProjectorMetadataController(const ProjectorMetadataController&) = delete;
  ProjectorMetadataController& operator=(const ProjectorMetadataController&) =
      delete;
  virtual ~ProjectorMetadataController();

  // Invoked when recording is started to create a new instance of metadata
  // model. Virtual for testing.
  virtual void OnRecordingStarted();
  // Records the transcript in metadata. Virtual for testing.
  virtual void RecordTranscription(
      const media::SpeechRecognitionResult& speech_result);
  // Records the state of the transcription. Virtual for testing.
  virtual void SetSpeechRecognitionStatus(RecognitionStatus status);
  // Marks the next transcript as the beginning of a key idea.
  // Virtual for testing.
  virtual void RecordKeyIdea();
  // Serializes the metadata and saves in storage. Virtual for testing.
  virtual void SaveMetadata(const base::FilePath& video_file_path);

  void SetProjectorMetadataModelForTest(
      std::unique_ptr<ProjectorMetadata> metadata);

 protected:
  // Triggered after finish saving the metadata file.
  virtual void OnSaveFileResult(const base::FilePath& path,
                                size_t transcripts_count,
                                bool success);

 private:
  std::unique_ptr<ProjectorMetadata> metadata_;

  base::WeakPtrFactory<ProjectorMetadataController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_PROJECTOR_PROJECTOR_METADATA_CONTROLLER_H_
