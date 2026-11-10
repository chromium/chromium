// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_DEBUG_TTC_DEBUG_SESSION_H_
#define CHROME_BROWSER_TTC_DEBUG_TTC_DEBUG_SESSION_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/function_ref.h"
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
// interface and by ToolController through the browser-side methods below. It
// keeps:
//  - per-stage pipeline health and counters (Snapshot),
//  - a timeline of events for the whole session,
//  - per-turn latency records and a "no response" watchdog,
//  - tool calls traced to their effect, and problems detected in real time
//    (ttc_debug_session_issues.cc),
//  - the exact audio sent and received (TtcSessionAudioRecorder).
// Input is ignored once the session has ended. Observers (the debug page)
// receive batched updates at most every kFlushInterval.
//
// Memory is bounded: text is truncated (kMaxTextLength, kMaxTranscriptLength),
// and tool calls, issues and turns beyond their caps are dropped and counted in
// Snapshot::dropped_items.
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
    bool speaking = false;
    bool no_response_suspected = false;
    std::string no_response_detail;
    std::string last_error;
    size_t audio_bytes = 0;
    size_t audio_cap_bytes = 0;
    bool audio_cap_reached = false;
    int open_errors = 0;
    int open_warnings = 0;
    // Tool calls, issues and turns dropped because their cap was reached.
    int64_t dropped_items = 0;
    std::array<StageStatus, kStageCount> stages;
  };

  enum class EventKind {
    kMicLevel,
    kChunkSent,
    kFrameDropped,
    kServerFrame,
    kOutputLevel,
    kPlayback,
    kSpeechStart,
    kSpeechEnd,
    kTranscriptIn,
    kTranscriptOut,
    kTurnComplete,
    kInterrupted,
    kNoResponseSuspected,
    kTransport,
    kCaptureMuted,
    kError,
    kSessionStarted,
    kAudioCapReached,
    kItemsTruncated,
    kSessionEnded,
    kToolStart,
    kToolEnd,
    kIssue,
    kActor,
    kServerJournal,
    kContextSent,
  };

  struct TimelineEvent {
    double t_ms = 0;
    EventKind kind = EventKind::kMicLevel;
    float value = 0;
    std::string label;
  };

  enum class TurnStatus { kOpen, kOk, kNoAudio, kInterrupted, kNoResponse };

  struct Turn {
    int index = 0;
    TurnStatus status = TurnStatus::kOpen;
    std::optional<double> speech_start_ms;
    double speech_end_ms = 0;
    std::optional<double> first_transcript_ms;
    std::optional<double> first_audio_ms;
    std::optional<double> first_playback_ms;
    // Where this turn's server audio sits in the recorded output stream.
    std::optional<double> audio_out_start_ms;
    std::optional<double> audio_out_end_ms;
    std::string input_transcript;
    std::string output_transcript;
  };

  // State of a tab a tool acted on.
  struct TabSnapshot {
    int tab_id = -1;
    std::string url;
    std::string title;
    bool visible = false;
    // Whether it is the active tab of its window.
    bool active_in_window = false;
  };

  enum class ToolStatus { kPending, kRunning, kOk, kError, kRejected };
  enum class ToolEffect { kNotChecked, kChecking, kVerified, kNoEffect };

  // One tool call, traced from the server's request to the model's follow-up.
  struct ToolCall {
    ToolCall();
    ToolCall(const ToolCall&);
    ToolCall& operator=(const ToolCall&);
    ~ToolCall();

    int id = 0;
    std::string call_id;
    std::string name;
    std::string arguments_json;
    int turn_index = 0;
    double received_ms = 0;
    std::optional<double> dispatched_ms;
    std::optional<double> finished_ms;
    std::optional<double> response_sent_ms;
    ToolStatus status = ToolStatus::kPending;
    ToolEffect effect = ToolEffect::kNotChecked;
    std::string effect_detail;
    std::optional<TabSnapshot> target_before;
    std::optional<TabSnapshot> target_after;
    std::string result_code;
    std::string result_message;
    std::string response_json;
    bool response_dropped = false;
    std::vector<std::string> actor_steps;
    bool model_followed_up = false;
  };

  enum class IssueSeverity { kInfo, kWarning, kError };
  enum class IssueCategory {
    kTool,
    kServer,
    kTransport,
    kAudio,
    kPageContext,
    kModel,
    kActor,
  };

  // A problem detected in real time.
  struct Issue {
    Issue();
    Issue(const Issue&);
    Issue& operator=(const Issue&);
    ~Issue();

    int id = 0;
    // Identifies repeated occurrences of the same problem.
    std::string key;
    IssueSeverity severity = IssueSeverity::kWarning;
    IssueCategory category = IssueCategory::kServer;
    std::string title;
    std::string detail;
    std::vector<std::pair<std::string, std::string>> evidence;
    double first_ms = 0;
    double last_ms = 0;
    int count = 1;
    std::optional<int> turn_index;
    std::optional<int> tool_call_id;
    // Condition-based issues resolve once the condition clears.
    bool resolved = false;
    std::optional<double> resolved_ms;
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
    std::vector<Turn> changed_turns;
    std::vector<ToolCall> changed_tool_calls;
    std::vector<Issue> changed_issues;
  };

  class Observer : public base::CheckedObserver {
   public:
    // Observers fetch the full state when they start observing; updates only
    // cover what happened after AddObserver().
    virtual void OnDebugUpdate(const Update& update) = 0;
  };

  // Voice activity thresholds on the mic RMS level (0..1).
  static constexpr float kSpeechOnLevel = 0.02f;
  static constexpr float kSpeechOffLevel = 0.01f;
  static constexpr float kDigitalSilenceLevel = 1e-4f;
  // Duration of one mic level / audio input chunk.
  static constexpr base::TimeDelta kChunkDuration = base::Milliseconds(100);
  static constexpr int kSpeechOnChunks = 3;   // 300 ms at 100 ms chunks.
  static constexpr int kSpeechOffChunks = 8;  // 800 ms.
  static constexpr base::TimeDelta kNoResponseTimeout = base::Seconds(6);
  static constexpr base::TimeDelta kFlushInterval = base::Milliseconds(100);

  // Memory bounds. The frequent events (IsFrequentEvent()), several a second
  // while audio flows, have a cap of their own, so that they can't crowd out
  // the others in a long session.
  static constexpr size_t kMaxFrequentEvents = 200000;
  static constexpr size_t kMaxOtherEvents = 50000;
  // Total bytes of event labels; further events are dropped like past their
  // cap.
  static constexpr size_t kMaxEventLabelBytes = 16 * 1024 * 1024;
  static constexpr size_t kMaxToolCalls = 1000;
  static constexpr size_t kMaxIssues = 1000;
  static constexpr size_t kMaxTurns = 1000;
  // Labels, tool arguments and responses, issue details and evidence.
  static constexpr size_t kMaxTextLength = 4096;
  // Each transcript of a turn.
  static constexpr size_t kMaxTranscriptLength = 16 * 1024;
  static constexpr size_t kMaxIssueKeyLength = 256;

  // Stage health: how recent a signal must be to count.
  static constexpr base::TimeDelta kActivityWindow = base::Seconds(1);
  static constexpr base::TimeDelta kFrameDropWindow = base::Seconds(2);
  static constexpr base::TimeDelta kReceiveWindow = base::Seconds(3);
  static constexpr base::TimeDelta kErrorWindow = base::Seconds(5);

  // Detector thresholds.
  static constexpr base::TimeDelta kToolTimeout = base::Seconds(15);
  static constexpr base::TimeDelta kModelFollowUpTimeout = base::Seconds(10);
  static constexpr int kMicSilenceIssueChunks = 30;  // 3 s.
  static constexpr base::TimeDelta kPlaybackStallTimeout = base::Seconds(2);
  static constexpr base::TimeDelta kDetectorInterval = base::Milliseconds(250);
  static constexpr size_t kMaxActorStepsPerTool = 60;

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
  static std::string_view TurnStatusName(TurnStatus status);
  static std::string_view ToolStatusName(ToolStatus status);
  static std::string_view ToolEffectName(ToolEffect effect);
  static std::string_view IssueSeverityName(IssueSeverity severity);
  static std::string_view IssueCategoryName(IssueCategory category);

  // Marks the TTC session as ended. Data stays available for the debug page.
  void End();

  bool ended() const { return ended_; }

  Snapshot GetSnapshot() const;
  const std::vector<TimelineEvent>& events() const { return events_; }
  const std::vector<Turn>& turns() const { return turns_; }
  const std::vector<ToolCall>& tool_calls() const { return tool_calls_; }
  const std::vector<Issue>& issues() const { return issues_; }
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

  // Tool calls rejected before they reached the browser, and tool responses.
  void OnToolCallRejected(std::string_view call_id, std::string_view reason);
  void OnToolResponseSent(std::string_view call_id,
                          bool sent,
                          std::string_view response_json);

  // Browser-side tool execution (from ToolController via the forwarder).
  // `verifying` is true if the effect of the tool will be checked.
  void OnToolDispatched(std::string_view call_id,
                        std::string_view name,
                        std::string_view arguments_json,
                        const std::optional<TabSnapshot>& target,
                        bool verifying);
  void OnToolNoTarget(std::string_view call_id, std::string_view name);
  // `result_code` is only displayed; `ok` says whether the tool succeeded.
  void OnToolFinished(std::string_view call_id,
                      bool ok,
                      std::string_view result_code,
                      std::string_view message,
                      const std::optional<TabSnapshot>& target_after);
  void OnToolEffect(std::string_view call_id,
                    bool verified,
                    std::string_view detail);

  // An actor journal entry for the session's actor task. `type` is 'B', 'E'
  // or 'I'.
  void OnActorJournalEntry(std::string_view event,
                           char type,
                           std::string_view details,
                           bool has_error);

  // Page context.
  void OnPageContextFailed(std::string_view url, std::string_view error);
  void OnPageContextBlocked(std::string_view url);

 private:
  // Returns `text` cut to at most `max_length` bytes, ending in an ellipsis
  // (U+2026) if it was cut.
  static std::string Truncate(std::string_view text,
                              size_t max_length = kMaxTextLength);
  // Like Truncate(), in place.
  static void TruncateInPlace(std::string& text,
                              size_t max_length = kMaxTextLength);
  // `tab` with its URL and title truncated.
  static TabSnapshot TruncateTab(const TabSnapshot& tab);

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
  // Returns the open turn, opening one if needed. Null if kMaxTurns is
  // reached.
  Turn* EnsureOpenTurn(double speech_end_ms);
  // The last turn, if it is still open.
  Turn* OpenTurn();
  void CloseOpenTurn(TurnStatus status);
  void MarkTurnChanged(const Turn& turn);
  // Counts an item dropped because of a cap, and reports the first one.
  void DropItem(std::string_view what);
  void OnServerActivity();
  void ArmWatchdog();
  void OnWatchdogFired();
  void ComputeStages(Snapshot& snapshot) const;
  void MaybeReportCapReached();

  // Problem tracking (ttc_debug_session_issues.cc).
  void HandleContextUpdate(const ContextUpdate& update);
  using Evidence = std::vector<std::pair<std::string, std::string>>;
  struct IssueSpec {
    IssueSeverity severity;
    IssueCategory category;
    std::string title;
    std::string detail;
    Evidence evidence;
    std::optional<int> tool_call_id;
  };
  // Raises the issue `key`, or bumps its count if it is already open.
  void RaiseIssue(std::string key, IssueSpec spec);
  // Raises "<kind>:<tool call id>" with the tool call's evidence attached.
  void RaiseToolIssue(std::string_view kind,
                      const ToolCall& tool_call,
                      IssueSeverity severity,
                      std::string title,
                      std::string detail);
  void ResolveIssue(std::string_view key);
  // Keeps the condition-based issue `key` open exactly while `active`.
  void SetCondition(std::string_view key,
                    bool active,
                    base::FunctionRef<IssueSpec()> spec);
  Issue* FindOpenIssue(std::string_view key);
  static std::string ToolIssueKey(std::string_view kind, int tool_call_id);
  void MarkIssueChanged(const Issue& issue);
  ToolCall* FindToolCall(std::string_view call_id);
  // Null if the call is new and kMaxToolCalls is reached.
  ToolCall* FindOrCreateToolCall(std::string_view call_id,
                                 std::string_view name);
  void MarkToolCallChanged(const ToolCall& tool_call);
  ToolCall* RunningToolCall();
  Evidence ToolEvidence(const ToolCall& tool_call) const;
  // Whether audio arrived after the last playback and wasn't dropped by an
  // interruption.
  bool HasUnplayedAudio() const;
  void MaybeStartDetectorTimer();
  void RunDetectors();
  void AppendProblemsToValue(base::DictValue& dict) const;
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
  int speech_on_run_ = 0;
  int speech_off_run_ = 0;
  bool speaking_ = false;
  double speech_start_ms_ = 0;

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
  std::optional<double> last_speech_end_ms_;

  // Output.
  int64_t audio_chunks_received_ = 0;
  int64_t audio_chunks_played_ = 0;
  std::optional<double> last_audio_received_ms_;
  std::optional<double> last_playback_ms_;
  // Audio queued before an interruption is dropped rather than played.
  std::optional<double> last_interrupted_ms_;
  std::optional<double> last_render_error_ms_;

  std::string last_error_;
  bool no_response_suspected_ = false;
  std::string no_response_detail_;
  // The turn closed by the watchdog, or 0 if there was none.
  int no_response_turn_ = 0;
  base::OneShotTimer watchdog_;

  std::vector<ToolCall> tool_calls_;
  std::vector<Issue> issues_;
  // Tool call awaiting a follow-up from the model after its response.
  std::optional<int> awaiting_follow_up_;
  base::RepeatingTimer detector_timer_;

  std::vector<TimelineEvent> events_;
  size_t frequent_events_ = 0;
  size_t event_label_bytes_ = 0;
  // Whether events were dropped because of the caps, frequent ones and the
  // others (the ones ToValue() exports) separately.
  bool frequent_events_truncated_ = false;
  bool events_truncated_ = false;
  std::vector<Turn> turns_;

  // Items dropped because of the caps.
  int64_t dropped_items_ = 0;
  bool items_truncated_reported_ = false;
  // Whether a turn is being dropped, so that it is only counted once.
  bool dropping_turn_ = false;
  // The last dropped tool call, so that it is only counted once.
  std::string last_dropped_call_id_;
  // Keys of dropped issues, so that each is only counted once.
  std::set<std::string, std::less<>> dropped_issue_keys_;

  TtcSessionAudioRecorder recorder_;
  bool cap_reported_ = false;

  // Pending observer updates.
  size_t first_unflushed_event_ = 0;
  std::set<int> changed_turns_;
  std::set<int> changed_tool_calls_;
  std::set<int> changed_issues_;
  base::RepeatingTimer flush_timer_;
  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<TtcDebugSession> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_DEBUG_TTC_DEBUG_SESSION_H_
