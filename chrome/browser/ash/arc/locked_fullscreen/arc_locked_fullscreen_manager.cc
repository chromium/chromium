// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/locked_fullscreen/arc_locked_fullscreen_manager.h"

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/ash/experiences/arc/arc_util.h"

namespace arc {
namespace {
constexpr char kMuteAudioWithSuccessHistogram[] = "Arc.MuteAudioSuccess";
constexpr char kUnmuteAudioWithSuccessHistogram[] = "Arc.UnmuteAudioSuccess";
}  // namespace

ArcLockedFullscreenManager::ArcLockedFullscreenManager(Profile* profile)
    : profile_(profile->GetWeakPtr()) {}

ArcLockedFullscreenManager::~ArcLockedFullscreenManager() = default;

void ArcLockedFullscreenManager::UpdateForLockedFullscreenMode(bool locked) {
  if (!profile_) {
    return;
  }

  arc::ArcSessionManager* const arc_session_manager =
      arc::ArcSessionManager::Get();
  if (!arc_session_manager) {
    return;
  }

  // Disable ARC audio when in locked fullscreen mode (or enable them if
  // unlocked) as long as the `BocaOnTaskMuteArcAudio` feature flag is enabled.
  // This eliminates the need for disabling critical ARC apps.
  if (ash::features::IsBocaOnTaskMuteArcAudioEnabled()) {
    mute_audio_requested_ = locked;
    if (arc_session_manager->state() != ArcSessionManager::State::ACTIVE) {
      // ARC is not up, so delay the mute/unmute operation.
      if (!arc_session_manager_observation_.IsObserving()) {
        arc_session_manager_observation_.Observe(arc_session_manager);
      }
      return;
    }
    MuteOrUnmuteArcAudio();
    return;
  }

  // Fall back to the deprecated flow that disables ARC to prepare for locked
  // fullscreen mode otherwise.
  // TODO - crbug.com/400483081: Remove deprecated flow.
  if (locked) {
    // Disable ARC, preserve data.
    arc_session_manager->RequestDisable();
  } else {
    // Re-enable ARC if needed.
    if (arc::IsArcPlayStoreEnabledForProfile(profile_.get())) {
      arc_session_manager->RequestEnable();
    }
  }
}

void ArcLockedFullscreenManager::OnArcStarted() {
  MuteOrUnmuteArcAudio();
}

void ArcLockedFullscreenManager::OnShutdown() {
  arc_session_manager_observation_.Reset();
}

void ArcLockedFullscreenManager::MuteOrUnmuteArcAudio() {
  if (!profile_ || !mute_audio_requested_.has_value()) {
    return;
  }

  vm_tools::concierge::MuteVmAudioRequest request;
  request.set_owner_id(
      ash::BrowserContextHelper::GetUserIdHashFromBrowserContext(
          profile_.get()));
  request.set_name(kArcVmName);
  request.set_muted(mute_audio_requested_.value());
  ash::ConciergeClient::Get()->MuteVmAudio(
      request, base::BindOnce(
                   &ArcLockedFullscreenManager::OnMuteOrUnmuteArcAudioProcessed,
                   weak_ptr_factory_.GetWeakPtr(), request.muted()));
}

void ArcLockedFullscreenManager::OnMuteOrUnmuteArcAudioProcessed(
    bool mute,
    std::optional<vm_tools::concierge::SuccessFailureResponse> response) {
  const char* const histogram_name =
      mute ? kMuteAudioWithSuccessHistogram : kUnmuteAudioWithSuccessHistogram;
  if (!response || !response.has_value() || !response->success()) {
    LOG(ERROR) << "Failed to process ARC audio request with mute value: "
               << mute;
    base::UmaHistogramBoolean(histogram_name, false);
    return;
  }
  base::UmaHistogramBoolean(histogram_name, true);

  // Clear `mute_audio_requested_` only if ARC audio was successfully unmuted
  // and there are no more pending mute audio requests. This ensures that we do
  // not submit ARC unmute audio requests unnecessarily on restart. Mute audio
  // requests are meant to persist across ARC restarts when in locked fullscreen
  // mode.
  if (!mute && mute_audio_requested_.has_value() &&
      !mute_audio_requested_.value()) {
    mute_audio_requested_ = std::nullopt;
    arc_session_manager_observation_.Reset();
  }
}

}  // namespace arc
