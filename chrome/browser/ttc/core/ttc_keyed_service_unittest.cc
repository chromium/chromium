// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/ttc_keyed_service.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/session_controller_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {
class FakeConversation : public Conversation {
 public:
  explicit FakeConversation(base::RepeatingClosure on_stopped = {})
      : on_stopped_(std::move(on_stopped)) {}
  ~FakeConversation() override = default;

  void Start() override { is_started_ = true; }
  void Stop() override {
    is_started_ = false;
    if (on_stopped_) {
      on_stopped_.Run();
    }
  }
  void SendTextInput(const std::string& text) override {}
  void SendContextUpdate(
      const GURL& url,
      const std::string& title,
      const optimization_guide::proto::AnnotatedPageContent& apc) override {}
  void OnPageContextChanged() override {}

  bool is_started() const { return is_started_; }

 private:
  bool is_started_ = false;
  base::RepeatingClosure on_stopped_;
};
}  // namespace

class TtcKeyedServiceUnitTest : public testing::Test {
 public:
  TtcKeyedServiceUnitTest() {
    scoped_feature_list_.InitAndEnableFeature(kTtc);
    service_ = std::make_unique<TtcKeyedService>(
        &profile_,
        base::BindRepeating(&TtcKeyedServiceUnitTest::CreateFakeConversation,
                            base::Unretained(this)));
  }
  ~TtcKeyedServiceUnitTest() override = default;

  std::unique_ptr<Conversation> CreateFakeConversation(SessionController&) {
    return std::make_unique<FakeConversation>(
        base::BindRepeating(&TtcKeyedServiceUnitTest::OnConversationStopped,
                            base::Unretained(this)));
  }

  void OnConversationStopped() { conversation_stopped_count_++; }

  int conversation_stopped_count() const { return conversation_stopped_count_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TtcKeyedService> service_;
  int conversation_stopped_count_ = 0;
};

TEST_F(TtcKeyedServiceUnitTest, EndSessionDoesNotCrash) {
  ASSERT_EQ(service_->session_controller(), nullptr);
  service_->EndSession();
}

TEST_F(TtcKeyedServiceUnitTest, StartSession) {
  EXPECT_FALSE(service_->is_session_active());
  service_->StartSession();
  EXPECT_TRUE(service_->is_session_active());
}

TEST_F(TtcKeyedServiceUnitTest, StartSessionFailsIfAlreadyStarted) {
  service_->StartSession();
  EXPECT_DEATH_IF_SUPPORTED(service_->StartSession(), "");
  EXPECT_TRUE(service_->is_session_active());
}

TEST_F(TtcKeyedServiceUnitTest, EndSessionRemovesController) {
  service_->StartSession();
  ASSERT_NE(service_->session_controller(), nullptr);
  service_->EndSession();
  EXPECT_EQ(service_->session_controller(), nullptr);
}

TEST_F(TtcKeyedServiceUnitTest, ShutdownRemovesController) {
  service_->StartSession();
  ASSERT_NE(service_->session_controller(), nullptr);
  service_->Shutdown();
  EXPECT_EQ(service_->session_controller(), nullptr);
}

TEST_F(TtcKeyedServiceUnitTest, ConversationStartedAndStoppedWithSession) {
  service_->StartSession();
  SessionController* controller = service_->session_controller();
  ASSERT_NE(controller, nullptr);
  auto* conversation = static_cast<FakeConversation*>(
      &static_cast<SessionControllerImpl*>(controller)->conversation());
  ASSERT_NE(conversation, nullptr);
  EXPECT_TRUE(conversation->is_started());
  EXPECT_EQ(conversation_stopped_count(), 0);

  service_->EndSession();
  EXPECT_EQ(service_->session_controller(), nullptr);
  EXPECT_EQ(conversation_stopped_count(), 1);
}

TEST_F(TtcKeyedServiceUnitTest, ConversationErrorEndsSession) {
  service_->StartSession();
  SessionController* controller = service_->session_controller();
  ASSERT_NE(controller, nullptr);

  // When disconnected (with or without an error message), SessionController
  // should post a task to end the session.
  controller->OnTransportStateChanged(
      /*connected=*/false, /*session_id=*/"",
      /*error_message=*/"");

  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return service_->session_controller() == nullptr; }));
}

}  // namespace ttc
