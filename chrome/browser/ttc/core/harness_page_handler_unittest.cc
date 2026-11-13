// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/harness_page_handler.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ttc/core/eval_harness.mojom.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/pointer_tracker.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/browser/ttc/core/switches.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ttc/core/ttc_keyed_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ttc/app/public/conversation.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"

namespace ttc {
namespace {

class FakeConversation : public Conversation {
 public:
  FakeConversation(SessionController& session_controller,
                   std::unique_ptr<AudioController> audio_controller)
      : session_controller_(session_controller),
        audio_controller_(std::move(audio_controller)) {}
  ~FakeConversation() override = default;

  void Start() override {}
  void Stop() override {}
  void SendTextInput(const std::string& text) override {}
  void SendContextUpdate(
      const GURL& url,
      const std::string& title,
      const optimization_guide::proto::AnnotatedPageContent& apc) override {}
  void OnPageContextInvalidated() override {}
  void SetMicMuted(bool muted) override {}
  void SetHotlAudioSink(HotlAudioSinkCallback sink) override {}
  SessionJournal& GetJournal() override {
    return session_controller_->GetJournal();
  }

 private:
  const raw_ref<SessionController> session_controller_;
  std::unique_ptr<AudioController> audio_controller_;
};

class HarnessPageHandlerTest : public testing::Test {
 public:
  HarnessPageHandlerTest() {
    scoped_feature_list_.InitWithFeatures({kTtc, kTtcGetPointer}, {});
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kEnableTtcTestApi);
    service_ = static_cast<TtcKeyedService*>(
        TtcKeyedServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_,
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<TtcKeyedService>(
                  Profile::FromBrowserContext(context),
                  base::BindRepeating(
                      [](SessionController& session_controller,
                         std::unique_ptr<AudioController> audio_controller)
                          -> std::unique_ptr<Conversation> {
                        return std::make_unique<FakeConversation>(
                            session_controller, std::move(audio_controller));
                      }));
            })));
  }
  ~HarnessPageHandlerTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
  raw_ptr<TtcKeyedService> service_ = nullptr;
  mojo::Remote<mojom::HarnessPageHandler> handler_remote_;
  std::unique_ptr<HarnessPageHandler> handler_;

  void SetUp() override {
    handler_ = std::make_unique<HarnessPageHandler>(
        *service_, handler_remote_.BindNewPipeAndPassReceiver());
  }

  // Starts a session through the handler and makes it live.
  void StartLiveSession() {
    base::test::TestFuture<bool> started;
    handler_remote_->StartSession(started.GetCallback());
    handler_remote_.FlushForTesting();
    ASSERT_TRUE(service_->is_session_active());
    service_->session_controller()->SetSessionLifecycle(
        SessionLifecycle::kLive);
    ASSERT_TRUE(started.Get());
  }

  bool SetPointerLocation(std::optional<gfx::PointF> point) {
    base::test::TestFuture<bool> success;
    handler_remote_->SetPointerLocation(point, success.GetCallback());
    return success.Get();
  }

  PointerTracker& pointer_tracker() {
    return *static_cast<SessionControllerImpl*>(service_->session_controller())
                ->pointer_tracker();
  }

  // Where the session thinks the user pointed in a turn starting now.
  std::vector<PointerTracker::Location> ResolvePointerLocations() {
    const base::TimeTicks now = base::TimeTicks::Now();
    return pointer_tracker().ResolveLocations(now, now);
  }
};

TEST_F(HarnessPageHandlerTest, StartSessionSucceeds) {
  base::RunLoop run_loop;
  handler_remote_->StartSession(base::BindLambdaForTesting([&](bool success) {
    EXPECT_TRUE(success);
    run_loop.Quit();
  }));
  handler_remote_.FlushForTesting();
  ASSERT_TRUE(service_->is_session_active());
  service_->session_controller()->SetSessionLifecycle(SessionLifecycle::kLive);
  run_loop.Run();
}

TEST_F(HarnessPageHandlerTest, DoubleStartSessionDoesNotCrashAndReturnsFalse) {
  base::RunLoop run_loop1;
  handler_remote_->StartSession(base::BindLambdaForTesting([&](bool success) {
    EXPECT_TRUE(success);
    run_loop1.Quit();
  }));
  handler_remote_.FlushForTesting();
  ASSERT_TRUE(service_->is_session_active());
  service_->session_controller()->SetSessionLifecycle(SessionLifecycle::kLive);
  run_loop1.Run();

  base::RunLoop run_loop2;
  handler_remote_->StartSession(base::BindLambdaForTesting([&](bool success) {
    EXPECT_FALSE(success);
    run_loop2.Quit();
  }));
  run_loop2.Run();
}

TEST_F(HarnessPageHandlerTest, SendTextReturnsResult) {
  // First start the session.
  base::RunLoop start_loop;
  handler_remote_->StartSession(base::BindLambdaForTesting([&](bool success) {
    EXPECT_TRUE(success);
    start_loop.Quit();
  }));
  handler_remote_.FlushForTesting();
  ASSERT_TRUE(service_->is_session_active());
  service_->session_controller()->SetSessionLifecycle(SessionLifecycle::kLive);
  start_loop.Run();

  base::RunLoop turn_loop;
  handler_remote_->SendText(
      "Hello, world!",
      base::BindLambdaForTesting([&](mojom::TurnResultPtr result) {
        ASSERT_TRUE(result);
        EXPECT_EQ(result->status, mojom::TurnStatus::kComplete);
        EXPECT_EQ(result->error_code, 0);
        turn_loop.Quit();
      }));
  handler_remote_.FlushForTesting();
  ASSERT_TRUE(handler_->eval_harness());
  handler_->eval_harness()->OnModelTurnComplete();
  turn_loop.Run();
}

TEST_F(HarnessPageHandlerTest, SendAudioReturnsResult) {
  base::RunLoop start_loop;
  handler_remote_->StartSession(base::BindLambdaForTesting([&](bool success) {
    EXPECT_TRUE(success);
    start_loop.Quit();
  }));
  handler_remote_.FlushForTesting();
  ASSERT_TRUE(service_->is_session_active());
  service_->session_controller()->SetSessionLifecycle(SessionLifecycle::kLive);
  start_loop.Run();

  base::RunLoop turn_loop;
  std::vector<uint8_t> pcm_bytes = {0, 0, 1, 0};
  handler_remote_->SendAudio(
      pcm_bytes, base::BindLambdaForTesting([&](mojom::TurnResultPtr result) {
        ASSERT_TRUE(result);
        EXPECT_EQ(result->status, mojom::TurnStatus::kComplete);
        EXPECT_EQ(result->error_code, 0);
        turn_loop.Quit();
      }));
  handler_remote_.FlushForTesting();
  ASSERT_TRUE(handler_->eval_harness());
  handler_->eval_harness()->OnModelTurnComplete();
  turn_loop.Run();
}

TEST_F(HarnessPageHandlerTest, SetPointerLocationNeedsSession) {
  EXPECT_FALSE(SetPointerLocation(gfx::PointF(10, 20)));
}

TEST_F(HarnessPageHandlerTest, SetPointerLocationNeedsTestApiSwitch) {
  StartLiveSession();
  scoped_command_line_.GetProcessCommandLine()->RemoveSwitch(
      switches::kEnableTtcTestApi);
  EXPECT_FALSE(SetPointerLocation(gfx::PointF(10, 20)));
}

TEST_F(HarnessPageHandlerTest, SetPointerLocationPressesUntilTurnEnds) {
  StartLiveSession();
  ASSERT_TRUE(SetPointerLocation(gfx::PointF(10, 20)));
  // The real pointer does not compete with the simulated one.
  pointer_tracker().RecordSample(base::TimeTicks::Now(), gfx::PointF(300, 300),
                                 /*is_press=*/true);

  base::test::TestFuture<mojom::TurnResultPtr> turn;
  handler_remote_->SendText("What is this?", turn.GetCallback());
  handler_remote_.FlushForTesting();
  std::vector<PointerTracker::Location> locations = ResolvePointerLocations();
  ASSERT_EQ(locations.size(), 1u);
  EXPECT_EQ(locations[0].point, gfx::PointF(10, 20));
  EXPECT_TRUE(locations[0].is_press);

  handler_->eval_harness()->OnModelTurnComplete();
  ASSERT_TRUE(turn.Get());
  // The real pointer counts again.
  locations = ResolvePointerLocations();
  ASSERT_EQ(locations.size(), 1u);
  EXPECT_EQ(locations[0].point, gfx::PointF(300, 300));
}

TEST_F(HarnessPageHandlerTest, SetPointerLocationNullForgetsPoint) {
  StartLiveSession();
  ASSERT_TRUE(SetPointerLocation(gfx::PointF(10, 20)));
  ASSERT_TRUE(SetPointerLocation(std::nullopt));
  EXPECT_THAT(ResolvePointerLocations(), testing::IsEmpty());
}

TEST_F(HarnessPageHandlerTest, CloseSessionDoesNotCrash) {
  handler_remote_->CloseSession();
  handler_remote_.FlushForTesting();
}

TEST_F(HarnessPageHandlerTest, SessionEndedDuringTurnResolvesWithError) {
  base::RunLoop start_loop;
  handler_remote_->StartSession(base::BindLambdaForTesting([&](bool success) {
    EXPECT_TRUE(success);
    start_loop.Quit();
  }));
  handler_remote_.FlushForTesting();
  ASSERT_TRUE(service_->is_session_active());
  service_->session_controller()->SetSessionLifecycle(SessionLifecycle::kLive);
  start_loop.Run();

  base::RunLoop turn_loop;
  handler_remote_->SendText(
      "Hello, world!",
      base::BindLambdaForTesting([&](mojom::TurnResultPtr result) {
        ASSERT_TRUE(result);
        EXPECT_EQ(result->status, mojom::TurnStatus::kError);
        EXPECT_EQ(result->error_message, "Session ended before turn completed");
        turn_loop.Quit();
      }));
  handler_remote_.FlushForTesting();

  // Abruptly end the session while the turn is in flight.
  service_->EndSession();
  turn_loop.Run();
}

TEST_F(HarnessPageHandlerTest, FatalErrorDuringTurnResolvesWithErrorCode) {
  base::RunLoop start_loop;
  handler_remote_->StartSession(base::BindLambdaForTesting([&](bool success) {
    EXPECT_TRUE(success);
    start_loop.Quit();
  }));
  handler_remote_.FlushForTesting();
  ASSERT_TRUE(service_->is_session_active());
  service_->session_controller()->SetSessionLifecycle(SessionLifecycle::kLive);
  start_loop.Run();

  base::RunLoop turn_loop;
  handler_remote_->SendText(
      "Hello, world!",
      base::BindLambdaForTesting([&](mojom::TurnResultPtr result) {
        ASSERT_TRUE(result);
        EXPECT_EQ(result->status, mojom::TurnStatus::kError);
        EXPECT_EQ(result->error_code,
                  static_cast<int32_t>(ErrorCode::kInternalBackendError));
        turn_loop.Quit();
      }));
  handler_remote_.FlushForTesting();

  // Trigger a fatal error on the SessionController while the turn is in flight.
  service_->session_controller()->OnError(ErrorCode::kInternalBackendError);
  turn_loop.Run();
}

TEST_F(HarnessPageHandlerTest, DestroyingHandlerEndsActiveSession) {
  base::RunLoop start_loop;
  handler_remote_->StartSession(base::BindLambdaForTesting([&](bool success) {
    EXPECT_TRUE(success);
    start_loop.Quit();
  }));
  handler_remote_.FlushForTesting();
  ASSERT_TRUE(service_->is_session_active());
  service_->session_controller()->SetSessionLifecycle(SessionLifecycle::kLive);
  start_loop.Run();

  // Destroying the owning HarnessPageHandler (e.g. when the WebUI tab closes)
  // should end the session it started.
  handler_.reset();
  EXPECT_FALSE(service_->is_session_active());
}

TEST_F(HarnessPageHandlerTest, SecondHandlerDoesNotDisruptActiveSession) {
  base::RunLoop start_loop;
  handler_remote_->StartSession(base::BindLambdaForTesting([&](bool success) {
    EXPECT_TRUE(success);
    start_loop.Quit();
  }));
  handler_remote_.FlushForTesting();
  ASSERT_TRUE(service_->is_session_active());
  service_->session_controller()->SetSessionLifecycle(SessionLifecycle::kLive);
  start_loop.Run();

  // A second WebUI window binding its own HarnessPageHandler should not
  // destroy or disrupt the first window's session.
  mojo::Remote<mojom::HarnessPageHandler> second_remote;
  auto second_handler = std::make_unique<HarnessPageHandler>(
      *service_, second_remote.BindNewPipeAndPassReceiver());

  base::RunLoop second_start_loop;
  second_remote->StartSession(base::BindLambdaForTesting([&](bool success) {
    EXPECT_FALSE(success);
    second_start_loop.Quit();
  }));
  second_start_loop.Run();
  EXPECT_TRUE(service_->is_session_active());

  // Closing or destroying the second handler must not end the first handler's
  // active session.
  second_remote->CloseSession();
  second_remote.FlushForTesting();
  second_handler.reset();
  EXPECT_TRUE(service_->is_session_active());
}

}  // namespace
}  // namespace ttc
