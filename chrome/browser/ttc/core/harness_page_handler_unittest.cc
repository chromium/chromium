// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/harness_page_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "chrome/browser/ttc/core/eval_harness.mojom.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ttc/core/ttc_keyed_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {
namespace {

class FakeConversation : public Conversation {
 public:
  explicit FakeConversation(std::unique_ptr<AudioController> audio_controller)
      : audio_controller_(std::move(audio_controller)) {}
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
  bool IsMicMuted() const override { return false; }

 private:
  std::unique_ptr<AudioController> audio_controller_;
};

class HarnessPageHandlerTest : public testing::Test {
 public:
  HarnessPageHandlerTest() {
    scoped_feature_list_.InitAndEnableFeature(kTtc);
    service_ = static_cast<TtcKeyedService*>(
        TtcKeyedServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_,
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<TtcKeyedService>(
                  Profile::FromBrowserContext(context),
                  base::BindRepeating(
                      [](SessionController&,
                         std::unique_ptr<AudioController> audio_controller)
                          -> std::unique_ptr<Conversation> {
                        return std::make_unique<FakeConversation>(
                            std::move(audio_controller));
                      }));
            })));
  }
  ~HarnessPageHandlerTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TtcKeyedService> service_ = nullptr;
  mojo::Remote<mojom::HarnessPageHandler> handler_remote_;
  std::unique_ptr<HarnessPageHandler> handler_;

  void SetUp() override {
    handler_ = std::make_unique<HarnessPageHandler>(
        *service_, handler_remote_.BindNewPipeAndPassReceiver());
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
