// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/test/test_future.h"
#include "chrome/browser/optimization_guide/mock_remote_model_executor.h"
#include "chrome/browser/ttc/app/app_browser_test_base.h"
#include "chrome/browser/ttc/app/conversation_impl.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/states.h"
#include "components/optimization_guide/proto/features/streaming/ttc.pb.h"
#include "components/ttc/app/public/conversation.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace ttc {

namespace {

namespace proto = optimization_guide::proto;

class TtcHotlProtocolBrowserTest : public AppBrowserTestBase {
 public:
  TtcHotlProtocolBrowserTest() = default;
  ~TtcHotlProtocolBrowserTest() override = default;

  void StartSession() {
    ON_CALL(*model_execution_session(), Send)
        .WillByDefault([callback = playback_statuses_.GetRepeatingCallback()](
                           const google::protobuf::MessageLite& message) {
          proto::TtcClientFrame frame;
          CHECK(frame.ParseFromString(message.SerializeAsString()));
          if (frame.has_playback_status()) {
            callback.Run(frame.playback_status());
          }
        });

    StartSessionAndConnectBackend();
    RespondSessionStatus(proto::SessionStatusResponse::STATE_CONNECTED,
                         "session_1");
    ASSERT_EQ(session_controller()->GetSessionLifecycle(),
              SessionLifecycle::kLive);
  }

  void RespondModelTurnState(proto::ModelTurnStateUpdate::State state,
                             int64_t generation_id) {
    proto::TtcServerFrame frame;
    proto::ModelTurnStateUpdate* update =
        frame.mutable_model_turn_state_update();
    update->set_state(state);
    update->set_generation_id(generation_id);
    SimulateResponse(frame);
  }

  proto::PlaybackStatus TakeNextPlaybackStatus() {
    return playback_statuses_.Take();
  }

  bool HasPlaybackStatuses() const { return playback_statuses_.IsReady(); }

 protected:
  const std::vector<int16_t> samples_{std::vector<int16_t>(480, 1000)};
  base::test::TestFuture<proto::PlaybackStatus> playback_statuses_{
      base::test::TestFutureMode::kQueue};
};

IN_PROC_BROWSER_TEST_F(
    TtcHotlProtocolBrowserTest,
    GenerationEndedBeforeConfirmationFlushesAudioAndCompletesPlayback) {
  StartSession();
  ASSERT_NE(conversation(), nullptr);
  ASSERT_NE(conversation()->audio_controller(), nullptr);

  constexpr int64_t kGenerationId = 101;
  RespondModelTurnState(proto::ModelTurnStateUpdate::TURN_STARTED,
                        kGenerationId);

  SendServerAudio(samples_, /*sequence_number=*/1);
  EXPECT_TRUE(conversation()->audio_controller()->is_playing());

  RespondModelTurnState(proto::ModelTurnStateUpdate::GENERATION_COMPLETED,
                        kGenerationId);
  EXPECT_TRUE(conversation()->audio_controller()->is_playing());

  // Trigger HOTL confirmation after generation ended while audio is still
  // queued for playback.
  conversation()->SetHotlAudioSink(base::DoNothing());

  EXPECT_FALSE(conversation()->audio_controller()->is_playing());
  proto::PlaybackStatus status = TakeNextPlaybackStatus();
  EXPECT_TRUE(status.playback_complete());
  EXPECT_EQ(status.generation_id(), kGenerationId);

  RespondModelTurnState(proto::ModelTurnStateUpdate::TURN_COMPLETED,
                        kGenerationId);
  EXPECT_FALSE(HasPlaybackStatuses());
}

IN_PROC_BROWSER_TEST_F(
    TtcHotlProtocolBrowserTest,
    GenerationEndedAfterConfirmationDropsAudioAndCompletesPlayback) {
  StartSession();
  ASSERT_NE(conversation(), nullptr);
  ASSERT_NE(conversation()->audio_controller(), nullptr);

  constexpr int64_t kGenerationId = 102;
  RespondModelTurnState(proto::ModelTurnStateUpdate::TURN_STARTED,
                        kGenerationId);

  SendServerAudio(samples_, /*sequence_number=*/1);
  EXPECT_TRUE(conversation()->audio_controller()->is_playing());

  // Trigger HOTL confirmation while model generation is still actively running.
  conversation()->SetHotlAudioSink(base::DoNothing());

  EXPECT_FALSE(conversation()->audio_controller()->is_playing());
  proto::PlaybackStatus status = TakeNextPlaybackStatus();
  EXPECT_TRUE(status.playback_complete());
  EXPECT_EQ(status.generation_id(), kGenerationId);

  // Subsequent audio frames arriving while confirmation is active must be
  // dropped and must not be queued or played.
  SendServerAudio(samples_, /*sequence_number=*/2);
  SendServerAudio(samples_, /*sequence_number=*/3);
  EXPECT_FALSE(conversation()->audio_controller()->is_playing());

  RespondModelTurnState(proto::ModelTurnStateUpdate::GENERATION_COMPLETED,
                        kGenerationId);
  RespondModelTurnState(proto::ModelTurnStateUpdate::TURN_COMPLETED,
                        kGenerationId);
  EXPECT_FALSE(HasPlaybackStatuses());
}

IN_PROC_BROWSER_TEST_F(
    TtcHotlProtocolBrowserTest,
    ConfirmationActiveBeforeTurnStartsCompletesPlaybackImmediately) {
  StartSession();
  ASSERT_NE(conversation(), nullptr);
  ASSERT_NE(conversation()->audio_controller(), nullptr);

  // Activate HOTL confirmation before the model turn begins.
  conversation()->SetHotlAudioSink(base::DoNothing());

  constexpr int64_t kGenerationId = 103;
  RespondModelTurnState(proto::ModelTurnStateUpdate::TURN_STARTED,
                        kGenerationId);

  // Playback completion report is dispatched immediately upon turn start.
  proto::PlaybackStatus status = TakeNextPlaybackStatus();
  EXPECT_TRUE(status.playback_complete());
  EXPECT_EQ(status.generation_id(), kGenerationId);

  // Incoming audio for this turn is dropped.
  SendServerAudio(samples_, /*sequence_number=*/1);
  EXPECT_FALSE(conversation()->audio_controller()->is_playing());

  RespondModelTurnState(proto::ModelTurnStateUpdate::GENERATION_COMPLETED,
                        kGenerationId);
  RespondModelTurnState(proto::ModelTurnStateUpdate::TURN_COMPLETED,
                        kGenerationId);
  EXPECT_FALSE(HasPlaybackStatuses());

  // Subsequent turns play normally once the sink is cleared.
  conversation()->SetHotlAudioSink(base::NullCallback());

  constexpr int64_t kNextGenerationId = 104;
  RespondModelTurnState(proto::ModelTurnStateUpdate::TURN_STARTED,
                        kNextGenerationId);
  SendServerAudio(samples_, /*sequence_number=*/1);
  EXPECT_TRUE(conversation()->audio_controller()->is_playing());
}

}  // namespace

}  // namespace ttc
