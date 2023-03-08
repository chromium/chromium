// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/tts_crosapi_util.h"
#include "chrome/browser/speech/tts_lacros.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/crosapi/mojom/tts.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/lacros/lacros_test_helper.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/test_extension_registry_observer.h"

namespace extensions {

namespace {

void GiveItSomeTime(base::TimeDelta delta) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delta);
  run_loop.Run();
}

}  // namespace

class LacrosTtsApiTest : public ExtensionApiTest,
                         public content::VoicesChangedDelegate {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    content::TtsController::SkipAddNetworkChangeObserverForTests(true);
    content::TtsController* tts_controller =
        content::TtsController::GetInstance();
    tts_controller->SetTtsEngineDelegate(TtsExtensionEngine::GetInstance());
    TtsPlatformImplLacros::EnablePlatformSupportForTesting();
  }

 protected:
  bool HasVoiceWithName(const std::string& name) {
    std::vector<content::VoiceData> voices;
    content::TtsController::GetInstance()->GetVoices(profile(), GURL(),
                                                     &voices);
    for (auto& voice : voices) {
      if (voice.name == name)
        return true;
    }

    return false;
  }

  // content::VoicesChangedDelegate:
  void OnVoicesChanged() override {
    voices_changed_ = true;
    std::vector<content::VoiceData> voices;
    content::TtsController::GetInstance()->GetVoices(profile(), GURL(),
                                                     &voices);
  }

  bool VoicesChangedNotified() { return voices_changed_; }
  void ResetVoicesChanged() { voices_changed_ = false; }

  void WaitUntilVoicesLoaded(const std::string& voice_name) {
    while (!HasVoiceWithName(voice_name)) {
      GiveItSomeTime(base::Milliseconds(100));
    }
  }

  void WaitUntilVoicesUnloaded(const std::string& voice_name) {
    while (HasVoiceWithName(voice_name)) {
      GiveItSomeTime(base::Milliseconds(100));
    }
  }

  // Returns true if the Tts utterance queue of TtsController running in Ash is
  // empty.
  bool IsUtteranceQueueEmpty() const {
    crosapi::mojom::TestControllerAsyncWaiter waiter(
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::TestController>()
            .get());
    return waiter.GetTtsUtteranceQueueSize() == 0;
  }

  bool FoundVoiceInMojoVoices(
      const std::string& voice_name,
      const std::vector<crosapi::mojom::TtsVoicePtr>& mojo_voices) {
    for (const auto& voice : mojo_voices) {
      if (voice_name == voice->voice_name)
        return true;
    }
    return false;
  }

  void WaitUntilTtsEventReceivedByUtteranceEventDelegateInAsh() {
    while (!tts_event_notified_in_ash_) {
      GiveItSomeTime(base::Milliseconds(100));
    }
  }

  void NotifyTtsEventReceivedInAsh(content::TtsEventType tts_event) {
    tts_event_notified_in_ash_ = true;
    tts_event_received_ = tts_event;
  }

  bool TtsEventNotifiedInAsh() { return tts_event_notified_in_ash_; }
  bool TtsEventReceivedEq(content::TtsEventType expected_tts_event) {
    return tts_event_received_ == expected_tts_event;
  }

  // Used to verify that the TtsEvent is received by the Ash utterance's
  // UtteranceEventDelegate in Ash.
  class EventDelegate : public crosapi::mojom::TtsUtteranceClient {
   public:
    explicit EventDelegate(extensions::LacrosTtsApiTest* owner)
        : owner_(owner) {}
    EventDelegate(const EventDelegate&) = delete;
    EventDelegate& operator=(const EventDelegate&) = delete;
    ~EventDelegate() override = default;

    // crosapi::mojom::TtsUtteranceClient:
    void OnTtsEvent(crosapi::mojom::TtsEventType mojo_tts_event,
                    uint32_t char_index,
                    uint32_t char_length,
                    const std::string& error_message) override {
      content::TtsEventType tts_event =
          tts_crosapi_util::FromMojo(mojo_tts_event);
      owner_->NotifyTtsEventReceivedInAsh(tts_event);
    }

    mojo::PendingRemote<crosapi::mojom::TtsUtteranceClient>
    BindTtsUtteranceClient() {
      return receiver_.BindNewPipeAndPassRemoteWithVersion();
    }

   private:
    extensions::LacrosTtsApiTest* owner_;
    mojo::Receiver<crosapi::mojom::TtsUtteranceClient> receiver_{this};
  };

 private:
  bool voices_changed_ = false;
  bool tts_event_notified_in_ash_ = false;
  content::TtsEventType tts_event_received_;
};

//
// TTS Engine tests.
//
IN_PROC_BROWSER_TEST_F(LacrosTtsApiTest, LoadAndUnloadLacrosTtsEngine) {
  // Before tts engine extension is loaded, verify the internal states are
  // clean.
  EXPECT_FALSE(VoicesChangedNotified());
  EXPECT_FALSE(HasVoiceWithName("Alice"));
  EXPECT_FALSE(HasVoiceWithName("Pat"));
  EXPECT_FALSE(HasVoiceWithName("Cat"));

  // Load tts engine extension and register the tts engine events.
  content::TtsController::GetInstance()->AddVoicesChangedDelegate(this);
  ASSERT_TRUE(RunExtensionTest("tts_engine/lacros_register_engine", {},
                               {.ignore_manifest_warnings = true}))
      << message_;

  // Wait until Lacros gets the voices registered by the tts engine extension.
  WaitUntilVoicesLoaded("Alice");

  // Verify TtsController notifies VoicesChangedDelegate for the voices change.
  EXPECT_TRUE(VoicesChangedNotified());

  // Verify all the voices from tts engine extension are returned by
  // TtsController::GetVoices().
  std::vector<content::VoiceData> voices;
  content::TtsController::GetInstance()->GetVoices(profile(), GURL(), &voices);
  EXPECT_TRUE(HasVoiceWithName("Alice"));
  EXPECT_TRUE(HasVoiceWithName("Pat"));
  EXPECT_TRUE(HasVoiceWithName("Cat"));

  ResetVoicesChanged();

  // Uninstall tts engine extension.
  extensions::TestExtensionRegistryObserver observer(
      extensions::ExtensionRegistry::Get(profile()),
      last_loaded_extension_id());
  UninstallExtension(last_loaded_extension_id());
  observer.WaitForExtensionUninstalled();

  WaitUntilVoicesUnloaded("Alice");

  // Verify TtsController notifies VoicesChangedDelegate for the voices
  // change.
  EXPECT_TRUE(VoicesChangedNotified());

  // Verify the voices from the tts engine extensions are unloaded in Lacros
  // TtsController.
  EXPECT_FALSE(HasVoiceWithName("Alice"));
  EXPECT_FALSE(HasVoiceWithName("Pat"));
  EXPECT_FALSE(HasVoiceWithName("Cat"));
}

IN_PROC_BROWSER_TEST_F(LacrosTtsApiTest,
                       SpeakLacrosUtteranceWithLacrosTtsEngine) {
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::TestController::Uuid_) <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kGetTtsUtteranceQueueSizeMinVersion)) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  // Load tts engine extension, register the tts engine events and call
  // tts.speak from the testing extension.
  ASSERT_TRUE(
      RunExtensionTest("tts_engine/lacros_tts_support/"
                       "tts_speak_lacros_utterance_with_lacros_engine",
                       {}, {.ignore_manifest_warnings = true}))
      << message_;

  // Verify the utterance issued from the testing extension is properly
  // finished and the utterance queue is empty in Ash's TtsController.
  ASSERT_TRUE(IsUtteranceQueueEmpty());
}

IN_PROC_BROWSER_TEST_F(LacrosTtsApiTest,
                       SpeakAshUtteranceWithLacrosSpeechEngine) {
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::TestController::Uuid_) <
      static_cast<int>(crosapi::mojom::TestController::MethodMinVersions::
                           kGetTtsVoicesMinVersion)) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  EXPECT_FALSE(VoicesChangedNotified());
  EXPECT_FALSE(HasVoiceWithName("Alice"));
  EXPECT_FALSE(HasVoiceWithName("Alex"));
  EXPECT_FALSE(HasVoiceWithName("Amanda"));

  ResetVoicesChanged();
  content::TtsController::GetInstance()->AddVoicesChangedDelegate(this);

  // Load speech engine extension in Lacros.
  ASSERT_TRUE(
      RunExtensionTest("tts_engine/lacros_tts_support/"
                       "register_lacros_tts_engine",
                       {}, {.ignore_manifest_warnings = true}))
      << message_;

  WaitUntilVoicesLoaded("Alice");

  EXPECT_TRUE(VoicesChangedNotified());

  // Verify all the voices from tts engine extension are laded in Lacros.
  std::vector<content::VoiceData> voices;
  content::TtsController::GetInstance()->GetVoices(profile(), GURL(), &voices);
  EXPECT_TRUE(HasVoiceWithName("Alice"));
  EXPECT_TRUE(HasVoiceWithName("Alex"));
  EXPECT_TRUE(HasVoiceWithName("Amanda"));
  // Verify a random dummy voice is not loaded.
  EXPECT_FALSE(HasVoiceWithName("Tommy"));

  // Verify the same voices are also loaded in Ash.
  crosapi::mojom::TestControllerAsyncWaiter waiter(
      chromeos::LacrosService::Get()
          ->GetRemote<crosapi::mojom::TestController>()
          .get());

  std::vector<crosapi::mojom::TtsVoicePtr> mojo_voices;
  while (mojo_voices.size() == 0) {
    waiter.GetTtsVoices(&mojo_voices);
    if (mojo_voices.size() > 0)
      break;
    GiveItSomeTime(base::Milliseconds(100));
  }

  EXPECT_TRUE(FoundVoiceInMojoVoices("Alice", mojo_voices));
  EXPECT_TRUE(FoundVoiceInMojoVoices("Alex", mojo_voices));
  EXPECT_TRUE(FoundVoiceInMojoVoices("Amanda", mojo_voices));
  // Verify a random dummy voice is not loaded.
  EXPECT_FALSE(FoundVoiceInMojoVoices("Tommy", mojo_voices));

  std::unique_ptr<content::TtsUtterance> ash_utterance =
      content::TtsUtterance::Create();
  ash_utterance->SetText("Hello from Ash");
  // Use a voice provided by Lacros speech engine to speak the ash utterance.
  ash_utterance->SetVoiceName("Alice");
  crosapi::mojom::TtsUtterancePtr mojo_utterance =
      tts_crosapi_util::ToMojo(ash_utterance.get());
  // Note: mojo_utterance requires a value for browser_context_id, but it will
  // not used in Ash for the testing case.
  mojo_utterance->browser_context_id = base::UnguessableToken::Create();
  auto pending_client = std::make_unique<EventDelegate>(this);
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::TestController>()
      ->TtsSpeak(std::move(mojo_utterance),
                 pending_client->BindTtsUtteranceClient());

  // Verify that the Tts event has been received by the UtteranceEventDelegate
  // in Ash.
  WaitUntilTtsEventReceivedByUtteranceEventDelegateInAsh();
  EXPECT_TRUE(TtsEventNotifiedInAsh());
  EXPECT_TRUE(TtsEventReceivedEq(content::TTS_EVENT_END));

  // Verify the utterance issued from the testing extension is properly
  // finished and the utterance queue is empty in Ash's TtsController.
  ASSERT_TRUE(IsUtteranceQueueEmpty());
}

IN_PROC_BROWSER_TEST_F(LacrosTtsApiTest,
                       StopLacrosUtteranceWithLacrosTtsEngine) {
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::Tts::Uuid_) <
      static_cast<int>(crosapi::mojom::Tts::kStopMinVersion)) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  // Load tts engine extension, register the tts engine events and
  // call tts.speak and tts.stop from the testing extension.
  ASSERT_TRUE(
      RunExtensionTest("tts_engine/lacros_tts_support/"
                       "tts_stop_lacros_engine",
                       {}, {.ignore_manifest_warnings = true}))
      << message_;

  // Verify the utterance issued from the testing extension is properly
  // finished and the utterance queue is empty in Ash's TtsController.
  ASSERT_TRUE(IsUtteranceQueueEmpty());
}

IN_PROC_BROWSER_TEST_F(LacrosTtsApiTest, PauseBeforeSpeakWithLacrosTtsEngine) {
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::Tts::Uuid_) <
      static_cast<int>(crosapi::mojom::Tts::kPauseMinVersion)) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  // Load Lacros tts engine extension, register the tts engine events, and
  // call tts.pause, then tts.speak, tts.resume from the testing extension.
  ASSERT_TRUE(
      RunExtensionTest("tts_engine/lacros_tts_support/"
                       "tts_pause_before_speak_lacros_engine",
                       {}, {.ignore_manifest_warnings = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(LacrosTtsApiTest, PauseDuringSpeakWithLacrosTtsEngine) {
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::Tts::Uuid_) <
      static_cast<int>(crosapi::mojom::Tts::kPauseMinVersion)) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  // Load Lacros tts engine extension, register the tts engine events, and
  // call tts.speak, then tts.pause, tts.resume from the testing extension.
  ASSERT_TRUE(
      RunExtensionTest("tts_engine/lacros_tts_support/"
                       "tts_pause_during_speak_lacros_engine",
                       {}, {.ignore_manifest_warnings = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(LacrosTtsApiTest, IsSpeaking) {
  if (chromeos::LacrosService::Get()->GetInterfaceVersion(
          crosapi::mojom::Tts::Uuid_) <
      static_cast<int>(crosapi::mojom::Tts::kIsSpeakingMinVersion)) {
    GTEST_SKIP() << "Unsupported ash version.";
  }

  // Load Lacros tts engine extension, register the tts engine events, and
  // call tts.isSpeaking before/during/after tts.speak.
  ASSERT_TRUE(RunExtensionTest("tts/is_speaking/",
                               {}, {.ignore_manifest_warnings = true}))
      << message_;
}

}  // namespace extensions
