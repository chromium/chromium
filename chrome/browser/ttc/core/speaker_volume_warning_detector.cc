// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/speaker_volume_warning_detector.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/ttc/app/public/session_journal.h"

namespace ttc {

// static
std::unique_ptr<SpeakerVolumeWarningDetector>
SpeakerVolumeWarningDetector::MaybeCreate(SessionJournal& journal) {
  auto detector = base::WrapUnique(new SpeakerVolumeWarningDetector(journal));
  if (!detector->Start()) {
    return nullptr;
  }
  return detector;
}

SpeakerVolumeWarningDetector::SpeakerVolumeWarningDetector(
    SessionJournal& journal)
    : journal_(journal) {}

SpeakerVolumeWarningDetector::~SpeakerVolumeWarningDetector() = default;

bool SpeakerVolumeWarningDetector::Start() {
  // Unretained is safe: `this` owns `monitor_`, which only runs the callback
  // while alive.
  monitor_ = OutputVolumeMonitor::Create(
      base::BindRepeating(&SpeakerVolumeWarningDetector::OnOutputVolumeChanged,
                          base::Unretained(this)));
  if (!monitor_) {
    return false;
  }
  if (std::optional<OutputVolumeState> state = monitor_->GetState()) {
    OnOutputVolumeChanged(*state);
  }
  return true;
}

void SpeakerVolumeWarningDetector::OnOutputVolumeChanged(
    const OutputVolumeState& state) {
  actor::JournalDetailsBuilder details;
  details.Add("muted", state.muted);
  if (state.level) {
    details.Add("level", *state.level);
  }
  journal_->Log("TtcSystemSpeakerVolume", std::move(details).Build());

  output_volume_ = state;
}

}  // namespace ttc
