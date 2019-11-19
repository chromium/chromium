// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/tts/arc_tts_service.h"

#include <memory>

#include "base/threading/platform_thread.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class TestableTtsController : public content::TtsController {
 public:
  TestableTtsController() = default;
  ~TestableTtsController() override = default;

  void OnTtsEvent(int utterance_id,
                  content::TtsEventType event_type,
                  int char_index,
                  int length,
                  const std::string& error_message) override {
    last_utterance_id_ = utterance_id;
    last_event_type_ = event_type;
    last_char_index_ = char_index;
    last_length_ = length;
    last_error_message_ = error_message;
  }

  // Unimplemented.
  bool IsSpeaking() override { return false; }
  void SpeakOrEnqueue(
      std::unique_ptr<content::TtsUtterance> utterance) override {}
  void Stop() override {}
  void Stop(const GURL& source_url) override {}
  void Pause() override {}
  void Resume() override {}
  void GetVoices(content::BrowserContext* browser_context,
                 std::vector<content::VoiceData>* out_voices) override {}
  void VoicesChanged() override {}
  void AddVoicesChangedDelegate(
      content::VoicesChangedDelegate* delegate) override {}
  void RemoveVoicesChangedDelegate(
      content::VoicesChangedDelegate* delegate) override {}
  void RemoveUtteranceEventDelegate(
      content::UtteranceEventDelegate* delegate) override {}
  void SetTtsEngineDelegate(content::TtsEngineDelegate* delegate) override {}
  content::TtsEngineDelegate* GetTtsEngineDelegate() override {
    return nullptr;
  }
  void SetTtsPlatform(content::TtsPlatform* tts_platform) override {}
  int QueueSize() override { return 0; }

  void StripSSML(
      const std::string& utterance,
      base::OnceCallback<void(const std::string&)> callback) override {}

  int last_utterance_id_;
  content::TtsEventType last_event_type_;
  int last_char_index_;
  int last_length_;
  std::string last_error_message_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestableTtsController);
};

class ArcTtsServiceTest : public testing::Test {
 public:
  ArcTtsServiceTest()
      : arc_service_manager_(std::make_unique<ArcServiceManager>()),
        testing_profile_(std::make_unique<TestingProfile>()),
        tts_controller_(std::make_unique<TestableTtsController>()),
        tts_service_(ArcTtsService::GetForBrowserContextForTesting(
            testing_profile_.get())) {
    tts_service_->set_tts_controller_for_testing(tts_controller_.get());
  }

  ~ArcTtsServiceTest() override { tts_service_->Shutdown(); }

 protected:
  ArcTtsService* tts_service() const { return tts_service_; }
  TestableTtsController* tts_controller() const {
    return tts_controller_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<TestableTtsController> tts_controller_;
  ArcTtsService* const tts_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcTtsServiceTest);
};

// Tests that ArcTtsService can be constructed and destructed.
TEST_F(ArcTtsServiceTest, TestConstructDestruct) {}

// Tests that OnTtsEvent() properly calls into
// TtsControllerDelegateImpl::OnTtsEvent().
TEST_F(ArcTtsServiceTest, TestOnTtsEvent) {
  tts_service()->OnTtsEvent(1, mojom::TtsEventType::START, 0, "");
  EXPECT_EQ(1, tts_controller()->last_utterance_id_);
  EXPECT_EQ(content::TTS_EVENT_START, tts_controller()->last_event_type_);
  EXPECT_EQ(-1, tts_controller()->last_length_);
  EXPECT_EQ("", tts_controller()->last_error_message_);

  tts_service()->OnTtsEvent(1, mojom::TtsEventType::END, 10, "");
  EXPECT_EQ(1, tts_controller()->last_utterance_id_);
  EXPECT_EQ(content::TTS_EVENT_END, tts_controller()->last_event_type_);
  EXPECT_EQ(-1, tts_controller()->last_length_);
  EXPECT_EQ("", tts_controller()->last_error_message_);

  tts_service()->OnTtsEvent(2, mojom::TtsEventType::INTERRUPTED, 0, "");
  EXPECT_EQ(2, tts_controller()->last_utterance_id_);
  EXPECT_EQ(content::TTS_EVENT_INTERRUPTED, tts_controller()->last_event_type_);
  EXPECT_EQ(-1, tts_controller()->last_length_);
  EXPECT_EQ("", tts_controller()->last_error_message_);

  tts_service()->OnTtsEvent(3, mojom::TtsEventType::ERROR, 0, "");
  EXPECT_EQ(3, tts_controller()->last_utterance_id_);
  EXPECT_EQ(content::TTS_EVENT_ERROR, tts_controller()->last_event_type_);
  EXPECT_EQ(-1, tts_controller()->last_length_);
  EXPECT_EQ("", tts_controller()->last_error_message_);
}

}  // namespace

}  // namespace arc
