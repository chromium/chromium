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
  FakeConversation() = default;
  ~FakeConversation() override = default;

  void Start() override {}
  void Stop() override {}
  void SendTextInput(const std::string& text) override {}
  void SendContextUpdate(
      const GURL& url,
      const std::string& title,
      const optimization_guide::proto::AnnotatedPageContent& apc) override {}
  void OnPageContextInvalidated() override {}
};

class FakeHarnessPage : public mojom::HarnessPage {
 public:
  FakeHarnessPage() = default;
  ~FakeHarnessPage() override = default;

  mojo::PendingRemote<mojom::HarnessPage> BindAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnModelTurnStarted() override { turn_started_ = true; }
  void OnModelTurnEnded() override { turn_ended_ = true; }

  bool turn_started_ = false;
  bool turn_ended_ = false;

 private:
  mojo::Receiver<mojom::HarnessPage> receiver_{this};
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
                      [](SessionController&) -> std::unique_ptr<Conversation> {
                        return std::make_unique<FakeConversation>();
                      }));
            })));
  }
  ~HarnessPageHandlerTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TtcKeyedService> service_ = nullptr;
  FakeHarnessPage page_;
  mojo::Remote<mojom::HarnessPageHandler> handler_remote_;
  std::unique_ptr<HarnessPageHandler> handler_;

  void SetUp() override {
    handler_ = std::make_unique<HarnessPageHandler>(
        *service_, handler_remote_.BindNewPipeAndPassReceiver(),
        page_.BindAndPassRemote());
  }
};

TEST_F(HarnessPageHandlerTest, StartSessionSucceeds) {
  base::RunLoop run_loop;
  handler_remote_->StartSession(base::BindLambdaForTesting([&](bool success) {
    EXPECT_TRUE(success);
    run_loop.Quit();
  }));
  run_loop.Run();
}

TEST_F(HarnessPageHandlerTest, DoubleStartSessionDoesNotCrashAndReturnsFalse) {
  base::RunLoop run_loop1;
  handler_remote_->StartSession(base::BindLambdaForTesting([&](bool success) {
    EXPECT_TRUE(success);
    run_loop1.Quit();
  }));
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
  turn_loop.Run();
}

TEST_F(HarnessPageHandlerTest, SendAudioReturnsResult) {
  base::RunLoop start_loop;
  handler_remote_->StartSession(base::BindLambdaForTesting([&](bool success) {
    EXPECT_TRUE(success);
    start_loop.Quit();
  }));
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
  turn_loop.Run();
}

TEST_F(HarnessPageHandlerTest, CloseSessionDoesNotCrash) {
  handler_remote_->CloseSession();
  handler_remote_.FlushForTesting();
}

}  // namespace
}  // namespace ttc
