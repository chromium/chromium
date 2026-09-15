// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/ttc_keyed_service.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ttc/conversation.h"
#include "chrome/browser/ttc/features.h"
#include "chrome/browser/ttc/session_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {
class FakeConversation : public Conversation {
 public:
  FakeConversation() = default;
  ~FakeConversation() override = default;

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }
  void Start() override { is_started_ = true; }
  void Stop() override { is_started_ = false; }
  bool is_connected() const override { return is_started_; }
  void SendTextInput(const std::string& text) override {}
  void SendContextUpdate(
      const GURL& url,
      const std::string& title,
      const optimization_guide::proto::AnnotatedPageContent& apc) override {}
  void SendToolSetUpdate(const std::vector<ToolDefinition>& tools) override {}

  void OnPageContextChanged() override {}

 private:
  bool is_started_ = false;
  base::ObserverList<Observer> observers_;
};
}  // namespace

class TtcKeyedServiceUnitTest : public testing::Test {
 public:
  TtcKeyedServiceUnitTest() {
    scoped_feature_list_.InitAndEnableFeature(kTtc);
    Conversation::SetFactoryForTesting(
        base::BindRepeating([](Profile*) -> std::unique_ptr<Conversation> {
          return std::make_unique<FakeConversation>();
        }));
    service_ = std::make_unique<TtcKeyedService>(&profile_);
  }
  ~TtcKeyedServiceUnitTest() override {
    Conversation::SetFactoryForTesting({});
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TtcKeyedService> service_;
};

TEST_F(TtcKeyedServiceUnitTest, EndSessionDoesNotCrash) {
  ASSERT_EQ(service_->session_controller(), nullptr);
  service_->EndSession();
}

TEST_F(TtcKeyedServiceUnitTest, StartSession) {
  ASSERT_EQ(service_->session_controller(), nullptr);
  service_->StartSession();
  SessionController* controller = service_->session_controller();
  ASSERT_NE(controller, nullptr);
}

TEST_F(TtcKeyedServiceUnitTest, StartSessionFailsIfAlreadyStarted) {
  service_->StartSession();
  EXPECT_DEATH_IF_SUPPORTED(service_->StartSession(), "");
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

}  // namespace ttc
