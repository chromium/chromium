// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_READ_ALOUD_SERVICE_H_
#define CHROME_BROWSER_READALOUD_READ_ALOUD_SERVICE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/readaloud/read_aloud_audio_broker.h"
#include "chrome/common/readaloud/read_aloud.mojom.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

class Profile;

namespace readaloud {

class ReadAloudPlaybackSession;
class ReadAloudServiceTest;
class SpeechSynthesisBroker;

// Central lifecycle and state orchestrator for the Read Aloud feature in
// Chrome, which allows users to listen to web page content.
//
// Manages Read Aloud audio playback of distilled web page content by
// coordinating page distillation (via `DomDistillerService`), audio synthesis
// in the utility process, and UI state synchronization via `Delegate`.
//
// Instantiated per-Profile by `ReadAloudServiceFactory`. Observes the target
// `content::WebContents` during active playback, starting when the user
// requests playback (e.g., tapping "Listen to this page" or the Play button in
// the UI) until playback is `Stop()`ped or the tab is closed. It continues
// tracking the specific `WebContents` on which `Play()` was called even if the
// user switches focus to a different tab.
class ReadAloudService
    : public KeyedService,
      public dom_distiller::ViewRequestDelegate,
      public read_aloud::mojom::ReadAloudPlaybackControllerClient,
      public content::WebContentsObserver {
 public:
  // TODO(b/522830940): Share this enum with Java using java_cpp_enum.
  enum class PlaybackState {
    // Unknown state.
    kUnknown = 0,
    // An error occurred during playback.
    kError = 1,
    // 2 is skipped to maintain alignment with the legacy Read Aloud on Android.
    // Buffering (audio is loading and not playing).
    kBuffering = 3,
    // Playback is paused.
    kPaused = 4,
    // Audio is actively playing.
    kPlaying = 5,
    // Playback is stopped (represents the end of playback).
    kStopped = 6,
    // Playback session is currently being created.
    kPlaybackCreation = 7,
  };

  enum class PlaybackMode {
    // Unspecified playback mode.
    kUnspecified = 0,
    // Classic mode: reads the full text of the distilled article.
    kClassic = 1,
    // Overview mode: reads a summarized version of the distilled article.
    kOverview = 2,
  };

  enum class FeedbackType {
    // No feedback provided.
    kNone = 0,
    // Positive feedback (e.g. thumbs up).
    kPositive = 1,
    // Negative feedback (e.g. thumbs down).
    kNegative = 2,
  };

  struct Voice {
    std::string id;
    std::string display_name;
  };

  // Interface for dispatching events from the native service to the UI.
  // State changes are sent to the UI via this delegate.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the article's title and publisher are available.
    virtual void OnMetadataAvailable(std::string_view title,
                                     std::string_view publisher) = 0;

    // Called periodically during playback to update progress bar and time displays.
    virtual void OnPlaybackProgressUpdated(base::TimeDelta elapsed,
                                           base::TimeDelta duration) = 0;

    // Called when playback state changes (e.g. playing, paused, buffering).
    virtual void OnPlaybackStateChanged(PlaybackState playback_state) = 0;

    // Called when the available voices list and currently selected voice are updated.
    virtual void OnVoicesAvailable(const std::vector<Voice>& voices,
                                   std::string_view selected_voice_id) = 0;

    // Called when a new word is reached, providing offsets for UI highlighting.
    virtual void OnWordHighlightUpdated(int absolute_start_index,
                                        int absolute_end_index) = 0;

    // Called to notify if word highlighting is supported for the current content.
    virtual void OnHighlightingSupported(bool supported) = 0;

    // Called when playback switches to the on-device system TTS engine.
    virtual void OnFallbackEngaged() = 0;

    // Called when an unrecoverable playback error occurs.
    virtual void OnPlaybackError(std::string_view error_message) = 0;

    // Called when the playback state of a voice preview changes in settings.
    virtual void OnVoicePreviewPlaybackStateChanged(
        std::string_view voice_id,
        PlaybackState playback_state) = 0;

    // Called with the result of a page readability assessment.
    virtual void OnReadabilityResult(const GURL& url, bool is_readable) = 0;

    // Called immediately before the native service is destroyed.
    virtual void OnNativeDestroyed() = 0;

    // Called when the text content is chunked into sentences by the utility
    // process.
    virtual void OnTextChunked(const std::vector<std::u16string>& chunks) = 0;
  };

  // Callback type used to inject a fake or mock ReadAloudPlaybackController receiver
  // during unit testing without friending test classes or exposing ForTesting methods.
  using PlaybackControllerBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>,
      mojo::PendingRemote<
          read_aloud::mojom::ReadAloudPlaybackControllerClient>)>;

  explicit ReadAloudService(
      Profile* profile,
      PlaybackControllerBinder controller_binder = {},
      ReadAloudAudioBroker::AudioStreamFactoryBinder factory_binder = {});

  ReadAloudService(const ReadAloudService&) = delete;
  ReadAloudService& operator=(const ReadAloudService&) = delete;

  ~ReadAloudService() override;

  void SetDelegate(std::unique_ptr<Delegate> delegate);
  Delegate* delegate() const { return delegate_.get(); }

  // Playback control commands called by the UI (via the JNI bridge).
  // Starts or resumes audio playback.
  void Play(content::WebContents* new_web_contents);

  // Pauses the current audio playback.
  void Pause();

  // Stops audio playback and releases playback resources.
  void Stop();

  // Seeks to the start of the word at the specified index in the text (e.g., tap-to-seek).
  void SeekToWordIndex(int word_index);

  // Seeks to a specific absolute time offset from the beginning of the audio.
  void Seek(base::TimeDelta absolute_time);

  // Seeks forward or backward relatively (e.g., for the +10s / -10s skip buttons).
  void SeekRelative(base::TimeDelta offset);

  // Adjusts the audio playback speed (rate multiplier).
  void SetPlaybackRate(float rate);

  // Sets the voice to be used for text-to-speech synthesis.
  void SetVoice(std::string_view voice_id);

  // Sets the target language code for text-to-speech synthesis.
  void SetLanguageCode(std::string_view language_code);

  // Plays a short audio sample of the specified voice.
  void PreviewVoice(std::string_view voice_id);

  // Stops the active voice preview playback.
  void StopVoicePreview();

  // Sets the playback mode (e.g., classic full read or summary overview).
  void SetPlaybackMode(PlaybackMode mode);
  PlaybackMode playback_mode() const { return playback_mode_; }

  // Enables or disables synchronized word highlighting in the UI.
  void SetHighlightingEnabled(bool enabled);

  // Submits user feedback (e.g., thumbs up/down) for logging.
  void SendFeedback(FeedbackType feedback_type);

  // Initiates an asynchronous check to determine if the URL is readable.
  void CheckReadability(const GURL& url);

  // Methods called by ReadAloudPlaybackSession:
  void OnSessionSuspended();
  void OnSessionResumed();

  bool IsPlaybackPaused() const;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void PrimaryPageChanged(content::Page& page) override;

  // KeyedService:
  void Shutdown() override;

  // Triggers distillation of a webpage using DomDistillerService.
  void DistillPage(content::WebContents* web_contents);

  // dom_distiller::ViewRequestDelegate:
  void OnArticleReady(
      const dom_distiller::DistilledArticleProto* article_proto) override;
  void OnArticleUpdated(
      dom_distiller::ArticleDistillationUpdate article_update) override;
  void OnDistillationFailed(
      dom_distiller::DistillationParseResult reason) override;

  // Stops any active playback session and restarts the service lifecycle for
  // the given `web_contents`, triggering page distillation and ensuring the
  // utility process is connected.
  void Initialize(content::WebContents* web_contents);

  // TODO(b/553612030): Refactor out GetViewerHandleForTesting().
  dom_distiller::ViewerHandle* GetViewerHandleForTesting() const {
    return viewer_handle_.get();
  }

  // read_aloud::mojom::ReadAloudPlaybackControllerClient (called by Utility):
  void OnPlaybackStateChanged(read_aloud::mojom::PlaybackState state) override;
  void OnPlaybackDurationChanged(base::TimeDelta duration) override;
  void OnWordBoundaryReached(uint32_t segment_index,
                             uint32_t character_offset,
                             base::TimeDelta audio_timestamp) override;
  void OnTextChunked(const std::vector<std::u16string>& chunks) override;
  void RequestSpeechSynthesis(
      const std::u16string& text_chunk,
      uint64_t sequence_id,
      read_aloud::mojom::ReadAloudPlaybackControllerClient::
          RequestSpeechSynthesisCallback callback) override;

 private:
  // Sends page title and publisher metadata to the UI delegate.
  void ProvideInitialMetadata();
  void EnsurePlaybackControllerConnected();
  void InitializeAudioStream();
  void OnAudioStreamCreated(
      const media::AudioParameters& params,
      mojo::PendingRemote<media::mojom::AudioOutputStream> stream_remote,
      media::mojom::ReadWriteAudioDataPipePtr data_pipe);
  void OnUtilityDisconnect();
  void ResetUtilityConnection();
  PlaybackState GetCurrentPlaybackState() const;

  raw_ptr<Profile> profile_;
  PlaybackControllerBinder controller_binder_;
  std::unique_ptr<ReadAloudAudioBroker> audio_broker_;
  std::unique_ptr<dom_distiller::ViewerHandle> viewer_handle_;
  std::unique_ptr<Delegate> delegate_;
  base::TimeTicks distillation_start_time_;
  std::string current_title_;
  std::string current_publisher_;
  base::TimeDelta current_duration_;

  // Connection to the Utility process Factory.
  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackControllerFactory>
      player_factory_;

  // Connections to the Utility process Controller.
  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackController> utility_player_;
  mojo::Receiver<read_aloud::mojom::ReadAloudPlaybackControllerClient>
      utility_observer_receiver_{this};

  std::unique_ptr<ReadAloudPlaybackSession> active_session_;
  std::unique_ptr<SpeechSynthesisBroker> speech_synthesis_broker_;
  PlaybackMode playback_mode_ = PlaybackMode::kClassic;

  base::WeakPtrFactory<ReadAloudService> weak_factory_{this};
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_READ_ALOUD_SERVICE_H_
