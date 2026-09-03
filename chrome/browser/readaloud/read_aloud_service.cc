// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_service.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/readaloud/audio_generation/speech_synthesis_broker.h"
#include "chrome/browser/readaloud/read_aloud_audio_broker.h"
#include "chrome/browser/readaloud/read_aloud_playback_session.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "components/dom_distiller/content/browser/distiller_page_web_contents.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/web_contents.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/message.h"

namespace readaloud {

ReadAloudService::ReadAloudService(
    Profile* profile,
    PlaybackControllerBinder controller_binder,
    ReadAloudAudioBroker::AudioStreamFactoryBinder factory_binder)
    : profile_(profile),
      controller_binder_(std::move(controller_binder)),
      audio_broker_(
          std::make_unique<ReadAloudAudioBroker>(std::move(factory_binder))),
      speech_synthesis_broker_(std::make_unique<SpeechSynthesisBroker>()) {}

ReadAloudService::~ReadAloudService() = default;

void ReadAloudService::SetDelegate(std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void ReadAloudService::Play(content::WebContents* new_web_contents) {
  if (!new_web_contents) {
    return;
  }

  if (new_web_contents != web_contents()) {
    Initialize(new_web_contents);
  }

  PlaybackState previous_state = GetCurrentPlaybackState();

  // Start or resume audio playback.
  CHECK(active_session_);
  active_session_->NotifyPlaybackStarted();

  if (utility_player_.is_bound()) {
    utility_player_->Play();
  }

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

  if (utility_player_.is_bound()) {
    utility_player_->Pause();
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
  current_title_.clear();
  current_publisher_.clear();
  current_duration_ = base::Seconds(0);

  // Stopping the Utility Process and any of their connections.
  ResetUtilityConnection();

  // Notify the UI/client delegate if the playback state transitioned.
  PlaybackState current_state = GetCurrentPlaybackState();
  if (current_state != previous_state && delegate_) {
    delegate_->OnPlaybackStateChanged(current_state);
  }
}

void ReadAloudService::SeekToWordIndex(int word_index) {}
void ReadAloudService::Seek(base::TimeDelta absolute_time) {}
void ReadAloudService::SeekRelative(base::TimeDelta offset) {}
void ReadAloudService::SetPlaybackRate(float rate) {
  if (utility_player_.is_bound()) {
    utility_player_->SetPlaybackRate(rate);
  }
}
void ReadAloudService::SetVoice(std::string_view voice_id) {
  if (speech_synthesis_broker_) {
    speech_synthesis_broker_->SetVoice(voice_id);
  }
}
void ReadAloudService::SetLanguageCode(std::string_view language_code) {
  if (speech_synthesis_broker_) {
    speech_synthesis_broker_->SetLanguageCode(language_code);
  }
}
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
void ReadAloudService::SetPlaybackMode(PlaybackMode mode) {
  playback_mode_ = mode;
}
void ReadAloudService::SetHighlightingEnabled(bool enabled) {}
void ReadAloudService::SendFeedback(FeedbackType feedback_type) {}
void ReadAloudService::CheckReadability(const GURL& url) {
  if (delegate_) {
    bool is_readable = dom_distiller::url_utils::IsUrlDistillable(url);
    delegate_->OnReadabilityResult(url, is_readable);
  }
}

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
  bool distillation_succeeded =
      article_proto && !article_proto->pages().empty();
  if (!distillation_start_time_.is_null()) {
    base::UmaHistogramTimes("ReadAloud.Distillation.Duration",
                            base::TimeTicks::Now() - distillation_start_time_);
    base::UmaHistogramBoolean("ReadAloud.Distillation.Success",
                              distillation_succeeded);
    distillation_start_time_ = base::TimeTicks();
  }
  viewer_handle_.reset();

  if (!distillation_succeeded) {
    Stop();
    if (delegate_) {
      delegate_->OnPlaybackError("Distillation failed");
    }
    return;
  }

  // Refine article title in UI using distilled article headline if available.
  // Processed before checking utility_player_.is_bound() so service state
  // retains the distilled title independently of utility transport binding.
  if (article_proto && !article_proto->title().empty()) {
    current_title_ = article_proto->title();
    if (delegate_) {
      delegate_->OnMetadataAvailable(current_title_, current_publisher_);
    }
  }

  if (!utility_player_.is_bound()) {
    return;
  }

  if (playback_mode_ == PlaybackMode::kOverview) {
    // TODO(b/548552257): Connect to the Page Summary API to summarize the
    // distilled article before sending to utility_player_.
  }

  // Rule of Two Enforcement: Distilled webpage text originates from untrusted
  // renderer content. To adhere to Chromium security guidelines, ReadAloudService
  // (privileged Browser process) must not parse, sanitize, or tokenize the raw text.
  // We package the raw page strings directly into Mojo TextSegment structs and
  // forward them to the sandboxed Utility process for chunking and synthesis.
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  segments.reserve(article_proto->pages_size());
  for (int i = 0; i < article_proto->pages_size(); ++i) {
    auto segment = read_aloud::mojom::TextSegment::New();
    segment->segment_index = static_cast<uint32_t>(i);
    segment->text = base::UTF8ToUTF16(article_proto->pages(i).html());
    segments.push_back(std::move(segment));
  }
  utility_player_->SetTextContent(std::move(segments));
}

void ReadAloudService::OnArticleUpdated(
    dom_distiller::ArticleDistillationUpdate article_update) {}

void ReadAloudService::Initialize(content::WebContents* new_web_contents) {
  if (!new_web_contents) {
    return;
  }
  Stop();
  Observe(new_web_contents);
  active_session_ =
      std::make_unique<ReadAloudPlaybackSession>(new_web_contents, this);

  current_duration_ = base::Seconds(0);

  ProvideInitialMetadata();
  if (delegate_) {
    delegate_->OnPlaybackProgressUpdated(base::Seconds(0), current_duration_);
  }

  DistillPage(new_web_contents);
  EnsurePlaybackControllerConnected();
  InitializeAudioStream();
}

void ReadAloudService::ProvideInitialMetadata() {
  if (!web_contents()) {
    return;
  }
  current_title_ = base::UTF16ToUTF8(web_contents()->GetTitle());
  current_publisher_ = base::UTF16ToUTF8(
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          web_contents()->GetLastCommittedURL()));

  if (delegate_) {
    delegate_->OnMetadataAvailable(current_title_, current_publisher_);
  }
}

// Ensures the sandboxed ReadAloudPlaybackController utility process is running and
// bound. Uses `controller_binder_` if provided (e.g. in unit tests); otherwise
// launches `ReadAloudPlaybackControllerFactory` via ServiceProcessHost and requests
// a new controller instance with our client observer remote.
void ReadAloudService::EnsurePlaybackControllerConnected() {
  if (utility_player_.is_bound()) {
    return;
  }

  if (controller_binder_) {
    utility_player_.reset();
    utility_observer_receiver_.reset();
    controller_binder_.Run(
        utility_player_.BindNewPipeAndPassReceiver(),
        utility_observer_receiver_.BindNewPipeAndPassRemote());
    utility_player_.set_disconnect_handler(base::BindOnce(
        &ReadAloudService::OnUtilityDisconnect, weak_factory_.GetWeakPtr()));
    return;
  }

  player_factory_.reset();
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

void ReadAloudService::InitializeAudioStream() {
  if (!utility_player_.is_bound() || !audio_broker_) {
    return;
  }

  // TODO(b/547989045): Query audio parameters from the OS rather than using
  // hardcoded constants.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::ChannelLayoutConfig::Mono(),
                                readaloud::kAudioSampleRate,
                                readaloud::kAudioFramesPerBuffer);

  // Associate the audio output stream with the hosting WebContents' audio group
  // ID. This ensures the Audio Service correctly links this stream to the tab
  // for tab-strip muting, media indicator tracking, and tab audio
  // capture/mirroring.
  if (!web_contents()) {
    return;
  }
  base::UnguessableToken group_id = web_contents()->GetAudioGroupId();

  audio_broker_->CreateOutputStream(
      group_id, params,
      base::BindOnce(&ReadAloudService::OnAudioStreamCreated,
                     weak_factory_.GetWeakPtr(), params));
}

void ReadAloudService::OnAudioStreamCreated(
    const media::AudioParameters& params,
    mojo::PendingRemote<media::mojom::AudioOutputStream> stream_remote,
    media::mojom::ReadWriteAudioDataPipePtr data_pipe) {
  if (!utility_player_.is_bound()) {
    return;
  }
  if (!stream_remote.is_valid() || !data_pipe) {
    Stop();
    if (delegate_) {
      delegate_->OnPlaybackError("Failed to initialize audio output stream");
    }
    return;
  }

  utility_player_->InitializeAudio(std::move(stream_remote),
                                   std::move(data_pipe), params);
}

void ReadAloudService::OnUtilityDisconnect() {
  Stop();
  if (delegate_) {
    delegate_->OnPlaybackError("Utility process disconnected");
  }
}

void ReadAloudService::ResetUtilityConnection() {
  utility_observer_receiver_.reset();
  utility_player_.reset();
  player_factory_.reset();
  if (audio_broker_) {
    audio_broker_->Reset();
  }
}

void ReadAloudService::OnDistillationFailed(
    dom_distiller::DistillationParseResult reason) {
  base::UmaHistogramEnumeration("ReadAloud.Distillation.FailureReason", reason);
}

void ReadAloudService::OnPlaybackStateChanged(
    read_aloud::mojom::PlaybackState state) {}

void ReadAloudService::OnPlaybackDurationChanged(base::TimeDelta duration) {
  current_duration_ = std::max(base::Seconds(0), duration);
}

void ReadAloudService::OnWordBoundaryReached(uint32_t segment_index,
                                             uint32_t character_offset,
                                             base::TimeDelta audio_timestamp) {
  if (!current_duration_.is_positive() || !delegate_) {
    return;
  }
  base::TimeDelta clamped_elapsed =
      std::clamp(audio_timestamp, base::Seconds(0), current_duration_);
  delegate_->OnPlaybackProgressUpdated(clamped_elapsed, current_duration_);
}

void ReadAloudService::OnTextChunked(
    const std::vector<std::u16string>& chunks) {
  if (chunks.size() > readaloud::kMaxTextChunks) {
    mojo::ReportBadMessage("Received invalid chunk payload");
    return;
  }

  if (delegate_) {
    delegate_->OnTextChunked(chunks);
  }
}

void ReadAloudService::RequestSpeechSynthesis(
    const std::u16string& text_chunk,
    uint64_t sequence_id,
    read_aloud::mojom::ReadAloudPlaybackControllerClient::
        RequestSpeechSynthesisCallback callback) {
  if (!speech_synthesis_broker_) {
    std::move(callback).Run(mojo_base::BigBuffer(), /*success=*/false);
    return;
  }

  OptimizationGuideKeyedService* opt_guide_service = nullptr;
  if (profile_) {
    opt_guide_service =
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  }

  speech_synthesis_broker_->SynthesizeSpeech(opt_guide_service, text_chunk,
                                             std::move(callback));
}

}  // namespace readaloud
