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
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/speech/tts_platform.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::CreateFunctor;
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

class MockTtsPlatformImpl : public TtsPlatformImpl {
 public:
  MockTtsPlatformImpl()
      : should_fake_get_voices_(false),
        ptr_factory_(this) {}

  bool PlatformImplAvailable() override { return true; }

  MOCK_METHOD5(Speak,
               bool(int utterance_id,
                    const std::string& utterance,
                    const std::string& lang,
                    const VoiceData& voice,
                    const UtteranceContinuousParameters& params));

  MOCK_METHOD0(StopSpeaking, bool(void));

  MOCK_METHOD0(Pause, void(void));

  MOCK_METHOD0(Resume, void(void));

  MOCK_METHOD0(IsSpeaking, bool(void));

  // Fake this method to add a native voice.
  void GetVoices(std::vector<VoiceData>* voices) override {
    if (!should_fake_get_voices_)
      return;

    VoiceData voice;
    voice.name = "TestNativeVoice";
    voice.native = true;
    voice.lang = "en-GB";
    voice.events.insert(TTS_EVENT_START);
    voice.events.insert(TTS_EVENT_END);
    voices->push_back(voice);
  }

  void set_should_fake_get_voices(bool val) { should_fake_get_voices_ = val; }

  void SetErrorToEpicFail() {
    set_error("epic fail");
  }

  void SendEndEventOnSavedUtteranceId() {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&MockTtsPlatformImpl::SendEvent, ptr_factory_.GetWeakPtr(),
                   false, g_saved_utterance_id, TTS_EVENT_END, 0,
                   std::string()),
        base::TimeDelta());
  }

  void SendEndEvent(int utterance_id,
                    const std::string& utterance,
                    const std::string& lang,
                    const VoiceData& voice,
                    const UtteranceContinuousParameters& params) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&MockTtsPlatformImpl::SendEvent,
                              ptr_factory_.GetWeakPtr(), false, utterance_id,
                              TTS_EVENT_END, utterance.size(), std::string()),
        base::TimeDelta());
  }

  void SendEndEventWhenQueueNotEmpty(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const VoiceData& voice,
      const UtteranceContinuousParameters& params) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&MockTtsPlatformImpl::SendEvent,
                              ptr_factory_.GetWeakPtr(), true, utterance_id,
                              TTS_EVENT_END, utterance.size(), std::string()),
        base::TimeDelta());
  }

  void SendWordEvents(int utterance_id,
                      const std::string& utterance,
                      const std::string& lang,
                      const VoiceData& voice,
                      const UtteranceContinuousParameters& params) {
    for (int i = 0; i < static_cast<int>(utterance.size()); i++) {
      if (i == 0 || utterance[i - 1] == ' ') {
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::Bind(&MockTtsPlatformImpl::SendEvent,
                       ptr_factory_.GetWeakPtr(), false, utterance_id,
                       TTS_EVENT_WORD, i, std::string()),
            base::TimeDelta());
      }
    }
  }

  void SendEvent(bool wait_for_non_empty_queue,
                 int utterance_id,
                 TtsEventType event_type,
                 int char_index,
                 const std::string& message) {
    TtsController* controller = TtsController::GetInstance();
    if (wait_for_non_empty_queue && controller->QueueSize() == 0) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&MockTtsPlatformImpl::SendEvent, ptr_factory_.GetWeakPtr(),
                     true, utterance_id, event_type, char_index, message),
          base::TimeDelta::FromMilliseconds(100));
      return;
    }

    controller->OnTtsEvent(utterance_id, event_type, char_index, message);
  }

 private:
  bool should_fake_get_voices_;
  base::WeakPtrFactory<MockTtsPlatformImpl> ptr_factory_;
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
    TtsController::GetInstance()->SetPlatformImpl(&mock_platform_impl_);
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
    std::vector<VoiceData> voices;
    TtsController::GetInstance()->GetVoices(profile(), &voices);
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
  EXPECT_CALL(mock_platform_impl_, Speak(_, "", _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "Alpha", _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "Bravo", _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "Charlie", _, _, _))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "Echo", _, _, _))
      .WillOnce(Return(true));
  ASSERT_TRUE(RunExtensionTest("tts/optional_args")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakFinishesImmediately) {
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, _, _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
  ASSERT_TRUE(RunExtensionTest("tts/speak_once")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakInterrupt) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());

  // One utterance starts speaking, and then a second interrupts.
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "text 1", _, _, _))
      .WillOnce(Return(true));
  // Expect the second utterance and allow it to finish.
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "text 2", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
  ASSERT_TRUE(RunExtensionTest("tts/interrupt")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakQueueInterrupt) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());

  // In this test, two utterances are queued, and then a third
  // interrupts. Speak(, _) never gets called on the second utterance.
  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "text 1", _, _, _))
      .WillOnce(Return(true));
  // Don't expect the second utterance, because it's queued up and the
  // first never finishes.
  // Expect the third utterance and allow it to finish successfully.
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "text 3", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
  ASSERT_TRUE(RunExtensionTest("tts/queue_interrupt")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakEnqueue) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());

  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "text 1", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEventWhenQueueNotEmpty),
          Return(true)));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "text 2", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
  ASSERT_TRUE(RunExtensionTest("tts/enqueue")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformSpeakError) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .Times(AnyNumber());

  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "first try", _, _, _))
      .WillOnce(DoAll(
          InvokeWithoutArgs(
              CreateFunctor(&MockTtsPlatformImpl::SetErrorToEpicFail,
                            base::Unretained(&mock_platform_impl_))),
          Return(false)));
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "second try", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
  ASSERT_TRUE(RunExtensionTest("tts/speak_error")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformWordCallbacks) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking());

  InSequence s;
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "one two three", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendWordEvents),
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
  ASSERT_TRUE(RunExtensionTest("tts/word_callbacks")) << message_;
}

IN_PROC_BROWSER_TEST_F(TtsApiTest, PlatformPauseResume) {
  EXPECT_CALL(mock_platform_impl_, IsSpeaking())
      .Times(AnyNumber());

  InSequence s;
  EXPECT_CALL(mock_platform_impl_, Speak(_, "test 1", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
  EXPECT_CALL(mock_platform_impl_, StopSpeaking())
      .WillOnce(Return(true));
  EXPECT_CALL(mock_platform_impl_, Speak(_, "test 2", _, _, _))
      .WillOnce(DoAll(
          SaveArg<0>(&g_saved_utterance_id),
          Return(true)));
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
    EXPECT_CALL(mock_platform_impl_, Speak(_, "native speech", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
    EXPECT_CALL(mock_platform_impl_, Speak(_, "native speech 2", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
    EXPECT_CALL(mock_platform_impl_, Speak(_, "native speech 3", _, _, _))
      .WillOnce(DoAll(
          Invoke(&mock_platform_impl_,
                 &MockTtsPlatformImpl::SendEndEvent),
          Return(true)));
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
