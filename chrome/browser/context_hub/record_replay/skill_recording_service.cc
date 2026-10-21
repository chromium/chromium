// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/skill_recording_service.h"

#include <utility>

#include "base/time/time.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"

namespace context_hub {

SkillRecordingService::SkillRecordingService() = default;

SkillRecordingService::~SkillRecordingService() = default;

void SkillRecordingService::StartRecording() {
  if (is_recording()) {
    return;
  }
  actions_.clear();
  start_time_ = base::TimeTicks::Now();
}

browser::context_hub::mojom::SkillRecordingSessionDataPtr
SkillRecordingService::StopRecording() {
  if (!is_recording()) {
    return browser::context_hub::mojom::SkillRecordingSessionData::New();
  }
  start_time_.reset();

  auto session_data =
      browser::context_hub::mojom::SkillRecordingSessionData::New();
  session_data->actions = std::exchange(actions_, {});
  return session_data;
}

}  // namespace context_hub
