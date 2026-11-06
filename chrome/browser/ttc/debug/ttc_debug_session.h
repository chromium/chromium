// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_DEBUG_TTC_DEBUG_SESSION_H_
#define CHROME_BROWSER_TTC_DEBUG_TTC_DEBUG_SESSION_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/ttc/debug/ttc_session_audio_recorder.h"
#include "components/ttc/app/public/debug_sink.h"

namespace base {
class TickClock;
}

namespace ttc {

class SessionJournal;

// Debug record of one TTC session, fed by the pipeline through the DebugSink
// interface. It keeps:
//  - per-stage pipeline health and counters (Snapshot),
//  - a timeline of events for the whole session,
//  - the exact audio sent and received (TtcSessionAudioRecorder).
// Input is ignored once the session has ended. Observers (the debug page)
// receive batched updates at most every kFlushInterval.
//
// Memory is bounded: text is truncated (kMaxTextLength).
class TtcDebugSession final : public DebugSink {
 public:
  enum class Stage {
    kMic,
    kEncode,
    kSend,
    kServer,
    kReceive,
    kSpeaker,
    kMaxValue = kSpeaker,
  };
  static constexpr size_t kStageCount =
      static_cast<size_t>(Stage::kMaxValue) + 1;
  enum class Health { kIdle, kOk, kWarning, kError };

  struct StageStatus {
    Health health = Health::kIdle;
    std::string detail;
  };

  struct Snapshot {
    Snapshot();
    Snapshot(const Snapshot&);
    Snapshot& operator=(const Snapshot&);
    ~Snapshot();

    double now_ms = 0;
    bool ended = false;
    bool transport_connected = false;
    std::string server_session_id;
    int64_t chunks_sent = 0;
    int64_t bytes_sent = 0;
    int64_t frames_dropped = 0;
    int64_t server_frames = 0;
    int64_t audio_chunks_received = 0;
    int64_t audio_chunks_played = 0;
    std::string last_error;
    size_t audio_bytes = 0;
    size_t audio_cap_bytes = 0;
    bool audio_cap_reached = false;
    std::array<StageStatus, kStageCount> stages;
  };

  enum class EventKind {
    kMicLevel,
    kChunkSent,
    kFrameDropped,
    kServerFrame,
    kOutputLevel,
    kPlayback,
    kTranscriptIn,
    kTranscriptOut,
    kTurnComplete,
    kInterrupted,
    kTransport,
    kCaptureMuted,
    kError,
    kSessionStarted,
    kAudioCapReached,
    kSessionEnded,
  };

  struct TimelineEvent {
    double t_ms = 0;
    EventKind kind = EventKind::kMicLevel;
    float value = 0;
    std::string label;
  };

  // What changed since the previous update.
  struct Update {
    Update();
    Update(const Update&);
    Update& operator=(const Update&);
    ~Update();

    Snapshot snapshot;
    // Index in events() of new_events[0].
    size_t first_event_index = 0;
    std::vector<TimelineEvent> new_events;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Observers fetch the full state when they start observing; updates only
    // cover what happened after AddObserver().
    virtual void OnDebugUpdate(const Update& update) = 0;
  };

  static constexpr float kDigitalSilenceLevel = 1e-4f;
  static constexpr base::TimeDelta kFlushInterval = base::Milliseconds(100);

  // Memory bounds. The frequent events (IsFrequentEvent()), several a second
  // while audio flows, have a cap of their own, so that they can't crowd out
  // the others in a long session.
  static constexpr size_t kMaxFrequentEvents = 200000;
  static constexpr size_t kMaxOtherEvents = 50000;
  // Total bytes of event labels; further events are dropped like past their
  // cap.
  static constexpr size_t kMaxEventLabelBytes = 16 * 1024 * 1024;
  // Labels.
  static constexpr size_t kMaxTextLength = 4096;

  // Stage health: how recent a signal must be to count.
  static constexpr base::TimeDelta kActivityWindow = base::Seconds(1);
  static constexpr base::TimeDelta kFrameDropWindow = base::Seconds(2);
  static constexpr base::TimeDelta kReceiveWindow = base::Seconds(3);
  static constexpr base::TimeDelta kErrorWindow = base::Seconds(5);

  // Detector thresholds.
  static constexpr int kMicSilenceIssueChunks = 30;  // 3 s.
  static constexpr base::TimeDelta kPlaybackStallTimeout = base::Seconds(2);

  // `journal` is optional and is detached when the session ends. `clock` must
  // outlive this object.
  TtcDebugSession(SessionJournal* journal,
                  size_t audio_cap_bytes,
                  const base::TickClock* clock);
  TtcDebugSession(const TtcDebugSession&) = delete;
  TtcDebugSession& operator=(const TtcDebugSession&) = delete;
  ~TtcDebugSession() override;

  static std::string_view StageName(Stage stage);
  static std::string_view HealthName(Health health);
  static std::string_view EventKindName(EventKind kind);
  // Whether events of `kind` come several a second while audio flows: mic and
  // output levels, chunks sent and playback. They're left out of ToValue().
  static bool IsFrequentEvent(EventKind kind);

  // Marks the TTC session as ended. Data stays available for the debug page.
  void End();

  bool ended() const { return ended_; }

  Snapshot GetSnapshot() const;
  const std::vector<TimelineEvent>& events() const { return events_; }
  const TtcSessionAudioRecorder& recorder() const { return recorder_; }

  // Everything except audio, for attaching to bugs. This includes the
  // transcripts of what the user and the model said.
  base::DictValue ToValue() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  base::WeakPtr<TtcDebugSession> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // DebugSink:
  void OnCaptureFormat(const CaptureFormat& format) override;
  void OnCaptureMuted(bool muted) override;
  void OnMicLevel(float rms) override;
  void OnClientFrame(const ClientFrame& frame) override;
  void OnFrameDropped() override;
  void OnTransportStateChanged(bool connected) override;
  void OnServerFrame(const ServerFrame& frame) override;
  void OnRenderError() override;

  // The session reported an error, fatal or not. `error` is the name of its
  // ErrorCode, e.g. "SetupTimeout".
  void OnError(std::string_view error);

 private:
  // Returns `text` cut to at most `max_length` bytes, ending in an ellipsis
  // (U+2026) if it was cut.
  static std::string Truncate(std::string_view text,
                              size_t max_length = kMaxTextLength);
  // Like Truncate(), in place.
  static void TruncateInPlace(std::string& text,
                              size_t max_length = kMaxTextLength);

  // Time since the session started.
  base::TimeDelta Elapsed() const;
  double NowMs() const;
  void AddEvent(EventKind kind, float value = 0, std::string label = {});
  // Adds the timeline marker of a server frame, labelled "<kind>:<detail>",
  // e.g. "transcript:input".
  void AddServerFrameEvent(std::string_view kind, std::string_view detail = {});
  // Server frames, by type (OnServerFrame()).
  void HandleFrame(const SessionStatus& status);
  void HandleFrame(const AudioOutput& audio);
  void HandleFrame(const Transcript& transcript);
  void HandleFrame(const TurnComplete& turn_complete);
  void HandleFrame(const Interrupted& interrupted);
  void HandleFrame(const DebugSink::ToolCall& tool_call);
  void HandleFrame(const ServerError& error);
  void HandleFrame(const GoAway& go_away);
  void HandleFrame(const ServerJournalEvent& event);
  void HandleFrame(const ModelTurnState& state);
  void HandleFrame(const UserTurnState& state);
  void HandleFrame(const UnknownFrame& frame);
  void LogToJournal(std::string_view name, std::string detail);
  void ComputeStages(Snapshot& snapshot) const;
  void MaybeReportCapReached();
  // Whether audio arrived after the last playback and wasn't dropped by an
  // interruption.
  bool HasUnplayedAudio() const;
  void MaybeStartFlushTimer();
  void Flush();

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<SessionJournal> journal_;
  const raw_ptr<const base::TickClock> clock_;
  const base::TimeTicks start_time_;
  bool ended_ = false;

  // Capture.
  std::optional<CaptureFormat> capture_format_;
  bool capture_muted_ = false;
  std::optional<double> last_mic_level_ms_;
  float last_mic_level_ = 0;
  int consecutive_silent_chunks_ = 0;

  // Transport.
  bool transport_connected_ = false;
  bool ever_connected_ = false;
  std::string server_session_id_;
  int64_t chunks_sent_ = 0;
  int64_t bytes_sent_ = 0;
  std::optional<double> last_chunk_sent_ms_;
  int64_t frames_dropped_ = 0;
  std::optional<double> last_frame_dropped_ms_;
  int64_t server_frames_ = 0;
  std::optional<double> last_server_frame_ms_;
  std::optional<double> last_server_error_ms_;

  // Output.
  int64_t audio_chunks_received_ = 0;
  int64_t audio_chunks_played_ = 0;
  std::optional<double> last_audio_received_ms_;
  std::optional<double> last_playback_ms_;
  // Audio queued before an interruption is dropped rather than played.
  std::optional<double> last_interrupted_ms_;
  std::optional<double> last_render_error_ms_;

  std::string last_error_;

  std::vector<TimelineEvent> events_;
  size_t frequent_events_ = 0;
  size_t event_label_bytes_ = 0;
  // Whether events were dropped because of the caps, frequent ones and the
  // others (the ones ToValue() exports) separately.
  bool frequent_events_truncated_ = false;
  bool events_truncated_ = false;

  TtcSessionAudioRecorder recorder_;
  bool cap_reported_ = false;

  // Pending observer updates.
  size_t first_unflushed_event_ = 0;
  base::RepeatingTimer flush_timer_;
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<TtcDebugSession> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_DEBUG_TTC_DEBUG_SESSION_H_
