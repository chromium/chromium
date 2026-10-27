// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_service.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
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
#include "chrome/browser/readaloud/read_aloud_voice_catalog.h"
#include "chrome/browser/readaloud/voice_preview_controller.h"
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

constexpr char kOverviewOutcomeHistogram[] = "ReadAloud.Overview.Outcome";

}  // namespace

ReadAloudService::ReadAloudService(
    Profile* profile,
    PlaybackControllerBinder controller_binder,
    ReadAloudAudioBroker::AudioStreamFactoryBinder factory_binder)
    : profile_(profile),
      controller_binder_(controller_binder),
      audio_broker_(std::make_unique<ReadAloudAudioBroker>(factory_binder)),
      speech_synthesis_broker_(std::make_unique<SpeechSynthesisBroker>()),
      overview_generation_broker_(std::make_unique<OverviewGenerationBroker>()),
      voice_preview_controller_(std::make_unique<VoicePreviewController>(
          profile,
          speech_synthesis_broker_.get(),
          base::BindRepeating(&ReadAloudService::OnVoicePreviewStateChanged,
                              base::Unretained(this)),
          std::move(controller_binder),
          std::move(factory_binder))) {}

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

void ReadAloudService::OnVoicePreviewStateChanged(
    std::string_view voice_id,
    read_aloud::mojom::PlaybackState state) {
  if (delegate_) {
    delegate_->OnVoicePreviewPlaybackStateChanged(voice_id, state);
  }
}

void ReadAloudService::Play(content::WebContents* new_web_contents) {
  if (!new_web_contents) {
    return;
  }

  ResetVoicePreview();

  if (new_web_contents != web_contents()) {
    Initialize(new_web_contents);
  }

  user_requested_play_ = true;

  // In overview mode, the utility Play() is deferred until
  // OnOverviewGenerated() has loaded the script, so the utility play-on-ready
  // watchdog does not race the browser-owned overview generation wait.
  if (utility_player_.is_bound() && !IsWaitingForOverviewContent()) {
    utility_player_->Play();
  }
}

void ReadAloudService::Pause() {
  user_requested_play_ = false;

  if (utility_player_.is_bound()) {
    utility_player_->Pause();
  }
}

void ReadAloudService::Stop() {
  ResetVoicePreview();
  ResetPlayback();
  NotifyPlaybackStateChanged(read_aloud::mojom::PlaybackState::kStopped);
}

void ReadAloudService::ResetPlayback() {
  user_requested_play_ = false;

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
  overview_generation_start_time_ = base::TimeTicks();
  article_title_.clear();
  current_publisher_.clear();
  highlight_text_length_ = 0;

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

void ReadAloudService::SeekRelative(base::TimeDelta offset) {
  if (offset.is_inf()) {
    return;
  }
  if (utility_player_.is_bound()) {
    utility_player_->SeekRelative(offset);
  }
}

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
void ReadAloudService::PreviewVoice(std::string_view voice_id,
                                    std::u16string_view preview_text) {
  // Pause active article playback while previewing a voice.
  Pause();

  if (voice_preview_controller_) {
    voice_preview_controller_->PlayPreview(voice_id, preview_text);
  }
}

void ReadAloudService::PauseVoicePreview() {
  if (voice_preview_controller_) {
    voice_preview_controller_->PausePreview();
  }
}

void ReadAloudService::StopVoicePreview() {
  if (voice_preview_controller_) {
    voice_preview_controller_->StopPreview();
  }
}

void ReadAloudService::ResetVoicePreview() {
  if (voice_preview_controller_) {
    voice_preview_controller_->StopPreview();
    // Guard against clobbering a new preview if `state_changed_callback_`
    // re-entrantly started one during `StopPreview()`.
    if (!voice_preview_controller_->IsPreviewActive()) {
      voice_preview_controller_->Reset();
    }
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

void ReadAloudService::InitVoices() {
  if (delegate_) {
    delegate_->OnVoicesAvailable(kAvailableVoices);
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

// `OnSessionSuspended()` and `OnSessionResumed()` run synchronously while
// `MediaSession` iterates over its players. They must not destroy or replace
// `media_session_`.
void ReadAloudService::OnSessionSuspended() {
  Pause();
}

void ReadAloudService::OnSessionResumed() {
  // `media_session_` observes the same `WebContents` as this service, so
  // `Play()` does not call `Initialize()`, which would destroy
  // `media_session_`.
  CHECK(media_session_);
  CHECK_EQ(media_session_->web_contents(), web_contents());
  Play(web_contents());
}

void ReadAloudService::Shutdown() {
  ResetPlayback();
  voice_preview_controller_.reset();
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

  overview_generation_start_time_ = base::TimeTicks::Now();

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
    overview_generation_start_time_ = base::TimeTicks();
    base::UmaHistogramEnumeration(kOverviewOutcomeHistogram,
                                  OverviewOutcome::kGenerationFailed);
    HandlePlaybackError("Overview generation failed");
    return;
  }

  // If user did not cancel play, signal Play() so that content
  // starts playing as soon as the overview text content is set.
  if (user_requested_play_) {
    utility_player_->Play();
  }
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
    overview_generation_start_time_ = base::TimeTicks();
    base::UmaHistogramEnumeration(kOverviewOutcomeHistogram,
                                  OverviewOutcome::kContentParsingFailed);
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

  if (!overview_generation_start_time_.is_null()) {
    base::UmaHistogramMediumTimes(
        "ReadAloud.Overview.TotalDuration",
        base::TimeTicks::Now() - overview_generation_start_time_);
    overview_generation_start_time_ = base::TimeTicks();
  }
  base::UmaHistogramEnumeration(kOverviewOutcomeHistogram,
                                OverviewOutcome::kSuccess);
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
  ResetVoicePreview();
  ResetPlayback();
  Observe(new_web_contents);
  media_session_ =
      std::make_unique<ReadAloudPlaybackSession>(new_web_contents, this);

  ProvideInitialMetadata();
  NotifyPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaybackCreation);
  if (delegate_) {
    delegate_->OnPlaybackProgressUpdated(base::TimeDelta(), base::TimeDelta());
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
    case read_aloud::mojom::PlaybackState::kEndOfStream:
      user_requested_play_ = false;
      if (media_session_) {
        media_session_->NotifyPlaybackPaused();
      }
      NotifyPlaybackStateChanged(state);
      return;
    case read_aloud::mojom::PlaybackState::kPaused:
      if (media_session_) {
        media_session_->NotifyPlaybackPaused();
      }
      NotifyPlaybackStateChanged(state);
      return;
    case read_aloud::mojom::PlaybackState::kBuffering:
      NotifyPlaybackStateChanged(state);
      return;
    case read_aloud::mojom::PlaybackState::kPlaying:
      if (media_session_) {
        media_session_->NotifyPlaybackStarted();
      }
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

void ReadAloudService::OnPlaybackProgressChanged(base::TimeDelta elapsed,
                                                 base::TimeDelta duration) {
  if (elapsed.is_negative() || duration.is_negative() ||
      duration > kMaxPlaybackDuration || elapsed > duration) {
    utility_observer_receiver_.ReportBadMessage(
        "ReadAloudService: invalid playback progress received from utility");
    return;
  }
  if (delegate_) {
    delegate_->OnPlaybackProgressUpdated(elapsed, duration);
  }
}

void ReadAloudService::OnWordBoundaryReached(uint32_t start_character_offset,
                                             uint32_t end_character_offset) {
  if (start_character_offset > end_character_offset) {
    utility_observer_receiver_.ReportBadMessage(
        "ReadAloudService: word boundary start exceeds end");
    return;
  }
  if (!delegate_) {
    return;
  }

  // The offsets are already in the highlighter's coordinate space, so they are
  // forwarded unchanged. Empty words and words past the forwarded chunks (e.g.
  // beyond `kMaxTextChunks`, which the UI has no text for) are dropped.
  if (start_character_offset < end_character_offset &&
      end_character_offset <= highlight_text_length_) {
    delegate_->OnWordHighlightUpdated(
        base::saturated_cast<int>(start_character_offset),
        base::saturated_cast<int>(end_character_offset));
  }
}

void ReadAloudService::OnTextChunked(
    const std::vector<std::u16string>& chunks) {
  if (chunks.size() > readaloud::kMaxTextChunks) {
    mojo::ReportBadMessage("Received invalid chunk payload");
    return;
  }

  // Must match the word offsets set by `PlaybackTimeline::SetTextContent()` in
  // the utility process: one separator between chunks. No word can end in the
  // separator after the last chunk, so the bound stops at the last chunk's end.
  highlight_text_length_ = 0;
  for (const std::u16string& chunk : chunks) {
    highlight_text_length_ += chunk.size();
  }
  if (!chunks.empty()) {
    highlight_text_length_ += chunks.size() - 1;
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
