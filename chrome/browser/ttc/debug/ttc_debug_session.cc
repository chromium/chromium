// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/debug/ttc_debug_session.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <variant>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/tick_clock.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/ttc/app/public/session_journal.h"

namespace ttc {

namespace {

using Stream = TtcSessionAudioRecorder::Stream;

// U+2026, marks truncated text.
constexpr std::string_view kEllipsis = "\xE2\x80\xA6";

float Rms(base::span<const int16_t> pcm) {
  if (pcm.empty()) {
    return 0;
  }
  double sum = 0;
  for (int16_t sample : pcm) {
    const double v = sample / 32768.0;
    sum += v * v;
  }
  return static_cast<float>(std::sqrt(sum / pcm.size()));
}

bool Within(const std::optional<double>& t,
            double now,
            base::TimeDelta window) {
  return t && now - *t <= window.InMillisecondsF();
}

std::string FormatAge(const std::optional<double>& t, double now) {
  if (!t) {
    return "never";
  }
  return base::StringPrintf("%.1f s ago", (now - *t) / 1000.0);
}

}  // namespace

TtcDebugSession::Snapshot::Snapshot() = default;
TtcDebugSession::Snapshot::Snapshot(const Snapshot&) = default;
TtcDebugSession::Snapshot& TtcDebugSession::Snapshot::operator=(
    const Snapshot&) = default;
TtcDebugSession::Snapshot::~Snapshot() = default;

TtcDebugSession::Update::Update() = default;
TtcDebugSession::Update::Update(const Update&) = default;
TtcDebugSession::Update& TtcDebugSession::Update::operator=(const Update&) =
    default;
TtcDebugSession::Update::~Update() = default;

TtcDebugSession::TtcDebugSession(SessionJournal* journal,
                                 size_t audio_cap_bytes,
                                 const base::TickClock* clock)
    : journal_(journal),
      clock_(clock),
      start_time_(clock->NowTicks()),
      recorder_(audio_cap_bytes),
      flush_timer_(clock) {
  AddEvent(EventKind::kSessionStarted);
  LogToJournal("TtcDebugSessionStarted", "");
}

TtcDebugSession::~TtcDebugSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::string TtcDebugSession::Truncate(std::string_view text,
                                      size_t max_length) {
  if (text.size() <= max_length) {
    return std::string(text);
  }
  return base::StrCat(
      {base::TruncateUTF8ToByteSize(text, max_length - kEllipsis.size()),
       kEllipsis});
}

// static
void TtcDebugSession::TruncateInPlace(std::string& text, size_t max_length) {
  if (text.size() > max_length) {
    text = Truncate(text, max_length);
  }
}

// static
std::string_view TtcDebugSession::StageName(Stage stage) {
  switch (stage) {
    case Stage::kMic:
      return "Mic";
    case Stage::kEncode:
      return "Encode";
    case Stage::kSend:
      return "Send";
    case Stage::kServer:
      return "Server";
    case Stage::kReceive:
      return "Receive";
    case Stage::kSpeaker:
      return "Speaker";
  }
}

// static
std::string_view TtcDebugSession::HealthName(Health health) {
  switch (health) {
    case Health::kIdle:
      return "idle";
    case Health::kOk:
      return "ok";
    case Health::kWarning:
      return "warning";
    case Health::kError:
      return "error";
  }
}

// static
std::string_view TtcDebugSession::EventKindName(EventKind kind) {
  switch (kind) {
    case EventKind::kMicLevel:
      return "mic_level";
    case EventKind::kChunkSent:
      return "chunk_sent";
    case EventKind::kFrameDropped:
      return "frame_dropped";
    case EventKind::kServerFrame:
      return "server_frame";
    case EventKind::kOutputLevel:
      return "output_level";
    case EventKind::kPlayback:
      return "playback";
    case EventKind::kTranscriptIn:
      return "transcript_in";
    case EventKind::kTranscriptOut:
      return "transcript_out";
    case EventKind::kTurnComplete:
      return "turn_complete";
    case EventKind::kInterrupted:
      return "interrupted";
    case EventKind::kTransport:
      return "transport";
    case EventKind::kCaptureMuted:
      return "capture_muted";
    case EventKind::kError:
      return "error";
    case EventKind::kSessionStarted:
      return "session_started";
    case EventKind::kAudioCapReached:
      return "audio_cap_reached";
    case EventKind::kSessionEnded:
      return "session_ended";
  }
}

// static
bool TtcDebugSession::IsFrequentEvent(EventKind kind) {
  return kind == EventKind::kMicLevel || kind == EventKind::kOutputLevel ||
         kind == EventKind::kChunkSent || kind == EventKind::kPlayback;
}

void TtcDebugSession::End() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  AddEvent(EventKind::kSessionEnded);
  ended_ = true;
  journal_ = nullptr;
  Flush();
}

base::TimeDelta TtcDebugSession::Elapsed() const {
  return clock_->NowTicks() - start_time_;
}

double TtcDebugSession::NowMs() const {
  return Elapsed().InMillisecondsF();
}

void TtcDebugSession::AddEvent(EventKind kind, float value, std::string label) {
  if (ended_) {
    return;
  }
  const bool frequent = IsFrequentEvent(kind);
  const size_t count =
      frequent ? frequent_events_ : events_.size() - frequent_events_;
  // The end of the session is always recorded.
  if (kind != EventKind::kSessionEnded &&
      (count >= (frequent ? kMaxFrequentEvents : kMaxOtherEvents) ||
       event_label_bytes_ >= kMaxEventLabelBytes)) {
    (frequent ? frequent_events_truncated_ : events_truncated_) = true;
    return;
  }
  if (frequent) {
    ++frequent_events_;
  }
  TruncateInPlace(label);
  event_label_bytes_ += label.size();
  events_.push_back({NowMs(), kind, value, std::move(label)});
  MaybeStartFlushTimer();
}

void TtcDebugSession::LogToJournal(std::string_view name, std::string detail) {
  if (!journal_) {
    return;
  }
  actor::JournalDetailsBuilder builder;
  if (!detail.empty()) {
    builder.Add("detail", detail);
  }
  journal_->Log(name, std::move(builder).Build());
}

void TtcDebugSession::OnCaptureFormat(const CaptureFormat& format) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  capture_format_ = format;
  MaybeStartFlushTimer();
}

void TtcDebugSession::OnCaptureMuted(bool muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  capture_muted_ = muted;
  AddEvent(EventKind::kCaptureMuted, muted ? 1.0f : 0.0f);
}

void TtcDebugSession::OnMicLevel(float rms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  const double now = NowMs();
  last_mic_level_ms_ = now;
  last_mic_level_ = rms;
  consecutive_silent_chunks_ =
      rms < kDigitalSilenceLevel ? consecutive_silent_chunks_ + 1 : 0;
  AddEvent(EventKind::kMicLevel, rms);
}

void TtcDebugSession::OnClientFrame(const ClientFrame& frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  // The other frames aren't recorded yet.
  if (const auto* audio = std::get_if<AudioInput>(&frame)) {
    const base::TimeDelta now = Elapsed();
    ++chunks_sent_;
    bytes_sent_ += audio->pcm.size() * sizeof(int16_t);
    last_chunk_sent_ms_ = now.InMillisecondsF();
    AddEvent(EventKind::kChunkSent, static_cast<float>(audio->pcm.size()));
    recorder_.Append(Stream::kInput, now, audio->pcm);
    MaybeReportCapReached();
  } else if (const auto* status = std::get_if<PlaybackStatus>(&frame)) {
    // Sent for each chunk played.
    ++audio_chunks_played_;
    last_playback_ms_ = NowMs();
    AddEvent(EventKind::kPlayback,
             static_cast<float>(status->last_played_sequence_number));
  }
}

void TtcDebugSession::MaybeReportCapReached() {
  if (recorder_.cap_reached() && !cap_reported_) {
    cap_reported_ = true;
    AddEvent(EventKind::kAudioCapReached);
    LogToJournal("TtcDebugAudioCapReached", "");
  }
}

void TtcDebugSession::OnFrameDropped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  ++frames_dropped_;
  last_frame_dropped_ms_ = NowMs();
  AddEvent(EventKind::kFrameDropped);
}

void TtcDebugSession::OnTransportStateChanged(bool connected) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  transport_connected_ = connected;
  ever_connected_ |= connected;
  AddEvent(EventKind::kTransport, connected ? 1.0f : 0.0f,
           connected ? "connected" : "disconnected");
}

void TtcDebugSession::OnServerFrame(const ServerFrame& frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  ++server_frames_;
  last_server_frame_ms_ = NowMs();
  std::visit([this](const auto& typed_frame) { HandleFrame(typed_frame); },
             frame);
}

void TtcDebugSession::AddServerFrameEvent(std::string_view kind,
                                          std::string_view detail) {
  AddEvent(
      EventKind::kServerFrame, 0,
      detail.empty() ? std::string(kind) : base::StrCat({kind, ":", detail}));
}

void TtcDebugSession::HandleFrame(const SessionStatus& status) {
  AddServerFrameEvent("session_status", status.state);
  server_session_id_ = Truncate(status.server_session_id);
}

void TtcDebugSession::HandleFrame(const AudioOutput& audio) {
  ++audio_chunks_received_;
  last_audio_received_ms_ = NowMs();
  AddEvent(EventKind::kOutputLevel, Rms(audio.pcm));
  recorder_.Append(Stream::kOutput, Elapsed(), audio.pcm);
  MaybeReportCapReached();
}

void TtcDebugSession::HandleFrame(const Transcript& transcript) {
  AddServerFrameEvent("transcript", transcript.is_input ? "input" : "output");
  AddEvent(transcript.is_input ? EventKind::kTranscriptIn
                               : EventKind::kTranscriptOut,
           0, transcript.text);
}

void TtcDebugSession::HandleFrame(const TurnComplete& turn_complete) {
  AddServerFrameEvent("turn_complete");
  AddEvent(EventKind::kTurnComplete);
}

void TtcDebugSession::HandleFrame(const Interrupted& interrupted) {
  AddServerFrameEvent("interrupted");
  last_interrupted_ms_ = NowMs();
  AddEvent(EventKind::kInterrupted);
  // Playback stops now, so the rest of the reply was never heard.
  recorder_.DiscardAfter(Stream::kOutput, Elapsed());
}

void TtcDebugSession::HandleFrame(const DebugSink::ToolCall& frame) {
  AddServerFrameEvent("tool_call", frame.name);
}

void TtcDebugSession::HandleFrame(const ServerError& error) {
  AddServerFrameEvent("server_error", error.code);
  last_server_error_ms_ = NowMs();
  last_error_ = Truncate(error.message.empty()
                             ? error.code
                             : base::StrCat({error.code, ": ", error.message}));
}

void TtcDebugSession::HandleFrame(const GoAway& go_away) {
  AddServerFrameEvent("go_away");
}

void TtcDebugSession::HandleFrame(const ServerJournalEvent& event) {
  AddServerFrameEvent("journal", event.name);
}

void TtcDebugSession::HandleFrame(const ModelTurnState& state) {
  AddServerFrameEvent("model_turn", state.state);
}

void TtcDebugSession::HandleFrame(const UserTurnState& state) {
  AddServerFrameEvent("user_turn", state.state);
}

void TtcDebugSession::HandleFrame(const UnknownFrame& frame) {
  AddServerFrameEvent("other");
}

void TtcDebugSession::OnRenderError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  last_render_error_ms_ = NowMs();
  last_error_ = "audio render error";
  AddEvent(EventKind::kError, 0, last_error_);
}

void TtcDebugSession::OnError(std::string_view error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ended_) {
    return;
  }
  last_error_ = Truncate(error);
  AddEvent(EventKind::kError, 0, last_error_);
}

TtcDebugSession::Snapshot TtcDebugSession::GetSnapshot() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Snapshot snapshot;
  snapshot.now_ms = NowMs();
  snapshot.ended = ended_;
  snapshot.transport_connected = transport_connected_;
  snapshot.server_session_id = server_session_id_;
  snapshot.chunks_sent = chunks_sent_;
  snapshot.bytes_sent = bytes_sent_;
  snapshot.frames_dropped = frames_dropped_;
  snapshot.server_frames = server_frames_;
  snapshot.audio_chunks_received = audio_chunks_received_;
  snapshot.audio_chunks_played = audio_chunks_played_;
  snapshot.last_error = last_error_;
  snapshot.audio_bytes = recorder_.bytes();
  snapshot.audio_cap_bytes = recorder_.cap_bytes();
  snapshot.audio_cap_reached = recorder_.cap_reached();
  ComputeStages(snapshot);
  return snapshot;
}

void TtcDebugSession::ComputeStages(Snapshot& snapshot) const {
  const double now = snapshot.now_ms;
  auto& stages = snapshot.stages;
  auto set = [&](Stage stage, Health health, std::string detail) {
    stages[static_cast<size_t>(stage)] = {health, std::move(detail)};
  };
  const bool live = !ended_;
  const bool mic_active = Within(last_mic_level_ms_, now, kActivityWindow);

  // Mic.
  if (!capture_format_ && !last_mic_level_ms_) {
    set(Stage::kMic, Health::kIdle, "not capturing");
  } else {
    std::string detail =
        capture_format_
            ? base::StringPrintf(
                  "%d Hz x%d, %s", capture_format_->sample_rate,
                  capture_format_->channels,
                  capture_format_->processing ? "processed" : "converted")
            : std::string("format unknown");
    base::StrAppend(&detail,
                    {base::StringPrintf(", level %.3f", last_mic_level_)});
    Health health = Health::kOk;
    if (capture_muted_) {
      health = Health::kWarning;
      base::StrAppend(&detail, {", muted"});
    } else if (live && !mic_active) {
      // No audio at all is worse than silent audio.
      health = Health::kError;
      base::StrAppend(&detail,
                      {", last audio ", FormatAge(last_mic_level_ms_, now)});
    } else if (consecutive_silent_chunks_ >= kMicSilenceIssueChunks) {
      health = Health::kWarning;
      base::StrAppend(&detail, {", digital silence"});
    }
    set(Stage::kMic, health, std::move(detail));
  }

  // Encode.
  set(Stage::kEncode, mic_active ? Health::kOk : Health::kIdle,
      base::StringPrintf("PCM16 %d kHz mono",
                         TtcSessionAudioRecorder::kInputSampleRate / 1000));

  // Send.
  {
    std::string detail = base::StringPrintf(
        "%lld chunks, %lld KB", static_cast<long long>(chunks_sent_),
        static_cast<long long>(bytes_sent_ / 1024));
    Health health = Health::kIdle;
    if (Within(last_frame_dropped_ms_, now, kFrameDropWindow)) {
      health = Health::kError;
      base::StrAppend(&detail, {base::StringPrintf(
                                   ", %lld dropped (no session)",
                                   static_cast<long long>(frames_dropped_))});
    } else if (Within(last_chunk_sent_ms_, now, kActivityWindow)) {
      health = Health::kOk;
    } else if (live && mic_active) {
      health = Health::kWarning;
      base::StrAppend(&detail, {", mic active but nothing sent"});
    }
    set(Stage::kSend, health, std::move(detail));
  }

  // Server.
  {
    std::string detail =
        base::StrCat({transport_connected_ ? "connected" : "disconnected",
                      ", last frame ", FormatAge(last_server_frame_ms_, now)});
    Health health = Health::kIdle;
    if (Within(last_server_error_ms_, now, kErrorWindow)) {
      health = Health::kError;
      base::StrAppend(&detail, {", ", last_error_});
    } else if (live && ever_connected_ && !transport_connected_) {
      health = Health::kError;
    } else if (transport_connected_) {
      health = Health::kOk;
    }
    set(Stage::kServer, health, std::move(detail));
  }

  // Receive.
  {
    std::string detail =
        base::StringPrintf("%lld audio chunks, last %s",
                           static_cast<long long>(audio_chunks_received_),
                           FormatAge(last_audio_received_ms_, now).c_str());
    set(Stage::kReceive,
        Within(last_audio_received_ms_, now, kReceiveWindow) ? Health::kOk
                                                             : Health::kIdle,
        std::move(detail));
  }

  // Speaker.
  {
    std::string detail = base::StringPrintf(
        "%lld chunks played", static_cast<long long>(audio_chunks_played_));
    Health health = Health::kIdle;
    if (Within(last_render_error_ms_, now, kErrorWindow)) {
      health = Health::kError;
      base::StrAppend(&detail, {", render error"});
    } else if (Within(last_playback_ms_, now, kActivityWindow)) {
      health = Health::kOk;
    } else if (live && HasUnplayedAudio() &&
               now - *last_audio_received_ms_ >
                   kPlaybackStallTimeout.InMillisecondsF()) {
      health = Health::kWarning;
      base::StrAppend(&detail, {", audio received but not played"});
    }
    set(Stage::kSpeaker, health, std::move(detail));
  }
}

bool TtcDebugSession::HasUnplayedAudio() const {
  if (!last_audio_received_ms_) {
    return false;
  }
  const double received = *last_audio_received_ms_;
  return (!last_playback_ms_ || *last_playback_ms_ < received) &&
         (!last_interrupted_ms_ || *last_interrupted_ms_ < received);
}

base::DictValue TtcDebugSession::ToValue() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Snapshot snapshot = GetSnapshot();
  base::DictValue dict;
  base::DictValue snap;
  snap.Set("now_ms", snapshot.now_ms);
  snap.Set("ended", snapshot.ended);
  snap.Set("transport_connected", snapshot.transport_connected);
  snap.Set("server_session_id", snapshot.server_session_id);
  snap.Set("chunks_sent", static_cast<double>(snapshot.chunks_sent));
  snap.Set("bytes_sent", static_cast<double>(snapshot.bytes_sent));
  snap.Set("frames_dropped", static_cast<double>(snapshot.frames_dropped));
  snap.Set("server_frames", static_cast<double>(snapshot.server_frames));
  snap.Set("audio_chunks_received",
           static_cast<double>(snapshot.audio_chunks_received));
  snap.Set("audio_chunks_played",
           static_cast<double>(snapshot.audio_chunks_played));
  snap.Set("last_error", snapshot.last_error);
  snap.Set("audio_bytes", static_cast<double>(snapshot.audio_bytes));
  snap.Set("audio_cap_bytes", static_cast<double>(snapshot.audio_cap_bytes));
  snap.Set("audio_cap_reached", snapshot.audio_cap_reached);
  base::ListValue stages;
  for (size_t i = 0; i < kStageCount; ++i) {
    stages.Append(base::DictValue()
                      .Set("stage", StageName(static_cast<Stage>(i)))
                      .Set("health", HealthName(snapshot.stages[i].health))
                      .Set("detail", snapshot.stages[i].detail));
  }
  snap.Set("stages", std::move(stages));
  dict.Set("snapshot", std::move(snap));

  base::ListValue events;
  for (const TimelineEvent& event : events_) {
    if (IsFrequentEvent(event.kind)) {
      continue;  // Too verbose for a bug attachment.
    }
    events.Append(base::DictValue()
                      .Set("t_ms", event.t_ms)
                      .Set("kind", EventKindName(event.kind))
                      .Set("label", event.label));
  }
  dict.Set("events", std::move(events));
  dict.Set("events_truncated", events_truncated_);
  dict.Set("frequent_events_truncated", frequent_events_truncated_);
  return dict;
}

void TtcDebugSession::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (observers_.empty()) {
    // Observers fetch the current state first (see Observer); only send what
    // happens from now on.
    first_unflushed_event_ = events_.size();
  } else {
    // Sends the others what they haven't had yet, so that the new observer's
    // first update doesn't repeat what it fetched with the state.
    Flush();
  }
  observers_.AddObserver(observer);
  MaybeStartFlushTimer();
}

void TtcDebugSession::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
  if (observers_.empty()) {
    flush_timer_.Stop();
  }
}

void TtcDebugSession::MaybeStartFlushTimer() {
  if (observers_.empty() || flush_timer_.IsRunning()) {
    return;
  }
  flush_timer_.Start(
      FROM_HERE, kFlushInterval,
      base::BindRepeating(&TtcDebugSession::Flush, base::Unretained(this)));
}

void TtcDebugSession::Flush() {
  if (observers_.empty()) {
    first_unflushed_event_ = events_.size();
    return;
  }
  Update update;
  update.first_event_index = std::min(first_unflushed_event_, events_.size());
  update.new_events.assign(events_.begin() + update.first_event_index,
                           events_.end());
  first_unflushed_event_ = events_.size();

  update.snapshot = GetSnapshot();
  for (Observer& observer : observers_) {
    observer.OnDebugUpdate(update);
  }
  // Keep ticking while live so that ages and stage health stay current.
  if (ended_) {
    flush_timer_.Stop();
  }
}

}  // namespace ttc
