// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/extension_api/tts_extension_api.h"
#include "chrome/browser/speech/tts_controller_delegate_impl.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::_;

namespace {
int g_saved_utterance_id;
}

namespace extensions {

class MockTtsPlatformImpl : public content::TtsPlatform {
 public:
  MockTtsPlatformImpl() : should_fake_get_voices_(false) {}

  bool PlatformImplAvailable() override { return true; }

  void WillSpeakUtteranceWithVoice(
      content::TtsUtterance* utterance,
      const content::VoiceData& voice_data) override {}

  bool LoadBuiltInTtsEngine(content::BrowserContext* browser_context) override {
    return false;
  }

  void ClearError() override { error_ = ""; }

  void SetError(const std::string& error) override { error_ = error; }

  std::string GetError() override { return error_; }

  // Work-around for functions that take move-only types as arguments.
  // https://github.com/google/googlemock/blob/master/googlemock/docs/CookBook.md
  // Delegate the Speak() method to DoSpeak(), which doesn't take any move
  // parameters.
  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const content::VoiceData& voice,
             const content::UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override {
    DoSpeak(utterance_id, utterance, lang, voice, params);
    // Logic for PlatformSpeakError test. Needs to fail the first time, but
    // succeed the second time.
    if (speak_error_test_) {
      speak_error_count_ > 0 ? std::move(on_speak_finished).Run(true)
                             : std::move(on_speak_finished).Run(false);
      ++speak_error_count_;
    } else {
      std::move(on_speak_finished).Run(true);
    }
  }
  MOCK_METHOD5(DoSpeak,
               void(int utterance_id,
                    const std::string& utterance,
                    const std::string& lang,
                    const content::VoiceData& voice,
                    const content::UtteranceContinuousParameters& params));

  MOCK_METHOD0(StopSpeaking, bool(void));

  MOCK_METHOD0(Pause, void(void));

  MOCK_METHOD0(Resume, void(void));

  MOCK_METHOD0(IsSpeaking, bool(void));

  // Fake this method to add a native voice.
  void GetVoices(std::vector<content::VoiceData>* voices) override {
    if (!should_fake_get_voices_)
      return;

    content::VoiceData voice;
    voice.name = "TestNativeVoice";
    voice.native = true;
    voice.lang = "en-GB";
    voice.events.insert(content::TTS_EVENT_START);
    voice.events.insert(content::TTS_EVENT_END);
    voices->push_back(voice);
  }

  void set_should_fake_get_voices(bool val) { should_fake_get_voices_ = val; }

  void SetErrorToEpicFail() { SetError("epic fail"); }

  void SendEndEventOnSavedUtteranceId() {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MockTtsPlatformImpl::SendEvent,
                       ptr_factory_.GetWeakPtr(), false, g_saved_utterance_id,
                       content::TTS_EVENT_END, 0, 0, std::string()),
        base::TimeDelta());
  }

  void SendEndEvent(int utterance_id,
                    const std::string& utterance,
                    const std::string& lang,
                    const content::VoiceData& voice,
                    const content::UtteranceContinuousParameters& params) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MockTtsPlatformImpl::SendEvent,
                       ptr_factory_.GetWeakPtr(), false, utterance_id,
                       content::TTS_EVENT_END, utterance.size(), 0,
                       std::string()),
        base::TimeDelta());
  }

  void SendEndEventWhenQueueNotEmpty(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const content::VoiceData& voice,
      const content::UtteranceContinuousParameters& params) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MockTtsPlatformImpl::SendEvent,
                       ptr_factory_.GetWeakPtr(), true, utterance_id,
                       content::TTS_EVENT_END, utterance.size(), 0,
                       std::string()),
        base::TimeDelta());
  }

  void SendWordEvents(int utterance_id,
                      const std::string& utterance,
                      const std::string& lang,
                      const content::VoiceData& voice,
                      const content::UtteranceContinuousParameters& params) {
    for (int i = 0; i < static_cast<int>(utterance.size()); i++) {
      if (i == 0 || utterance[i - 1] == ' ') {
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&MockTtsPlatformImpl::SendEvent,
                           ptr_factory_.GetWeakPtr(), false, utterance_id,
                           content::TTS_EVENT_WORD, i, 1, std::string()),
            base::TimeDelta());
      }
    }
  }

  void SendEvent(bool wait_for_non_empty_queue,
                 int utterance_id,
                 content::TtsEventType event_type,
                 int char_index,
                 int length,
                 const std::string& message) {
    content::TtsController* tts_controller =
        content::TtsController::GetInstance();
    if (wait_for_non_empty_queue && tts_controller->QueueSize() == 0) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&MockTtsPlatformImpl::SendEvent,
                         ptr_factory_.GetWeakPtr(), true, utterance_id,
                         event_type, char_index, length, message),
          base::TimeDelta::FromMilliseconds(100));
      return;
    }

    tts_controller->OnTtsEvent(utterance_id, event_type, char_index, length,
                               message);
  }

  void SetSpeakErrorTest(bool value) { speak_error_test_ = value; }

 private:
  bool speak_error_test_ = false;
  int speak_error_count_ = 0;
  bool should_fake_get_voices_;
  std::string error_;
  base::WeakPtrFactory<MockTtsPlatformImpl> ptr_factory_{this};
};

class FakeNetworkOnlineStateForTest : public net::NetworkChangeNotifier {
 public:
  explicit FakeNetworkOnlineStateForTest(bool online) : online_(online) {}
  ~FakeNetworkOnlineStateForTest() override {}

  ConnectionType GetCurrentConnectionType() const override {
    return online_ ?
        net::NetworkChangeNotifier::CONNECTION_ETHERNET :
        net::NetworkChangeNotifier::CONNECTION_NONE;
  }

 private:
  bool online_;
  DISALLOW_COPY_AND_ASSIGN(FakeNetworkOnlineStateForTest);
};

class EventRouterAddListenerWaiter : public EventRouter::Observer {
 public:
  EventRouterAddListenerWaiter(Profile* profile, const std::string& event_name)
      : event_router_(EventRouter::EventRouter::Get(profile)) {
    DCHECK(profile);
    event_router_->RegisterObserver(this, event_name);
  }

  ~EventRouterAddListenerWaiter() override {
    event_router_->UnregisterObserver(this);
  }

  void Wait() { loop_runner_.Run(); }

  // EventRouter::Observer overrides.
  void OnListenerAdded(const EventListenerInfo& details) override {
    loop_runner_.Quit();
  }

 private:
  EventRouter* const event_router_;
  base::RunLoop loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(EventRouterAddListenerWaiter);
};

class TtsApiTest : public ExtensionApiTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    content::TtsController* tts_controller =
        content::TtsController::GetInstance();
    tts_controller->SetTtsPlatform(&mock_platform_impl_);
    tts_controller->SetTtsEngineDelegate(TtsExtensionEngine::GetInstance());
  }

  void AddNetworkSpeechSynthesisExtension() {
    content::WindowedNotificationObserver observer(
        NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
        content::NotificationService::AllSources());
    ExtensionService* service =
        extensions::ExtensionSystem::Get(profile())->extension_service();
    service->component_loader()->AddNetworkSpeechSynthesisExtension();
    observer.Wait();
    ASSERT_EQ(Manifest::COMPONENT,
              content::Source<const Extension>(observer.source())->location());
  }

 protected:
  bool HasVoiceWithName(const std::string& name) {
    std::vector<content::VoiceData> voices;
    content::TtsController::GetInstance()->GetVoices(profile(), &voices);
    for (auto& voice : voices) {
      if (voice.name == name)
        return true;
    }

    return false;
  }

  StrictMock<MockTtsPlatformImpl> mock_platform_impl_;
};

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakOptionalArgs) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());

  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "", _, _, _)).WillOnce(Return());
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "Alpha", _, _, _))
      .WillOnce(Return());
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "Bravo", _, _, _))
      .WillOnce(Return());
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "Charlie", _, _, _))
      .WillOnce(Return());
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "Echo", _, _, _))
      .WillOnce(Return());
  ASSERT_TRUE(RunExtensionTest("tts/optional_args")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakFinishesImmediately) {
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, _, _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_, &MockTtsPlatformImpl::SendEndEvent),
          Return()));
  ASSERT_TRUE(RunExtensionTest("tts/speak_once")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakInterrupt) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());

  // One utterance starts speaking, and then a second interrupts.
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "text 1", _, _, _))
      .WillOnce(Return());
  // Expect the second utterance and allow it to finish.
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "text 2", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_, &MockTtsPlatformImpl::SendEndEvent),
          Return()));
  ASSERT_TRUE(RunExtensionTest("tts/interrupt")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakQueueInterrupt) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());

  // In this test, two utterances are queued, and then a third
  // interrupts. Speak(, _) never gets called on the second utterance.
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "text 1", _, _, _))
      .WillOnce(Return());
  // Don't expect the second utterance, because it's queued up and the
  // first never finishes.
  // Expect the third utterance and allow it to finish successfully.
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "text 3", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_, &MockTtsPlatformImpl::SendEndEvent),
          Return()));
  ASSERT_TRUE(RunExtensionTest("tts/queue_interrupt")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakEnqueue) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());

  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "text 1", _, _, _))
      .WillOnce(
          DoAll(Invoke(&mock_platform_impl_,
                       &MockTtsPlatformImpl::SendEndEventWhenQueueNotEmpty),
                Return()));
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "text 2", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_, &MockTtsPlatformImpl::SendEndEvent),
          Return()));
  ASSERT_TRUE(RunExtensionTest("tts/enqueue")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakError) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .Times(AnyNumber());

  mock_platform_impl_.SetSpeakErrorTest(true);

  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "first try", _, _, _))
      .WillOnce(
          DoAll(InvokeWithoutArgs(&mock_platform_impl_,
                                  &MockTtsPlatformImpl::SetErrorToEpicFail),
                Return()));
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));

  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "second try", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_, &MockTtsPlatformImpl::SendEndEvent),
          Return()));
  ASSERT_TRUE(RunExtensionTest("tts/speak_error")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformWordCallbacks) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());

  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "one two three", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_, &MockTtsPlatformImpl::SendWordEvents),
          Invoke(&mock_platform_impl_, &MockTtsPlatformImpl::SendEndEvent),
          Return()));
  ASSERT_TRUE(RunExtensionTest("tts/word_callbacks")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformPauseResume) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .Times(AnyNumber());

  InSequence s;
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "test 1", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_, &MockTtsPlatformImpl::SendEndEvent),
          Return()));
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "test 2", _, _, _))
      .WillOnce(DoAll(SaveArg<0>(&g_saved_utterance_id), Return()));
  EXPECT_CALL(mock_platform_impl_, Pause());
  EXPECT_CALL(mock_platform_impl_, Resume())
      .WillOnce(
          InvokeWithoutArgs(
              &mock_platform_impl_,
              &MockTtsPlatformImpl::SendEndEventOnSavedUtteranceId));
  ASSERT_TRUE(RunExtensionTest("tts/pause_resume")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformPauseSpeakNoEnqueue) {
  // While paused, one utterance is enqueued, and then a second utterance that
  // cannot be enqueued cancels both.
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking()).WillOnce(Return(true));
  ASSERT_TRUE(RunExtensionTest("tts/pause_speak_no_enqueue")) << message_;
}

//
// TTS Engine tests.
//

IN_PROC_BROWSER_TEST_F(TtsApiTest, RegisterEngine) {
  mock_platform_impl_.set_should_fake_get_voices(true);

  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .Times(AnyNumber());
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillRepeatedly(Return(true));

  {
    InSequence s;
    EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "native speech", _, _, _))
        .WillOnce(DoAll(
            Invoke(&mock_platform_impl_, &MockTtsPlatformImpl::SendEndEvent),
            Return()));
    EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "native speech 2", _, _, _))
        .WillOnce(DoAll(
            Invoke(&mock_platform_impl_, &MockTtsPlatformImpl::SendEndEvent),
            Return()));
    EXPECT_CALL(mock_platform_impl_, DoSpeak(_, "native speech 3", _, _, _))
        .WillOnce(DoAll(
            Invoke(&mock_platform_impl_, &MockTtsPlatformImpl::SendEndEvent),
            Return()));
  }

  // TODO(katie): Expect the deprecated gender warning rather than ignoring
  // warnings.
  ASSERT_TRUE(RunExtensionTestWithFlags("tts_engine/register_engine",
                                        kFlagIgnoreManifestWarnings))
      << message_;
}

// https://crbug.com/709115 tracks test flakiness.
#if defined(OS_POSIX)
#define MAYBE_EngineError DISABLED_EngineError
#else
#define MAYBE_EngineError EngineError
#endif
IN_PROC_BROWSER_TEST_F(TtsApiTest, MAYBE_EngineError) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(RunExtensionTest("tts_engine/engine_error")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, EngineWordCallbacks) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(RunExtensionTest("tts_engine/engine_word_callbacks")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, LangMatching) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(RunExtensionTest("tts_engine/lang_matching")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, NetworkSpeechEngine) {
  // Simulate online network state.
  net::NetworkChangeNotifier::DisableForTest disable_for_test;
  FakeNetworkOnlineStateForTest fake_online_state(true);

  ASSERT_NO_FATAL_FAILURE(AddNetworkSpeechSynthesisExtension());
  ASSERT_TRUE(RunExtensionTest("tts_engine/network_speech_engine")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, NoNetworkSpeechEngineWhenOffline) {
  // Simulate offline network state.
  net::NetworkChangeNotifier::DisableForTest disable_for_test;
  FakeNetworkOnlineStateForTest fake_online_state(false);

  ASSERT_NO_FATAL_FAILURE(AddNetworkSpeechSynthesisExtension());
  // Test should fail when offline.
  ASSERT_FALSE(RunExtensionTest("tts_engine/network_speech_engine"));
}

// http://crbug.com/122474
IN_PROC_BROWSER_TEST_F(TtsApiTest, EngineApi) {
  ASSERT_TRUE(RunExtensionTest("tts_engine/engine_api")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, UpdateVoicesApi) {
  ASSERT_TRUE(RunExtensionTest("tts_engine/update_voices_api")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PRE_VoicesAreCached) {
  EXPECT_FALSE(HasVoiceWithName("Dynamic Voice 1"));
  EXPECT_FALSE(HasVoiceWithName("Dynamic Voice 2"));
  ASSERT_TRUE(RunExtensionTest("tts_engine/call_update_voices")) << message_;
  EXPECT_TRUE(HasVoiceWithName("Dynamic Voice 1"));
  EXPECT_TRUE(HasVoiceWithName("Dynamic Voice 2"));
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, VoicesAreCached) {
  // Make sure the dynamically loaded voices are available even though
  // the extension didn't "run". Note that the voices might not be available
  // immediately when the test runs, but the test should pass shortly after
  // the extension's event listeners are registered.
  while (!HasVoiceWithName("Dynamic Voice 1") ||
         !HasVoiceWithName("Dynamic Voice 2")) {
    // Wait for the extension's event listener to be registered before
    // checking what voices are registered.
    EventRouterAddListenerWaiter waiter(profile(), tts_engine_events::kOnStop);
    waiter.Wait();
  }
}

}  // namespace extensions
