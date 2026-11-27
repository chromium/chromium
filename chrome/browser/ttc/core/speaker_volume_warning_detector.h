// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_SPEAKER_VOLUME_WARNING_DETECTOR_H_
#define CHROME_BROWSER_TTC_CORE_SPEAKER_VOLUME_WARNING_DETECTOR_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ttc/core/output_volume_monitor.h"
#include "chrome/browser/ttc/core/session_controller.h"

namespace ttc {

class SessionJournal;

// Tracks the system's default audio output while a TTC session is live and
// invokes `warning_callback` when the user might not hear model replies:
// - `kMuted`: emitted immediately if already muted when created, and on each
//   subsequent transition to muted.
class SpeakerVolumeWarningDetector {
 public:
  using WarningCallback = base::RepeatingCallback<void(SpeakerVolumeWarning)>;

  // Returns null if the platform cannot read the system output volume.
  static std::unique_ptr<SpeakerVolumeWarningDetector> MaybeCreate(
      SessionJournal& journal,
      WarningCallback warning_callback);

  SpeakerVolumeWarningDetector(const SpeakerVolumeWarningDetector&) = delete;
  SpeakerVolumeWarningDetector& operator=(const SpeakerVolumeWarningDetector&) =
      delete;
  ~SpeakerVolumeWarningDetector();

 private:
  SpeakerVolumeWarningDetector(SessionJournal& journal,
                               WarningCallback warning_callback);

  bool Start();
  void OnOutputVolumeChanged(const OutputVolumeState& state);

  // Owned by `SessionControllerImpl`, which destroys this detector before
  // tearing down its journal.
  const raw_ref<SessionJournal> journal_;
  WarningCallback warning_callback_;

  // Last known OS output volume from `OutputVolumeMonitor`, or nullopt while
  // no active output device is reported. Detects false->true mute edges so
  // muted slider moves do not emit duplicate kMuted warnings.
  std::optional<OutputVolumeState> output_volume_;

  std::unique_ptr<OutputVolumeMonitor> monitor_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_SPEAKER_VOLUME_WARNING_DETECTOR_H_
