// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/output_volume_monitor.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/check_is_test.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"

namespace ttc {

namespace {

// Follows the system output mute state and volume through CRAS, which already
// runs in the browser process. CrasAudioHandler doesn't notify observers when
// policy or the security curtain forces mute, so GetState() can change without
// a callback then. The callback also doesn't run when the state becomes
// unknown because no output node is active.
class OutputVolumeMonitorChromeOS
    : public OutputVolumeMonitor,
      public ash::CrasAudioHandler::AudioObserver {
 public:
  OutputVolumeMonitorChromeOS(ash::CrasAudioHandler& handler,
                              StateChangedCallback callback)
      : handler_(handler),
        callback_(std::move(callback)),
        last_state_(GetState()) {
    observation_.Observe(&handler);
  }
  OutputVolumeMonitorChromeOS(const OutputVolumeMonitorChromeOS&) = delete;
  OutputVolumeMonitorChromeOS& operator=(const OutputVolumeMonitorChromeOS&) =
      delete;
  ~OutputVolumeMonitorChromeOS() override = default;

  // OutputVolumeMonitor:
  std::optional<OutputVolumeState> GetState() const override {
    // Without an active output, the volume CRAS reports is stale.
    if (!handler_->GetPrimaryActiveOutputNode()) {
      return std::nullopt;
    }
    return OutputVolumeState{
        .muted = handler_->IsOutputMuted(),
        .level = handler_->GetOutputVolumePercent() / 100.0f};
  }

  // ash::CrasAudioHandler::AudioObserver:
  // CRAS applies one volume to all active output nodes. Events on inactive
  // nodes don't change it, and NotifyIfChanged() drops them.
  void OnOutputNodeVolumeChanged(uint64_t node_id, int volume) override {
    NotifyIfChanged();
  }
  void OnOutputMuteChanged(bool mute_on) override { NotifyIfChanged(); }
  void OnActiveOutputNodeChanged() override { NotifyIfChanged(); }

 private:
  void NotifyIfChanged() {
    std::optional<OutputVolumeState> state = GetState();
    if (state == last_state_) {
      return;
    }
    last_state_ = state;
    if (state) {
      callback_.Run(*state);
    }
  }

  // `CrasAudioHandler` outlives the browser's profiles, hence TTC sessions.
  const raw_ref<ash::CrasAudioHandler> handler_;
  const StateChangedCallback callback_;
  std::optional<OutputVolumeState> last_state_;
  base::ScopedObservation<ash::CrasAudioHandler,
                          ash::CrasAudioHandler::AudioObserver>
      observation_{this};
};

}  // namespace

// static
std::unique_ptr<OutputVolumeMonitor> OutputVolumeMonitor::CreateForPlatform(
    StateChangedCallback callback) {
  ash::CrasAudioHandler* handler = ash::CrasAudioHandler::Get();
  // Null only in unit tests that don't set up CRAS.
  if (!handler) {
    CHECK_IS_TEST();
    return nullptr;
  }
  return std::make_unique<OutputVolumeMonitorChromeOS>(*handler,
                                                       std::move(callback));
}

}  // namespace ttc
