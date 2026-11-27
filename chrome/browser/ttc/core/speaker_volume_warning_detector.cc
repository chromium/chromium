// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/speaker_volume_warning_detector.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ttc/core/features.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/ttc/app/public/session_journal.h"

namespace ttc {

// static
std::unique_ptr<SpeakerVolumeWarningDetector>
SpeakerVolumeWarningDetector::MaybeCreate(SessionJournal& journal,
                                          WarningCallback warning_callback) {
  auto detector = base::WrapUnique(
      new SpeakerVolumeWarningDetector(journal, std::move(warning_callback)));
  if (!detector->Start()) {
    return nullptr;
  }
  return detector;
}

SpeakerVolumeWarningDetector::SpeakerVolumeWarningDetector(
    SessionJournal& journal,
    WarningCallback warning_callback)
    : journal_(journal), warning_callback_(std::move(warning_callback)) {}

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

void SpeakerVolumeWarningDetector::OnModelAudioEnvelopeReady() {
  if (low_volume_warned_ || !IsOutputVolumeLow()) {
    return;
  }
  // Users may keep the volume low on purpose, so warn only once.
  low_volume_warned_ = true;
  warning_callback_.Run(SpeakerVolumeWarning::kLow);
}

void SpeakerVolumeWarningDetector::OnOutputVolumeChanged(
    const OutputVolumeState& state) {
  actor::JournalDetailsBuilder details;
  details.Add("muted", state.muted);
  if (state.level) {
    details.Add("level", *state.level);
  }
  journal_->Log("TtcSystemSpeakerVolume", std::move(details).Build());

  const bool was_muted = output_volume_ && output_volume_->muted;
  output_volume_ = state;
  if (state.muted && !was_muted) {
    warning_callback_.Run(SpeakerVolumeWarning::kMuted);
  }
}

bool SpeakerVolumeWarningDetector::IsOutputVolumeLow() const {
  // Muting has its own warning. A device without volume control is never low.
  return output_volume_ && !output_volume_->muted && output_volume_->level &&
         *output_volume_->level < kTtcSpeakerVolumeMinPercent.Get() / 100.0f;
}

}  // namespace ttc
