// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tts/arc_tts_service.h"

#include <memory>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/speech/tts_chromeos.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class TestableTtsController : public content::TtsController {
 public:
  TestableTtsController() = default;

  TestableTtsController(const TestableTtsController&) = delete;
  TestableTtsController& operator=(const TestableTtsController&) = delete;

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
                 const GURL& source_url,
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
  void RefreshVoices() override {}
  void SetRemoteTtsEngineDelegate(
      content::RemoteTtsEngineDelegate* delegate) override {}
  void SetTtsPlatform(content::TtsPlatform* tts_platform) override {}
  int QueueSize() override { return 0; }
  void StripSSML(
      const std::string& utterance,
      base::OnceCallback<void(const std::string&)> callback) override {}
  void OnTtsUtteranceBecameInvalid(int utterance_id) override {}

  int last_utterance_id_;
  content::TtsEventType last_event_type_;
  int last_char_index_;
  int last_length_;
  std::string last_error_message_;
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

  ArcTtsServiceTest(const ArcTtsServiceTest&) = delete;
  ArcTtsServiceTest& operator=(const ArcTtsServiceTest&) = delete;

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
  const raw_ptr<ArcTtsService> tts_service_;
};

// Tests that ArcTtsService can be constructed and destructed.
TEST_F(ArcTtsServiceTest, TestConstructDestruct) {}

// Tests that OnTtsEvent() properly calls into
// TtsControllerDelegateImpl::OnTtsEvent().
TEST_F(ArcTtsServiceTest, TestOnTtsEvent) {
  tts_service()->OnTtsEvent(1, mojom::TtsEventType::START, 0, -1, "");
  EXPECT_EQ(1, tts_controller()->last_utterance_id_);
  EXPECT_EQ(content::TTS_EVENT_START, tts_controller()->last_event_type_);
  EXPECT_EQ(-1, tts_controller()->last_length_);
  EXPECT_EQ("", tts_controller()->last_error_message_);

  tts_service()->OnTtsEvent(1, mojom::TtsEventType::END, 10, 2, "");
  EXPECT_EQ(1, tts_controller()->last_utterance_id_);
  EXPECT_EQ(content::TTS_EVENT_END, tts_controller()->last_event_type_);
  EXPECT_EQ(2, tts_controller()->last_length_);
  EXPECT_EQ("", tts_controller()->last_error_message_);

  tts_service()->OnTtsEvent(2, mojom::TtsEventType::INTERRUPTED, 0, -1, "");
  EXPECT_EQ(2, tts_controller()->last_utterance_id_);
  EXPECT_EQ(content::TTS_EVENT_INTERRUPTED, tts_controller()->last_event_type_);
  EXPECT_EQ(-1, tts_controller()->last_length_);
  EXPECT_EQ("", tts_controller()->last_error_message_);

  tts_service()->OnTtsEvent(3, mojom::TtsEventType::ERROR, 0, 10, "");
  EXPECT_EQ(3, tts_controller()->last_utterance_id_);
  EXPECT_EQ(content::TTS_EVENT_ERROR, tts_controller()->last_event_type_);
  EXPECT_EQ(10, tts_controller()->last_length_);
  EXPECT_EQ("", tts_controller()->last_error_message_);
}

TEST_F(ArcTtsServiceTest, GetVoices) {
  std::vector<mojom::TtsVoicePtr> android_voices;

  // These voices are sorted so that those where
  // |is_network_connection_required| when false, come before those with that
  // field set to true.
  auto voice1 = mojom::TtsVoice::New();
  voice1->name = "voice1";
  voice1->locale = "en_US";
  voice1->is_network_connection_required = true;
  android_voices.push_back(std::move(voice1));

  auto voice0 = mojom::TtsVoice::New();
  voice0->name = "voice0";
  voice0->locale = "eng-usa";
  voice0->is_network_connection_required = false;
  android_voices.push_back(std::move(voice0));

  auto voice2 = mojom::TtsVoice::New();
  voice2->name = "voice2";
  voice2->locale = "FOO_bar";
  voice2->is_network_connection_required = true;
  android_voices.push_back(std::move(voice2));

  tts_service()->OnVoicesChanged(std::move(android_voices));

  TtsPlatformImplChromeOs* tts_chromeos =
      TtsPlatformImplChromeOs::GetInstance();
  std::vector<content::VoiceData> chrome_voices;
  tts_chromeos->GetVoices(&chrome_voices);

  EXPECT_EQ(3U, chrome_voices.size());
  EXPECT_EQ("voice0", chrome_voices[0].name);
  EXPECT_EQ("en-US", chrome_voices[0].lang);
  EXPECT_FALSE(chrome_voices[0].remote);

  EXPECT_EQ("voice1", chrome_voices[1].name);
  EXPECT_EQ("en-US", chrome_voices[1].lang);
  EXPECT_TRUE(chrome_voices[1].remote);

  EXPECT_EQ("voice2", chrome_voices[2].name);
  EXPECT_EQ("foo-BAR", chrome_voices[2].lang);
  EXPECT_TRUE(chrome_voices[2].remote);
}

}  // namespace

TEST_F(ArcTtsServiceTest, ChromeVoiceEvents) {
  std::vector<mojom::TtsVoicePtr> android_voices;
  auto voice0 = mojom::TtsVoice::New();
  android_voices.push_back(std::move(voice0));

  auto voice1 = mojom::TtsVoice::New();
  android_voices.push_back(std::move(voice1));

  tts_service()->OnVoicesChanged(std::move(android_voices));

  TtsPlatformImplChromeOs* tts_chromeos =
      TtsPlatformImplChromeOs::GetInstance();
  std::vector<content::VoiceData> chrome_voices;
  tts_chromeos->GetVoices(&chrome_voices);

  EXPECT_EQ(2U, chrome_voices.size());

  std::set<content::TtsEventType> expected_events(
      {content::TTS_EVENT_START, content::TTS_EVENT_END,
       content::TTS_EVENT_INTERRUPTED, content::TTS_EVENT_ERROR});
  EXPECT_EQ(expected_events, chrome_voices[0].events);
  EXPECT_EQ(expected_events, chrome_voices[1].events);

  chrome_voices.clear();
  tts_chromeos->ReceivedWordEvent();
  tts_chromeos->GetVoices(&chrome_voices);
  expected_events.insert(content::TTS_EVENT_WORD);

  EXPECT_EQ(expected_events, chrome_voices[0].events);
  EXPECT_EQ(expected_events, chrome_voices[1].events);

  // All events should have been mapped at runtime.
  EXPECT_EQ(static_cast<size_t>(mojom::TtsEventType::kMaxValue),
            expected_events.size() - 1);

  // Setting the voice again results in all Android-side mojo events besides
  // word.
  auto voice2 = mojom::TtsVoice::New();
  std::vector<mojom::TtsVoicePtr> more_android_voices;
  more_android_voices.push_back(std::move(voice2));
  tts_service()->OnVoicesChanged(std::move(more_android_voices));
  chrome_voices.clear();
  tts_chromeos->GetVoices(&chrome_voices);

  expected_events.erase(content::TTS_EVENT_WORD);
  EXPECT_EQ(1U, chrome_voices.size());
  EXPECT_EQ(expected_events, chrome_voices[0].events);
}

}  // namespace arc
