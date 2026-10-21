// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_SKILL_RECORDING_SERVICE_H_
#define CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_SKILL_RECORDING_SERVICE_H_

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-forward.h"
#include "components/keyed_service/core/keyed_service.h"

namespace context_hub {

// Per-profile service that manages in-memory recording sessions for capturing
// user actions to generate skills.
class SkillRecordingService : public KeyedService {
 public:
  SkillRecordingService();
  SkillRecordingService(const SkillRecordingService&) = delete;
  SkillRecordingService& operator=(const SkillRecordingService&) = delete;
  ~SkillRecordingService() override;

  // Starts a new recording session if one is not already active.
  void StartRecording();

  // Stops the current recording session and returns the captured session data.
  // Returns empty session data if no recording session is active.
  browser::context_hub::mojom::SkillRecordingSessionDataPtr StopRecording();

  bool is_recording() const { return start_time_.has_value(); }

  std::optional<base::TimeTicks> start_time() const { return start_time_; }

 private:
  std::optional<base::TimeTicks> start_time_;

  std::vector<browser::context_hub::mojom::SkillRecordingActionPtr> actions_;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_RECORD_REPLAY_SKILL_RECORDING_SERVICE_H_
