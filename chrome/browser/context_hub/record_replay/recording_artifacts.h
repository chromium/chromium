// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_RECORDING_ARTIFACTS_H_
#define CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_RECORDING_ARTIFACTS_H_

#include <cstddef>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"

namespace context_hub {

// Represents a single user action captured during a skill recording session.
struct RecordedAction {
  RecordedAction(browser::context_hub::mojom::SkillRecordingActionType type,
                 std::string description,
                 base::TimeDelta timestamp);
  RecordedAction(const RecordedAction&) = delete;
  RecordedAction& operator=(const RecordedAction&) = delete;
  RecordedAction(RecordedAction&&) noexcept;
  RecordedAction& operator=(RecordedAction&&) noexcept;
  ~RecordedAction();

  // The category of user interaction that occurred.
  browser::context_hub::mojom::SkillRecordingActionType type;
  // A human-readable summary of the action (e.g., click coordinates).
  std::string description;
  // Time elapsed since the start of the recording session when the action
  // occurred.
  base::TimeDelta timestamp;
  // Raw JPEG bytes of the viewport screenshot captured at the time of the
  // action, or empty if screenshot capture was unavailable or did not complete
  // before the session ended.
  std::string screenshot_jpeg_bytes;
};

// Standardized in-memory output of a skill recording session used by downstream
// consumers (e.g., WebUI display, MES skill generation).
struct RecordingArtifacts {
  // Maximum number of user actions retained in a single recording session.
  static constexpr size_t kMaxRecordedActions = 1000;

  RecordingArtifacts();
  RecordingArtifacts(const RecordingArtifacts&) = delete;
  RecordingArtifacts& operator=(const RecordingArtifacts&) = delete;
  RecordingArtifacts(RecordingArtifacts&&) noexcept;
  RecordingArtifacts& operator=(RecordingArtifacts&&) noexcept;
  ~RecordingArtifacts();

  // Recorded user actions in chronological order.
  std::vector<RecordedAction> actions;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_RECORDING_ARTIFACTS_H_
