// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_service.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/readaloud/audio_generation/overview_generation_broker.h"
#include "chrome/browser/readaloud/audio_generation/speech_synthesis_broker.h"
#include "chrome/browser/readaloud/read_aloud_audio_broker.h"
#include "chrome/browser/readaloud/read_aloud_playback_session.h"
#include "chrome/common/readaloud/read_aloud_constants.h"
#include "components/dom_distiller/content/browser/distiller_page_web_contents.h"
#include "components/dom_distiller/core/distiller_page.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/web_contents.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/message.h"

namespace readaloud {
namespace {

std::vector<std::string_view> ExtractDistilledPageTexts(
    const dom_distiller::DistilledArticleProto* article_proto) {
  std::vector<std::string_view> page_texts;
  if (!article_proto) {
    return page_texts;
  }
  page_texts.reserve(article_proto->pages_size());
  for (const auto& page : article_proto->pages()) {
    page_texts.push_back(page.text_content());
  }
  return page_texts;
}

// Returns sanitized `value` suitable for UI display. Neutralizes BIDI control
// characters via SanitizeUserSuppliedString and truncates at UTF-8 codepoint
// boundaries to kMaxOverviewMetadataLength.
std::string SanitizeMetadata(std::string_view value) {
  if (value.empty()) {
    return std::string();
  }
  std::u16string u16_value = base::UTF8ToUTF16(value);
  base::i18n::SanitizeUserSuppliedString(&u16_value);
  std::string sanitized = base::UTF16ToUTF8(u16_value);
  if (sanitized.size() > kMaxOverviewMetadataLength) {
    sanitized =
        base::TruncateUTF8ToByteSize(sanitized, kMaxOverviewMetadataLength);
  }
  return sanitized;
}

}  // namespace

ReadAloudService::ReadAloudService(
    Profile* profile,
    PlaybackControllerBinder controller_binder,
    ReadAloudAudioBroker::AudioStreamFactoryBinder factory_binder)
    : profile_(profile),
      controller_binder_(std::move(controller_binder)),
      audio_broker_(
          std::make_unique<ReadAloudAudioBroker>(std::move(factory_binder))),
      speech_synthesis_broker_(std::make_unique<SpeechSynthesisBroker>()),
      overview_generation_broker_(
          std::make_unique<OverviewGenerationBroker>()) {}

ReadAloudService::~ReadAloudService() = default;

void ReadAloudService::SetDelegate(std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void ReadAloudService::NotifyPlaybackStateChanged(
    read_aloud::mojom::PlaybackState state) {
  if (delegate_) {
    delegate_->OnPlaybackStateChanged(state);
  }
}

void ReadAloudService::Play(content::WebContents* new_web_contents) {
  if (!new_web_contents) {
    return;
  }

  if (new_web_contents != web_contents()) {
    Initialize(new_web_contents);
  }

  // Start or resume audio playback.
  CHECK(media_session_);
  media_session_->NotifyPlaybackStarted();

  // In overview mode, the utility Play() is deferred until
  // OnOverviewGenerated() has loaded the script, so the utility play-on-ready
  // watchdog does not race the browser-owned overview generation wait.
  if (utility_player_.is_bound() && !IsWaitingForOverviewContent()) {
    utility_player_->Play();
  }
}

void ReadAloudService::Pause() {
  if (media_session_) {
    media_session_->NotifyPlaybackPaused();
  }

  if (utility_player_.is_bound()) {
    utility_player_->Pause();
  }

  // TODO(b/562011435): Handle late kPlaying updates arriving from utility
  // after Pause() is called.
  if (media_session_ && media_session_->is_playback_in_progress()) {
    NotifyPlaybackStateChanged(read_aloud::mojom::PlaybackState::kPaused);
  }
}

void ReadAloudService::Stop() {
  ResetPlayback();
  NotifyPlaybackStateChanged(read_aloud::mojom::PlaybackState::kStopped);
}

void ReadAloudService::ResetPlayback() {
  // Detach observer since tracking is only needed during active playback.
  Observe(nullptr);

  // Stop active audio playback and release media session resources.
  if (media_session_) {
    media_session_->NotifyPlaybackStopped();
    media_session_.reset();
  }

  // Cancel any ongoing page distillation request and reset timing metrics.
  viewer_handle_.reset();
  if (overview_generation_broker_) {
    overview_generation_broker_->InvalidatePendingRequests();
  }
  overview_script_loaded_ = false;
  distillation_start_time_ = base::TimeTicks();
  article_title_.clear();
  current_publisher_.clear();
  current_duration_ = base::Seconds(0);

  // Stopping the Utility Process and any of their connections.
  ResetUtilityConnection();
}

void ReadAloudService::SeekToWord(int segment_index, int character_offset) {
  if (segment_index < 0 || character_offset < 0) {
    return;
  }
  if (utility_player_.is_bound()) {
    utility_player_->SeekToWord(static_cast<uint32_t>(segment_index),
                                static_cast<uint32_t>(character_offset));
  }
}

void ReadAloudService::Seek(base::TimeDelta absolute_time) {
  if (absolute_time.is_negative() || absolute_time.is_max()) {
    return;
  }
  if (utility_player_.is_bound()) {
    utility_player_->SeekToTime(absolute_time);
  }
}
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

  // Notify the UI/client delegate that voice preview playback is buffering.
  if (delegate_) {
    delegate_->OnVoicePreviewPlaybackStateChanged(
        voice_id, read_aloud::mojom::PlaybackState::kBuffering);
  }

  // TODO(b/522835686): Implement actual voice preview audio synthesis via the
  // utility process player and notify the delegate when playback transitions to
  // read_aloud::mojom::PlaybackState::kPlaying.
}

void ReadAloudService::StopVoicePreview() {
  // TODO(b/522835686): Stop actual voice preview audio playback in the utility
  // process player.

  // Notify the UI/client delegate that voice preview playback has stopped.
  // Passing an empty string indicates that any active voice preview is stopped.
  if (delegate_) {
    delegate_->OnVoicePreviewPlaybackStateChanged(
        /*voice_id=*/"", read_aloud::mojom::PlaybackState::kStopped);
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
  return !media_session_ || media_session_->is_paused();
}

void ReadAloudService::Shutdown() {
  ResetPlayback();
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

  std::unique_ptr<dom_distiller::DistillerPage> distiller_page =
      service->CreateDefaultDistillerPageWithHandle(
          std::make_unique<dom_distiller::SourcePageHandleWebContents>(
              web_contents, /*owned=*/false));
  if (distiller_page) {
    distiller_page->SetMinimumAllowableDistilledContentLength(0);
  }

  viewer_handle_ = service->ViewUrlIgnoreCache(
      this, std::move(distiller_page), web_contents->GetLastCommittedURL());
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
    HandlePlaybackError("Distillation failed");
    return;
  }

  // Refine article title in UI using distilled article headline if available.
  // Processed before checking utility_player_.is_bound() so service state
  // retains the distilled title independently of utility transport binding.
  if (article_proto && !article_proto->title().empty()) {
    article_title_ = article_proto->title();
    if (delegate_) {
      delegate_->OnMetadataAvailable(article_title_, current_publisher_);
    }
  }

  if (!utility_player_.is_bound()) {
    return;
  }

  std::vector<std::string_view> page_texts =
      ExtractDistilledPageTexts(article_proto);

  if (playback_mode_ == PlaybackMode::kOverview) {
    RequestOverviewGeneration(page_texts);
    return;
  }

  // Rule of Two Enforcement: Distilled webpage text originates from untrusted
  // renderer content. To adhere to Chromium security guidelines,
  // ReadAloudService (privileged Browser process) must not parse, sanitize, or
  // tokenize the raw text. We package the raw page strings directly into Mojo
  // TextSegment structs and forward them to the sandboxed Utility process for
  // chunking and synthesis.
  std::vector<read_aloud::mojom::TextSegmentPtr> segments;
  segments.reserve(page_texts.size());
  for (size_t i = 0; i < page_texts.size(); ++i) {
    auto segment = read_aloud::mojom::TextSegment::New();
    segment->segment_index = static_cast<uint32_t>(i);
    segment->text = base::UTF8ToUTF16(page_texts[i]);
    segment->speaker = read_aloud::mojom::Speaker::kSpeaker1;
    segments.push_back(std::move(segment));
  }
  utility_player_->SetTextContent(std::move(segments));
}

void ReadAloudService::RequestOverviewGeneration(
    const std::vector<std::string_view>& page_texts) {
  if (delegate_) {
    delegate_->OnHighlightingSupported(false);
  }

  OptimizationGuideKeyedService* opt_guide_service =
      profile_ ? OptimizationGuideKeyedServiceFactory::GetForProfile(profile_)
               : nullptr;

  std::string page_content = base::JoinString(page_texts, "\n\n");
  GURL page_url =
      web_contents() ? web_contents()->GetLastCommittedURL() : GURL();

  // AI Overviews are supported in English language only.
  // Hardcoding the language code maximizes prompt/prefix cache hit rates.
  overview_generation_broker_->GenerateOverview(
      opt_guide_service, article_title_, page_content, page_url,
      kAiOverviewLanguageCode,
      base::BindOnce(&ReadAloudService::OnOverviewGenerated,
                     weak_factory_.GetWeakPtr()));
}

void ReadAloudService::OnOverviewGenerated(mojo_base::BigBuffer response_bytes,
                                           bool success) {
  if (!utility_player_.is_bound() ||
      playback_mode_ != PlaybackMode::kOverview) {
    return;
  }

  if (!success || response_bytes.size() == 0) {
    // TODO(b/564908361): Record UMA metric for overview generation failure.
    HandlePlaybackError("Overview generation failed");
    return;
  }

  // TODO(cl/8542391): Prevent unintended play signal if user requested
  // pause during generation.

  // Signal Play() so that content starts playing as soon
  // as the overview text content is set.
  utility_player_->Play();
  utility_player_->SetOverviewContent(
      std::move(response_bytes),
      base::BindOnce(&ReadAloudService::OnOverviewContentSet,
                     weak_factory_.GetWeakPtr()));
}

void ReadAloudService::OnOverviewContentSet(bool success,
                                            const std::string& title) {
  if (!utility_player_.is_bound() ||
      playback_mode_ != PlaybackMode::kOverview) {
    return;
  }

  if (!success) {
    // TODO(b/564908361): Record UMA metric for overview content parsing
    // failure.
    HandlePlaybackError("Overview content parsing failed");
    return;
  }

  // Set the generated overview title.
  const std::string sanitized_title = SanitizeMetadata(title);
  if (delegate_) {
    delegate_->OnMetadataAvailable(
        sanitized_title.empty() ? article_title_ : sanitized_title,
        current_publisher_);
  }

  // TODO(b/564908361): Record UMA metric for overview generation success.
  overview_script_loaded_ = true;
}

bool ReadAloudService::IsWaitingForOverviewContent() const {
  return playback_mode_ == PlaybackMode::kOverview && !overview_script_loaded_;
}

void ReadAloudService::OnArticleUpdated(
    dom_distiller::ArticleDistillationUpdate article_update) {}

void ReadAloudService::Initialize(content::WebContents* new_web_contents) {
  if (!new_web_contents) {
    return;
  }
  ResetPlayback();
  Observe(new_web_contents);
  media_session_ =
      std::make_unique<ReadAloudPlaybackSession>(new_web_contents, this);

  current_duration_ = base::Seconds(0);

  ProvideInitialMetadata();
  NotifyPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaybackCreation);
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
  article_title_ = base::UTF16ToUTF8(web_contents()->GetTitle());
  current_publisher_ = base::UTF16ToUTF8(
      url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
          web_contents()->GetLastCommittedURL()));

  if (delegate_) {
    delegate_->OnMetadataAvailable(article_title_, current_publisher_);
  }
}

// Ensures the sandboxed ReadAloudPlaybackController utility process is running
// and bound. Uses `controller_binder_` if provided (e.g. in unit tests);
// otherwise launches `ReadAloudPlaybackControllerFactory` via
// ServiceProcessHost and requests a new controller instance with our client
// observer remote.
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
    HandlePlaybackError("Failed to initialize audio output stream");
    return;
  }

  utility_player_->InitializeAudio(std::move(stream_remote),
                                   std::move(data_pipe), params);
}

void ReadAloudService::OnUtilityDisconnect() {
  HandlePlaybackError("Utility process disconnected");
}

void ReadAloudService::HandlePlaybackError(std::string_view error_message) {
  Stop();
  if (delegate_) {
    delegate_->OnPlaybackError(error_message);
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
    read_aloud::mojom::PlaybackState state) {
  switch (state) {
    case read_aloud::mojom::PlaybackState::kPaused:
    case read_aloud::mojom::PlaybackState::kBuffering:
    case read_aloud::mojom::PlaybackState::kPlaying:
    case read_aloud::mojom::PlaybackState::kEndOfStream:
      // TODO(b/562011435): Utility-initiated pauses do not reach MediaSession.
      // media_session_->NotifyPlaybackPaused() should be called so the OS
      // notification reflects the paused state.
      NotifyPlaybackStateChanged(state);
      return;
    case read_aloud::mojom::PlaybackState::kError:
      HandlePlaybackError("Playback error reported by utility process");
      return;
    case read_aloud::mojom::PlaybackState::kStopped:
    case read_aloud::mojom::PlaybackState::kPlaybackCreation:
      utility_observer_receiver_.ReportBadMessage(
          "ReadAloudService: browser-only PlaybackState received from utility");
      return;
  }
}

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
    read_aloud::mojom::Speaker speaker,
    uint64_t /*sequence_id*/,
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

  std::string_view voice_id_override;
  if (playback_mode_ == PlaybackMode::kOverview) {
    voice_id_override = (speaker == read_aloud::mojom::Speaker::kSpeaker2)
                            ? SpeechSynthesisBroker::kOverviewVoiceSpeaker2
                            : SpeechSynthesisBroker::kOverviewVoiceSpeaker1;
  }

  speech_synthesis_broker_->SynthesizeSpeech(
      opt_guide_service, text_chunk, voice_id_override, std::move(callback));
}

}  // namespace readaloud
