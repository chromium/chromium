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
using testing::ElementsAre;

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
  }

  int updates = 0;
  TtcDebugSession::Snapshot last_snapshot;
  std::vector<size_t> first_event_indices;
  std::vector<TtcDebugSession::TimelineEvent> events;
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
  const base::ListValue* events = value.FindList("events");
  ASSERT_TRUE(events);
  for (const base::Value& event : *events) {
    EXPECT_NE(*event.GetDict().FindString("kind"), "mic_level");
    EXPECT_NE(*event.GetDict().FindString("kind"), "chunk_sent");
    EXPECT_NE(*event.GetDict().FindString("kind"), "playback");
  }
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
  for (size_t i = 0; i <= TtcDebugSession::kMaxFrequentEvents; ++i) {
    session_->OnMicLevel(0.1f);
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
  session_->RemoveObserver(&observer);
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

}  // namespace
}  // namespace ttc
