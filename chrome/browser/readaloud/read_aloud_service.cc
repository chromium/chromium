// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/readaloud/read_aloud_playback_session.h"
#include "components/dom_distiller/content/browser/distiller_page_web_contents.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/base/big_buffer.h"

namespace readaloud {

ReadAloudService::ReadAloudService(Profile* profile) : profile_(profile) {}

ReadAloudService::~ReadAloudService() = default;

void ReadAloudService::SetDelegate(std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void ReadAloudService::Play(content::WebContents* new_web_contents) {
  if (!new_web_contents) {
    return;
  }

  // If a new WebContents is available, stop active playback, start
  // observing the new WebContents, and create a new playback session.
  if (new_web_contents != web_contents()) {
    Stop();
    Observe(new_web_contents);
    active_session_ =
        std::make_unique<ReadAloudPlaybackSession>(new_web_contents, this);
  }

  PlaybackState previous_state = GetCurrentPlaybackState();

  // Start or resume audio playback.
  CHECK(active_session_);
  active_session_->NotifyPlaybackStarted();

  // Notify the UI/client delegate if the playback state transitioned.
  PlaybackState current_state = GetCurrentPlaybackState();
  if (current_state != previous_state && delegate_) {
    delegate_->OnPlaybackStateChanged(current_state);
  }
}

void ReadAloudService::Pause() {
  PlaybackState previous_state = GetCurrentPlaybackState();
  if (active_session_) {
    active_session_->NotifyPlaybackPaused();
  }

  // Notify the UI/client delegate if the playback state transitioned.
  PlaybackState current_state = GetCurrentPlaybackState();
  if (current_state != previous_state && delegate_) {
    delegate_->OnPlaybackStateChanged(current_state);
  }
}

void ReadAloudService::Stop() {
  // Detach observer since tracking is only needed during active playback.
  Observe(nullptr);

  // Stop active audio playback and release media session resources.
  PlaybackState previous_state = GetCurrentPlaybackState();
  if (active_session_) {
    active_session_->NotifyPlaybackStopped();
    active_session_.reset();
  }

  // Cancel any ongoing page distillation request and reset timing metrics.
  viewer_handle_.reset();
  distillation_start_time_ = base::TimeTicks();

  // Notify the UI/client delegate if the playback state transitioned.
  PlaybackState current_state = GetCurrentPlaybackState();
  if (current_state != previous_state && delegate_) {
    delegate_->OnPlaybackStateChanged(current_state);
  }
}
void ReadAloudService::SeekToWordIndex(int word_index) {}
void ReadAloudService::Seek(base::TimeDelta absolute_time) {}
void ReadAloudService::SeekRelative(base::TimeDelta offset) {}
void ReadAloudService::SetPlaybackRate(float rate) {}
void ReadAloudService::SetVoice(std::string_view voice_id) {}
void ReadAloudService::PreviewVoice(std::string_view voice_id) {
  // Pause active article playback while previewing a voice.
  Pause();

  // TODO(b/522835686): Implement actual voice preview audio synthesis via the
  // utility process player.

  // Notify the UI/client delegate that voice preview playback has started.
  if (delegate_) {
    delegate_->OnVoicePreviewPlaybackStateChanged(voice_id,
                                                  PlaybackState::kPlaying);
  }
}

void ReadAloudService::StopVoicePreview() {
  // TODO(b/522835686): Stop actual voice preview audio playback in the utility
  // process player.

  // Notify the UI/client delegate that voice preview playback has stopped.
  // Passing an empty string indicates that any active voice preview is stopped.
  if (delegate_) {
    delegate_->OnVoicePreviewPlaybackStateChanged(/*voice_id=*/"",
                                                  PlaybackState::kStopped);
  }
}
void ReadAloudService::SetPlaybackMode(PlaybackMode mode) {}
void ReadAloudService::SetHighlightingEnabled(bool enabled) {}
void ReadAloudService::SendFeedback(FeedbackType feedback_type) {}
void ReadAloudService::CheckReadability(const GURL& url) {}

void ReadAloudService::WebContentsDestroyed() {
  // Stop active playback and detach observer when the tab is destroyed.
  Stop();
}

void ReadAloudService::PrimaryPageChanged(content::Page& page) {
  // Stop playback, reset distillation, and detach observer when the primary
  // page navigates to a new URL.
  Stop();
}

void ReadAloudService::OnSessionSuspended() {
  Pause();
}

void ReadAloudService::OnSessionResumed() {
  Play(web_contents());
}

bool ReadAloudService::IsPlaybackPaused() const {
  return !active_session_ || active_session_->is_paused();
}

ReadAloudService::PlaybackState ReadAloudService::GetCurrentPlaybackState()
    const {
  if (!active_session_) {
    return PlaybackState::kStopped;
  }
  if (!active_session_->is_playback_in_progress()) {
    return PlaybackState::kStopped;
  }
  return active_session_->is_paused() ? PlaybackState::kPaused
                                      : PlaybackState::kPlaying;
}

void ReadAloudService::Shutdown() {
  Stop();
  weak_factory_.InvalidateWeakPtrs();
  if (delegate_) {
    delegate_->OnNativeDestroyed();
    delegate_.reset();
  }
  utility_observer_receiver_.reset();
  utility_player_.reset();
  player_factory_.reset();
}

void ReadAloudService::DistillPage(content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }

  dom_distiller::DomDistillerService* service =
      dom_distiller::DomDistillerServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (!service) {
    return;
  }

  distillation_start_time_ = base::TimeTicks::Now();

  viewer_handle_ = service->ViewUrlIgnoreCache(
      this,
      service->CreateDefaultDistillerPageWithHandle(
          std::make_unique<dom_distiller::SourcePageHandleWebContents>(
              web_contents, /*owned=*/false)),
      web_contents->GetLastCommittedURL());
}

void ReadAloudService::OnArticleReady(
    const dom_distiller::DistilledArticleProto* article_proto) {
  if (!distillation_start_time_.is_null()) {
    bool success = article_proto && !article_proto->pages().empty();
    base::UmaHistogramTimes("ReadAloud.Distillation.Duration",
                            base::TimeTicks::Now() - distillation_start_time_);
    base::UmaHistogramBoolean("ReadAloud.Distillation.Success", success);
    distillation_start_time_ = base::TimeTicks();
  }
  viewer_handle_.reset();
}

void ReadAloudService::OnArticleUpdated(
    dom_distiller::ArticleDistillationUpdate article_update) {}

void ReadAloudService::Initialize() {
  EnsureServiceConnected();
}

void ReadAloudService::EnsureServiceConnected() {
  if (player_factory_.is_bound()) {
    return;
  }
  content::ServiceProcessHost::Launch<
      read_aloud::mojom::ReadAloudPlaybackControllerFactory>(
      player_factory_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          // TODO(b/525116429): Use localized string resource.
          .WithDisplayName("ReadAloud Playback Service")
          .Pass());
  player_factory_.reset_on_disconnect();

  // Create controller in utility process and bind our utility endpoints.
  utility_player_.reset();
  utility_observer_receiver_.reset();

  player_factory_->CreateController(
      utility_player_.BindNewPipeAndPassReceiver(),
      utility_observer_receiver_.BindNewPipeAndPassRemote());

  utility_player_.set_disconnect_handler(base::BindOnce(
      &ReadAloudService::OnUtilityDisconnect, weak_factory_.GetWeakPtr()));
}

void ReadAloudService::OnUtilityDisconnect() {
  utility_observer_receiver_.reset();
  utility_player_.reset();
  player_factory_.reset();
}

void ReadAloudService::OnDistillationFailed(
    dom_distiller::DistillationParseResult reason) {
  base::UmaHistogramEnumeration("ReadAloud.Distillation.FailureReason", reason);
}

void ReadAloudService::OnPlaybackStateChanged(
    read_aloud::mojom::PlaybackState state) {}

void ReadAloudService::OnPlaybackDurationChanged(base::TimeDelta duration) {}

void ReadAloudService::OnWordBoundaryReached(uint32_t segment_index,
                                             uint32_t character_offset,
                                             base::TimeDelta audio_timestamp) {}

void ReadAloudService::RequestSpeechSynthesis(
    const std::u16string& text_chunk,
    uint64_t sequence_id,
    read_aloud::mojom::ReadAloudPlaybackControllerClient::
        RequestSpeechSynthesisCallback callback) {
  std::move(callback).Run(mojo_base::BigBuffer(), false);
}

}  // namespace readaloud
