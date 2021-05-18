// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CONTROLLER_H_
#define ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CONTROLLER_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class ProjectorClient;

// Interface to control projector in ash.
class ASH_PUBLIC_EXPORT ProjectorController {
 public:
  ProjectorController();
  ProjectorController(const ProjectorController&) = delete;
  ProjectorController& operator=(const ProjectorController&) = delete;
  virtual ~ProjectorController();

  static ProjectorController* Get();

  // Make sure the client is set before attempting to use to the
  // ProjectorController.
  virtual void SetClient(ProjectorClient* client) = 0;

  // Called when speech recognition using SODA is available.
  virtual void OnSpeechRecognitionAvailable(bool available) = 0;

  // Called when transcription result from mic input is ready.
  virtual void OnTranscription(
      const std::u16string& text,
      absl::optional<base::TimeDelta> start_time,
      absl::optional<base::TimeDelta> end_time,
      const absl::optional<std::vector<base::TimeDelta>>& word_offsets,
      bool is_final) = 0;

  // Sets projector toolbar visibility.
  virtual void SetProjectorToolsVisible(bool is_visible) = 0;

  // Returns true if Projector is eligible to start a new session.
  virtual bool IsEligible() const = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PROJECTOR_PROJECTOR_CONTROLLER_H_
