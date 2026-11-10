// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/context_hub/record_replay/recording_artifacts.h"

#include <string>
#include <utility>

#include "base/time/time.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-shared.h"

namespace context_hub {

RecordedAction::RecordedAction(
    browser::context_hub::mojom::SkillRecordingActionType type,
    std::string description,
    base::TimeDelta timestamp)
    : type(type), description(std::move(description)), timestamp(timestamp) {}

RecordedAction::RecordedAction(RecordedAction&&) noexcept = default;
RecordedAction& RecordedAction::operator=(RecordedAction&&) noexcept = default;
RecordedAction::~RecordedAction() = default;

RecordingArtifacts::RecordingArtifacts() = default;
RecordingArtifacts::RecordingArtifacts(RecordingArtifacts&&) noexcept = default;
RecordingArtifacts& RecordingArtifacts::operator=(
    RecordingArtifacts&&) noexcept = default;
RecordingArtifacts::~RecordingArtifacts() = default;

}  // namespace context_hub
