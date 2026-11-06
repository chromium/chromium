// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/debug/ttc_debug_session.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/ttc/app/public/debug_sink.h"
#include "components/ttc/app/public/server_journal_event.h"
#include "components/ttc/app/public/session_journal.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {
namespace {

using EventKind = TtcDebugSession::EventKind;
using Health = TtcDebugSession::Health;
using Stage = TtcDebugSession::Stage;
using TurnStatus = TtcDebugSession::TurnStatus;
using testing::ElementsAre;

// Marks truncated text.
constexpr std::string_view kEllipsis = "\xE2\x80\xA6";

class RecordingJournal : public SessionJournal {
 public:
  void Log(std::string_view event_name,
           std::vector<actor::mojom::JournalDetailsPtr> details,
           Track track) override {
    names.emplace_back(event_name);
  }
  void HandleServerJournalEvent(const ServerJournalEvent& event) override {}
  void LogAudio(std::string_view mime_type,
                base::span<const uint8_t> data) override {}

  std::vector<std::string> names;
};

class RecordingObserver : public TtcDebugSession::Observer {
 public:
  void OnDebugUpdate(const TtcDebugSession::Update& update) override {
    ++updates;
    last_snapshot = update.snapshot;
    first_event_indices.push_back(update.first_event_index);
    events.insert(events.end(), update.new_events.begin(),
                  update.new_events.end());
    turns.insert(turns.end(), update.changed_turns.begin(),
                 update.changed_turns.end());
  }

  int updates = 0;
  TtcDebugSession::Snapshot last_snapshot;
  std::vector<size_t> first_event_indices;
  std::vector<TtcDebugSession::TimelineEvent> events;
  std::vector<TtcDebugSession::Turn> turns;
};

class TtcDebugSessionTest : public testing::Test {
 protected:
  void SetUp() override {
    session_ = std::make_unique<TtcDebugSession>(
        &journal_, 1 << 20, task_environment_.GetMockTickClock());
  }

  // Feeds `count` 100 ms mic chunks at `level`, as the pipeline would: the
  // level, then the audio sent to the server.
  void Speak(int count, float level) {
    const std::vector<int16_t> pcm(1600, level > 0.01f ? 2000 : 0);
    for (int i = 0; i < count; ++i) {
      task_environment_.FastForwardBy(base::Milliseconds(100));
      session_->OnMicLevel(level);
      session_->OnClientFrame(DebugSink::AudioInput{.pcm = pcm});
    }
  }

  // Receives `chunks` 100 ms chunks of reply audio at once.
  void ReceiveAudio(int chunks) {
    const std::vector<int16_t> pcm(2400, 1000);
    for (int i = 0; i < chunks; ++i) {
      session_->OnServerFrame(
          DebugSink::AudioOutput{.sequence_number = i, .pcm = pcm});
    }
  }

  bool HasEvent(EventKind kind) const { return CountEvents(kind) > 0; }

  int CountEvents(EventKind kind) const {
    int count = 0;
    for (const auto& event : session_->events()) {
      count += event.kind == kind;
    }
    return count;
  }

  // Labels of the events of `kind`.
  std::vector<std::string> EventLabels(EventKind kind) const {
    std::vector<std::string> labels;
    for (const auto& event : session_->events()) {
      if (event.kind == kind) {
        labels.push_back(event.label);
      }
    }
    return labels;
  }

  Health StageHealth(Stage stage) const {
    return session_->GetSnapshot().stages[static_cast<size_t>(stage)].health;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  RecordingJournal journal_;
  std::unique_ptr<TtcDebugSession> session_;
};

TEST_F(TtcDebugSessionTest, DetectsSpeechAndOpensTurn) {
  session_->OnTransportStateChanged(true);
  Speak(5, 0.1f);
  EXPECT_TRUE(session_->GetSnapshot().speaking);
  EXPECT_TRUE(HasEvent(EventKind::kSpeechStart));
  Speak(10, 0.0f);
  EXPECT_FALSE(session_->GetSnapshot().speaking);
  EXPECT_TRUE(HasEvent(EventKind::kSpeechEnd));
  ASSERT_EQ(session_->turns().size(), 1u);
  EXPECT_EQ(session_->turns()[0].status, TurnStatus::kOpen);
  EXPECT_TRUE(session_->turns()[0].speech_start_ms.has_value());
}

TEST_F(TtcDebugSessionTest, SuccessfulTurnRecordsLatencies) {
  session_->OnTransportStateChanged(true);
  Speak(5, 0.1f);
  Speak(10, 0.0f);
  task_environment_.FastForwardBy(base::Milliseconds(300));
  session_->OnServerFrame(
      DebugSink::Transcript{.is_input = true, .text = "open my calendar"});
  task_environment_.FastForwardBy(base::Milliseconds(400));
  ReceiveAudio(3);
  task_environment_.FastForwardBy(base::Milliseconds(100));
  session_->OnClientFrame(
      DebugSink::PlaybackStatus{.last_played_sequence_number = 0});
  session_->OnServerFrame(DebugSink::TurnComplete());

  ASSERT_EQ(session_->turns().size(), 1u);
  const TtcDebugSession::Turn& turn = session_->turns()[0];
  EXPECT_EQ(turn.status, TurnStatus::kOk);
  EXPECT_EQ(turn.input_transcript, "open my calendar");
  ASSERT_TRUE(turn.first_transcript_ms && turn.first_audio_ms &&
              turn.first_playback_ms);
  EXPECT_LT(turn.speech_end_ms, *turn.first_transcript_ms);
  EXPECT_LT(*turn.first_transcript_ms, *turn.first_audio_ms);
  EXPECT_LT(*turn.first_audio_ms, *turn.first_playback_ms);
  ASSERT_TRUE(turn.audio_out_start_ms && turn.audio_out_end_ms);
  // Three 100 ms chunks of output audio.
  EXPECT_NEAR(*turn.audio_out_end_ms - *turn.audio_out_start_ms, 300, 1);
  EXPECT_FALSE(session_->GetSnapshot().no_response_suspected);
}

TEST_F(TtcDebugSessionTest, WatchdogFlagsMissingResponse) {
  session_->OnTransportStateChanged(true);
  Speak(5, 0.1f);
  Speak(10, 0.0f);
  task_environment_.FastForwardBy(TtcDebugSession::kNoResponseTimeout);

  TtcDebugSession::Snapshot snapshot = session_->GetSnapshot();
  EXPECT_TRUE(snapshot.no_response_suspected);
  EXPECT_THAT(snapshot.no_response_detail,
              testing::HasSubstr("Chunks sent: 15"));
  EXPECT_TRUE(HasEvent(EventKind::kNoResponseSuspected));
  EXPECT_EQ(session_->turns()[0].status, TurnStatus::kNoResponse);
  EXPECT_EQ(StageHealth(Stage::kServer), Health::kWarning);
  EXPECT_THAT(journal_.names, testing::Contains("TtcNoResponseSuspected"));
}

TEST_F(TtcDebugSessionTest, ServerActivityCancelsWatchdog) {
  session_->OnTransportStateChanged(true);
  Speak(5, 0.1f);
  Speak(10, 0.0f);
  task_environment_.FastForwardBy(base::Seconds(1));
  session_->OnServerFrame(
      DebugSink::Transcript{.is_input = true, .text = "hi"});
  task_environment_.FastForwardBy(TtcDebugSession::kNoResponseTimeout);
  EXPECT_FALSE(session_->GetSnapshot().no_response_suspected);
}

TEST_F(TtcDebugSessionTest, TurnCompleteWithoutAudio) {
  Speak(5, 0.1f);
  Speak(10, 0.0f);
  session_->OnServerFrame(
      DebugSink::Transcript{.is_input = true, .text = "hi"});
  session_->OnServerFrame(DebugSink::TurnComplete());
  EXPECT_EQ(session_->turns()[0].status, TurnStatus::kNoAudio);
}

TEST_F(TtcDebugSessionTest, InterruptionClosesTurn) {
  Speak(5, 0.1f);
  Speak(10, 0.0f);
  ReceiveAudio(1);
  session_->OnServerFrame(DebugSink::Interrupted());
  EXPECT_EQ(session_->turns()[0].status, TurnStatus::kInterrupted);
}

// The turn's reply only covers what played before the interruption.
TEST_F(TtcDebugSessionTest, InterruptionClipsTheTurnsReply) {
  Speak(5, 0.1f);
  Speak(10, 0.0f);
  // 300 ms of reply audio, placed from 100 ms before it arrived.
  ReceiveAudio(3);
  const double arrived = session_->GetSnapshot().now_ms;
  task_environment_.FastForwardBy(base::Milliseconds(100));
  session_->OnServerFrame(DebugSink::Interrupted());

  const TtcDebugSession::Turn& turn = session_->turns()[0];
  ASSERT_TRUE(turn.audio_out_start_ms && turn.audio_out_end_ms);
  EXPECT_NEAR(*turn.audio_out_start_ms, arrived - 100, 1);
  EXPECT_NEAR(*turn.audio_out_end_ms, arrived + 100, 1);
}

TEST_F(TtcDebugSessionTest, StageHealth) {
  // Nothing yet.
  EXPECT_EQ(StageHealth(Stage::kMic), Health::kIdle);
  EXPECT_EQ(StageHealth(Stage::kServer), Health::kIdle);

  session_->OnCaptureFormat({.sample_rate = 48000, .channels = 2});
  session_->OnTransportStateChanged(true);
  Speak(3, 0.1f);
  EXPECT_EQ(StageHealth(Stage::kMic), Health::kOk);
  EXPECT_EQ(StageHealth(Stage::kSend), Health::kOk);
  EXPECT_EQ(StageHealth(Stage::kServer), Health::kOk);

  // Digital silence for 3 s.
  Speak(30, 0.0f);
  EXPECT_EQ(StageHealth(Stage::kMic), Health::kWarning);

  // Dropped frames.
  session_->OnFrameDropped();
  EXPECT_EQ(StageHealth(Stage::kSend), Health::kError);

  // Transport lost.
  session_->OnTransportStateChanged(false);
  EXPECT_EQ(StageHealth(Stage::kServer), Health::kError);

  // Mic stalls.
  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_EQ(StageHealth(Stage::kMic), Health::kError);

  // Received audio that never plays.
  ReceiveAudio(1);
  EXPECT_EQ(StageHealth(Stage::kReceive), Health::kOk);
  task_environment_.FastForwardBy(base::Seconds(3));
  EXPECT_EQ(StageHealth(Stage::kSpeaker), Health::kWarning);
  session_->OnRenderError();
  EXPECT_EQ(StageHealth(Stage::kSpeaker), Health::kError);
}

TEST_F(TtcDebugSessionTest, StageDetailsDescribeTheCapture) {
  session_->OnCaptureFormat({.sample_rate = 48000,
                             .channels = 1,
                             .processing = DebugSink::CaptureFormat::Processing{
                                 .echo_cancellation = true}});
  Speak(1, 0.1f);
  EXPECT_THAT(
      session_->GetSnapshot().stages[static_cast<size_t>(Stage::kMic)].detail,
      testing::StartsWith("48000 Hz x1, processed"));

  // Playback reported by the client counts for the speaker.
  ReceiveAudio(1);
  session_->OnClientFrame(
      DebugSink::PlaybackStatus{.last_played_sequence_number = 0});
  EXPECT_EQ(StageHealth(Stage::kSpeaker), Health::kOk);
}

TEST_F(TtcDebugSessionTest, RecordsExactAudio) {
  using Stream = TtcSessionAudioRecorder::Stream;
  const std::vector<int16_t> pcm = {1, -2, 3, -4};
  task_environment_.FastForwardBy(base::Seconds(1));
  session_->OnClientFrame(DebugSink::AudioInput{.pcm = pcm});
  const std::vector<TtcSessionAudioRecorder::Placement> ranges =
      session_->recorder().SegmentRanges(Stream::kInput);
  ASSERT_EQ(ranges.size(), 1u);
  EXPECT_EQ(session_->recorder().GetRange(Stream::kInput, ranges[0].start,
                                          ranges[0].end),
            pcm);
}

// Playback stops when the user interrupts, so the rest of the reply isn't
// kept.
TEST_F(TtcDebugSessionTest, InterruptionDiscardsUnplayedReplyAudio) {
  using Stream = TtcSessionAudioRecorder::Stream;
  // 300 ms of reply audio, placed at [0, 300 ms).
  ReceiveAudio(3);
  task_environment_.FastForwardBy(base::Milliseconds(100));
  session_->OnServerFrame(DebugSink::Interrupted());

  const std::vector<TtcSessionAudioRecorder::Placement> ranges =
      session_->recorder().SegmentRanges(Stream::kOutput);
  ASSERT_EQ(ranges.size(), 1u);
  EXPECT_EQ(ranges[0].start, base::TimeDelta());
  EXPECT_EQ(ranges[0].end, base::Milliseconds(100));
  // All three chunks were still received.
  EXPECT_EQ(session_->GetSnapshot().audio_chunks_received, 3);
}

TEST_F(TtcDebugSessionTest, CountsClientFrames) {
  Speak(2, 0.1f);
  session_->OnClientFrame(
      DebugSink::PlaybackStatus{.last_played_sequence_number = 4});
  // Not recorded yet.
  session_->OnClientFrame(DebugSink::TextInput{.text = "hi"});

  const TtcDebugSession::Snapshot snapshot = session_->GetSnapshot();
  EXPECT_EQ(snapshot.chunks_sent, 2);
  EXPECT_EQ(snapshot.bytes_sent, 2 * 1600 * 2);
  EXPECT_EQ(snapshot.audio_chunks_played, 1);
}

TEST_F(TtcDebugSessionTest, ServerErrorsAreTheLastError) {
  session_->OnServerFrame(
      DebugSink::ServerError{.code = "INTERNAL", .message = "boom"});
  EXPECT_EQ(session_->GetSnapshot().last_error, "INTERNAL: boom");
  // Responses that couldn't be used are reported the same way.
  session_->OnServerFrame(
      DebugSink::ServerError{.code = "UnparsableServerFrame"});
  EXPECT_EQ(session_->GetSnapshot().last_error, "UnparsableServerFrame");
}

TEST_F(TtcDebugSessionTest, ObserversGetBatchedUpdates) {
  RecordingObserver observer;
  session_->AddObserver(&observer);
  session_->OnTransportStateChanged(true);
  session_->OnServerFrame(
      DebugSink::SessionStatus{.server_session_id = "srv-1"});
  EXPECT_EQ(observer.updates, 0);
  task_environment_.FastForwardBy(TtcDebugSession::kFlushInterval);
  EXPECT_EQ(observer.updates, 1);
  EXPECT_EQ(observer.last_snapshot.server_session_id, "srv-1");
  EXPECT_FALSE(observer.events.empty());
  // The observer started after the session_started event.
  EXPECT_EQ(observer.first_event_indices[0], 1u);

  // Live sessions keep sending snapshots so that ages stay current.
  task_environment_.FastForwardBy(TtcDebugSession::kFlushInterval * 3);
  EXPECT_EQ(observer.updates, 4);
  session_->RemoveObserver(&observer);
}

TEST_F(TtcDebugSessionTest, EndClosesOpenTurnAndDetachesJournal) {
  Speak(5, 0.1f);
  Speak(10, 0.0f);
  ReceiveAudio(1);
  session_->End();
  EXPECT_TRUE(session_->ended());
  EXPECT_EQ(session_->turns()[0].status, TurnStatus::kOk);
  EXPECT_TRUE(HasEvent(EventKind::kSessionEnded));
  // The watchdog never fires after the end, and nothing reaches the journal.
  const size_t journal_entries = journal_.names.size();
  task_environment_.FastForwardBy(base::Seconds(30));
  EXPECT_EQ(journal_.names.size(), journal_entries);
}

TEST_F(TtcDebugSessionTest, ExportOmitsVerboseEvents) {
  Speak(5, 0.1f);
  session_->OnServerFrame(
      DebugSink::Transcript{.is_input = true, .text = "hello"});
  ReceiveAudio(1);
  session_->OnClientFrame(
      DebugSink::PlaybackStatus{.last_played_sequence_number = 0});
  base::DictValue value = session_->ToValue();
  const base::DictValue* snapshot = value.FindDict("snapshot");
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(snapshot->FindDouble("bytes_sent"), 5 * 1600 * 2);
  EXPECT_TRUE(snapshot->FindDouble("audio_bytes"));
  EXPECT_TRUE(snapshot->FindDouble("dropped_items"));
  const base::ListValue* events = value.FindList("events");
  ASSERT_TRUE(events);
  for (const base::Value& event : *events) {
    EXPECT_NE(*event.GetDict().FindString("kind"), "mic_level");
    EXPECT_NE(*event.GetDict().FindString("kind"), "chunk_sent");
    EXPECT_NE(*event.GetDict().FindString("kind"), "playback");
  }
  ASSERT_TRUE(value.FindList("turns"));
  EXPECT_EQ(value.FindList("turns")->size(), 1u);
}

TEST_F(TtcDebugSessionTest, CapReachedIsReportedOnce) {
  auto session = std::make_unique<TtcDebugSession>(
      &journal_, 1000, task_environment_.GetMockTickClock());
  const std::vector<int16_t> pcm(400, 1);
  session->OnClientFrame(DebugSink::AudioInput{.pcm = pcm});
  session->OnClientFrame(DebugSink::AudioInput{.pcm = pcm});
  session->OnClientFrame(DebugSink::AudioInput{.pcm = pcm});
  int cap_events = 0;
  for (const auto& event : session->events()) {
    cap_events += event.kind == EventKind::kAudioCapReached;
  }
  EXPECT_EQ(cap_events, 1);
  EXPECT_TRUE(session->GetSnapshot().audio_cap_reached);
}

TEST_F(TtcDebugSessionTest, AudioDroppedByInterruptionIsNotFlagged) {
  session_->OnTransportStateChanged(true);
  ReceiveAudio(2);
  session_->OnServerFrame(DebugSink::Interrupted());
  task_environment_.FastForwardBy(TtcDebugSession::kPlaybackStallTimeout * 2);
  EXPECT_NE(StageHealth(Stage::kSpeaker), Health::kWarning);
}

TEST_F(TtcDebugSessionTest, EndedSessionIgnoresFurtherInput) {
  session_->End();
  const size_t events = session_->events().size();
  Speak(5, 0.1f);
  ReceiveAudio(2);
  session_->OnCaptureMuted(true);
  task_environment_.FastForwardBy(base::Seconds(5));
  EXPECT_EQ(session_->events().size(), events);
  EXPECT_EQ(session_->recorder().bytes(), 0u);
}

TEST_F(TtcDebugSessionTest, ServerFramesAreLabelledByContent) {
  ServerJournalEvent journal_event;
  journal_event.name = "ModelStart";
  journal_event.details.push_back({.key = "k", .value = "v"});

  session_->OnServerFrame(DebugSink::SessionStatus{.state = "STATE_CONNECTED"});
  session_->OnServerFrame(
      DebugSink::Transcript{.is_input = true, .text = "hi"});
  session_->OnServerFrame(
      DebugSink::Transcript{.is_input = false, .text = "hello"});
  session_->OnServerFrame(DebugSink::ToolCall{
      .call_id = "c1", .name = "navigate", .arguments_json = "{}"});
  session_->OnServerFrame(
      DebugSink::ServerError{.code = "INTERNAL", .message = "boom"});
  session_->OnServerFrame(journal_event);
  session_->OnServerFrame(DebugSink::ModelTurnState{.state = "GENERATING"});
  session_->OnServerFrame(DebugSink::UserTurnState{.state = "SPEAKING"});
  session_->OnServerFrame(DebugSink::TurnComplete());
  session_->OnServerFrame(DebugSink::UnknownFrame());

  EXPECT_THAT(EventLabels(EventKind::kServerFrame),
              ElementsAre("session_status:STATE_CONNECTED", "transcript:input",
                          "transcript:output", "tool_call:navigate",
                          "server_error:INTERNAL", "journal:ModelStart",
                          "model_turn:GENERATING", "user_turn:SPEAKING",
                          "turn_complete", "other"));
  EXPECT_EQ(session_->GetSnapshot().server_frames, 10);
}

TEST_F(TtcDebugSessionTest, EventsAreCapped) {
  EXPECT_THAT(session_->ToValue().FindBool("events_truncated"),
              testing::Optional(false));
  // The session_started event counts too.
  while (session_->events().size() < TtcDebugSession::kMaxOtherEvents) {
    session_->OnCaptureMuted(false);
  }
  session_->OnCaptureMuted(true);
  EXPECT_EQ(session_->events().size(), TtcDebugSession::kMaxOtherEvents);
  EXPECT_THAT(session_->ToValue().FindBool("events_truncated"),
              testing::Optional(true));

  // The end of the session is still recorded.
  session_->End();
  EXPECT_EQ(session_->events().back().kind, EventKind::kSessionEnded);
}

// Frequent events have a cap of their own, so that a long session keeps the
// others, which are the ones exported.
TEST_F(TtcDebugSessionTest, FrequentEventsDontCrowdOutOthers) {
  // Below the speech threshold, so that no speech_start event is added.
  for (size_t i = 0; i <= TtcDebugSession::kMaxFrequentEvents; ++i) {
    session_->OnMicLevel(TtcDebugSession::kSpeechOnLevel / 2);
  }
  // Plus session_started.
  EXPECT_EQ(session_->events().size(), TtcDebugSession::kMaxFrequentEvents + 1);

  session_->OnError("still recorded");
  EXPECT_EQ(session_->events().back().kind, EventKind::kError);
  base::DictValue value = session_->ToValue();
  EXPECT_THAT(value.FindBool("frequent_events_truncated"),
              testing::Optional(true));
  EXPECT_THAT(value.FindBool("events_truncated"), testing::Optional(false));
}

TEST_F(TtcDebugSessionTest, LateObserverOnlyGetsNewChanges) {
  session_->OnTransportStateChanged(true);
  task_environment_.FastForwardBy(base::Seconds(1));
  const size_t backlog = session_->events().size();

  // The page fetches the state when it opens, so the backlog isn't resent.
  RecordingObserver observer;
  session_->AddObserver(&observer);
  task_environment_.FastForwardBy(TtcDebugSession::kFlushInterval);
  ASSERT_EQ(observer.updates, 1);
  EXPECT_EQ(observer.first_event_indices[0], backlog);
  EXPECT_TRUE(observer.events.empty());
  EXPECT_TRUE(observer.turns.empty());
  session_->RemoveObserver(&observer);
}

// A second page doesn't get again what it fetched when it opened.
TEST_F(TtcDebugSessionTest, SecondObserverOnlyGetsNewChanges) {
  RecordingObserver first;
  session_->AddObserver(&first);
  session_->OnTransportStateChanged(true);
  const size_t events = session_->events().size();

  // The first observer gets what's pending when the second one starts.
  RecordingObserver second;
  session_->AddObserver(&second);
  ASSERT_EQ(first.updates, 1);
  EXPECT_EQ(first.events.size(), 1u);

  session_->OnCaptureMuted(true);
  task_environment_.FastForwardBy(TtcDebugSession::kFlushInterval);
  ASSERT_EQ(second.updates, 1);
  EXPECT_EQ(second.first_event_indices[0], events);
  ASSERT_EQ(second.events.size(), 1u);
  EXPECT_EQ(second.events[0].kind, EventKind::kCaptureMuted);
  session_->RemoveObserver(&second);
  session_->RemoveObserver(&first);
}

TEST_F(TtcDebugSessionTest, ObserverAddedAfterEndGetsOneUpdate) {
  Speak(5, 0.1f);
  session_->End();
  RecordingObserver observer;
  session_->AddObserver(&observer);
  task_environment_.FastForwardBy(TtcDebugSession::kFlushInterval * 5);
  EXPECT_EQ(observer.updates, 1);
  EXPECT_TRUE(observer.last_snapshot.ended);
  EXPECT_TRUE(observer.events.empty());
  EXPECT_TRUE(observer.turns.empty());
  session_->RemoveObserver(&observer);
}

TEST_F(TtcDebugSessionTest, LateReplyResolvesNoResponse) {
  session_->OnTransportStateChanged(true);
  Speak(5, 0.1f);
  Speak(10, 0.0f);
  task_environment_.FastForwardBy(TtcDebugSession::kNoResponseTimeout);
  ASSERT_EQ(session_->turns()[0].status, TurnStatus::kNoResponse);

  // The reply arrives after the watchdog fired.
  session_->OnServerFrame(
      DebugSink::Transcript{.is_input = true, .text = "open my calendar"});
  EXPECT_FALSE(session_->GetSnapshot().no_response_suspected);

  // It is attached to the same turn rather than opening a new one.
  ASSERT_EQ(session_->turns().size(), 1u);
  EXPECT_EQ(session_->turns()[0].status, TurnStatus::kOpen);
  EXPECT_EQ(session_->turns()[0].input_transcript, "open my calendar");
  session_->OnServerFrame(DebugSink::TurnComplete());
  EXPECT_EQ(session_->turns()[0].status, TurnStatus::kNoAudio);
}

TEST_F(TtcDebugSessionTest, LateReplyAfterNewSpeechStartsNewTurn) {
  session_->OnTransportStateChanged(true);
  Speak(5, 0.1f);
  Speak(10, 0.0f);
  task_environment_.FastForwardBy(TtcDebugSession::kNoResponseTimeout);
  // The user tries again before anything comes back.
  Speak(5, 0.1f);
  Speak(10, 0.0f);
  session_->OnServerFrame(
      DebugSink::Transcript{.is_input = true, .text = "hello"});
  ASSERT_EQ(session_->turns().size(), 2u);
  EXPECT_EQ(session_->turns()[0].status, TurnStatus::kNoResponse);
  EXPECT_EQ(session_->turns()[1].input_transcript, "hello");
}

TEST_F(TtcDebugSessionTest, PlaybackAfterTurnClosedIsRecorded) {
  Speak(5, 0.1f);
  Speak(10, 0.0f);
  ReceiveAudio(2);
  session_->OnServerFrame(DebugSink::TurnComplete());
  ASSERT_EQ(session_->turns()[0].status, TurnStatus::kOk);
  EXPECT_FALSE(session_->turns()[0].first_playback_ms);

  // Playback lags behind the server.
  task_environment_.FastForwardBy(base::Milliseconds(100));
  session_->OnClientFrame(
      DebugSink::PlaybackStatus{.last_played_sequence_number = 0});
  EXPECT_TRUE(session_->turns()[0].first_playback_ms.has_value());
}

TEST_F(TtcDebugSessionTest, LongTextIsTruncated) {
  // Two-byte UTF-8 characters, so that a byte cut could split one.
  std::string long_text;
  for (size_t i = 0; i < TtcDebugSession::kMaxTextLength; ++i) {
    long_text += "\xC3\xA9";
  }
  session_->OnError(long_text);
  session_->OnError(long_text);
  EXPECT_LE(session_->GetSnapshot().last_error.size(),
            TtcDebugSession::kMaxTextLength);
}

TEST_F(TtcDebugSessionTest, TranscriptsAreCapped) {
  const std::string chunk(1000, 'a');
  for (int i = 0; i < 20; ++i) {
    session_->OnServerFrame(
        DebugSink::Transcript{.is_input = true, .text = chunk});
  }
  const std::string& transcript = session_->turns()[0].input_transcript;
  EXPECT_LE(transcript.size(), TtcDebugSession::kMaxTranscriptLength);
  EXPECT_GT(transcript.size(), TtcDebugSession::kMaxTranscriptLength - 1000);
  EXPECT_TRUE(transcript.ends_with(kEllipsis));
}

TEST_F(TtcDebugSessionTest, TurnsAreCapped) {
  for (size_t i = 0; i < TtcDebugSession::kMaxTurns; ++i) {
    session_->OnServerFrame(
        DebugSink::Transcript{.is_input = true, .text = "hi"});
    session_->OnServerFrame(DebugSink::TurnComplete());
  }
  EXPECT_EQ(session_->turns().size(), TtcDebugSession::kMaxTurns);
  EXPECT_EQ(session_->GetSnapshot().dropped_items, 0);

  // Each dropped turn is counted once, however much happens in it.
  for (int i = 0; i < 2; ++i) {
    session_->OnServerFrame(
        DebugSink::Transcript{.is_input = true, .text = "hi"});
    ReceiveAudio(2);
    session_->OnServerFrame(DebugSink::TurnComplete());
  }
  EXPECT_EQ(session_->turns().size(), TtcDebugSession::kMaxTurns);
  EXPECT_EQ(session_->GetSnapshot().dropped_items, 2);
  EXPECT_EQ(CountEvents(EventKind::kItemsTruncated), 1);
}

}  // namespace
}  // namespace ttc
