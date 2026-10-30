// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "chrome/browser/ttc/app/app_browser_test_base.h"
#include "chrome/browser/ttc/app/conversation_impl.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/states.h"
#include "components/optimization_guide/proto/features/streaming/ttc.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace proto = optimization_guide::proto;

// Tests that the conversation tracks who is speaking based on the turn state
// updates sent by the server.
class TtcSpeakerStateBrowserTest : public AppBrowserTestBase {
 public:
  TtcSpeakerStateBrowserTest() = default;
  ~TtcSpeakerStateBrowserTest() override = default;

  void StartLiveSession() {
    StartSessionAndConnectBackend();
    RespondSessionStatus(proto::SessionStatusResponse::STATE_CONNECTED,
                         "session_1");
    ASSERT_EQ(session_controller()->GetSessionLifecycle(),
              SessionLifecycle::kLive);
  }

  void RespondUserTurnState(proto::UserTurnStateUpdate::State state) {
    proto::TtcServerFrame frame;
    frame.mutable_user_turn_state_update()->set_state(state);
    SimulateResponse(frame);
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
};

IN_PROC_BROWSER_TEST_F(TtcSpeakerStateBrowserTest, UserTurnThenModelTurn) {
  StartLiveSession();
  EXPECT_EQ(conversation()->speaker(), Speaker::kIdle);

  RespondUserTurnState(proto::UserTurnStateUpdate::TURN_STARTED);
  EXPECT_EQ(conversation()->speaker(), Speaker::kUser);

  RespondUserTurnState(proto::UserTurnStateUpdate::TURN_COMPLETED);
  EXPECT_EQ(conversation()->speaker(), Speaker::kIdle);

  constexpr int64_t kGenerationId = 42;
  RespondModelTurnState(proto::ModelTurnStateUpdate::TURN_STARTED,
                        kGenerationId);
  EXPECT_EQ(conversation()->speaker(), Speaker::kModel);

  // The model remains the speaker while its output audio plays back.
  RespondModelTurnState(proto::ModelTurnStateUpdate::GENERATION_COMPLETED,
                        kGenerationId);
  EXPECT_EQ(conversation()->speaker(), Speaker::kModel);

  RespondModelTurnState(proto::ModelTurnStateUpdate::TURN_COMPLETED,
                        kGenerationId);
  EXPECT_EQ(conversation()->speaker(), Speaker::kIdle);
}

IN_PROC_BROWSER_TEST_F(TtcSpeakerStateBrowserTest, UserInterruptsModel) {
  StartLiveSession();

  constexpr int64_t kGenerationId = 42;
  RespondModelTurnState(proto::ModelTurnStateUpdate::TURN_STARTED,
                        kGenerationId);
  EXPECT_EQ(conversation()->speaker(), Speaker::kModel);

  // When the user barges in, the server sends the user's turn start before
  // the model's interrupted and turn completed updates.
  RespondUserTurnState(proto::UserTurnStateUpdate::TURN_STARTED);
  EXPECT_EQ(conversation()->speaker(), Speaker::kUser);

  RespondModelTurnState(proto::ModelTurnStateUpdate::INTERRUPTED,
                        kGenerationId);
  EXPECT_EQ(conversation()->speaker(), Speaker::kUser);

  // The model completing its interrupted turn must not take the turn away from
  // the user.
  RespondModelTurnState(proto::ModelTurnStateUpdate::TURN_COMPLETED,
                        kGenerationId);
  EXPECT_EQ(conversation()->speaker(), Speaker::kUser);

  RespondUserTurnState(proto::UserTurnStateUpdate::TURN_COMPLETED);
  EXPECT_EQ(conversation()->speaker(), Speaker::kIdle);
}

}  // namespace ttc
