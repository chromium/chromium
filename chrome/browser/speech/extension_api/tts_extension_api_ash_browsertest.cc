// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crosapi/ash_requires_lacros_extension_apitest.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_api.h"
#include "chrome/browser/speech/tts_crosapi_util.h"
#include "chromeos/crosapi/mojom/test_controller.mojom-test-utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/crosapi/mojom/tts.mojom-forward.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_utterance.h"
#include "content/public/test/browser_test.h"

using crosapi::AshRequiresLacrosExtensionApiTest;

namespace {

void GiveItSomeTime(base::TimeDelta delta) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delta);
  run_loop.Run();
}

}  // namespace

namespace extensions {

// Test tts and ttsEngine APIs with Lacros Tts support enabled, which
// requires Lacros running to exercise crosapi calls.
class AshTtsApiTest : public AshRequiresLacrosExtensionApiTest,
                      public content::VoicesChangedDelegate {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    AshRequiresLacrosExtensionApiTest::SetUpInProcessBrowserTestFixture();

    // Enable Lacros tts support feature, and disable the 1st party Ash
    // extension keeplist feature so that it will allow loading test extension
    // in Ash in Lacros only mode.
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatures(
        {}, {ash::features::kEnforceAshExtensionKeeplist,
             ash::features::kDisableLacrosTtsSupport});

    content::TtsController::SkipAddNetworkChangeObserverForTests(true);
    content::TtsController* tts_controller =
        content::TtsController::GetInstance();
    TtsExtensionEngine::GetInstance()->DisableBuiltInTTSEngineForTesting();
    tts_controller->SetTtsEngineDelegate(TtsExtensionEngine::GetInstance());
  }

  void TearDownInProcessBrowserTestFixture() override {
    scoped_feature_list_.reset(nullptr);
  }

 protected:
  bool HasVoiceWithName(const std::string& name) {
    std::vector<content::VoiceData> voices;

    content::TtsController::GetInstance()->GetVoices(profile(), GURL(),
                                                     &voices);

    for (const auto& voice : voices) {
      if (voice.name == name)
        return true;
    }

    return false;
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

  // content::VoicesChangedDelegate:
  void OnVoicesChanged() override {
    voices_changed_ = true;
    std::vector<content::VoiceData> voices;
    content::TtsController::GetInstance()->GetVoices(profile(), GURL(),
                                                     &voices);
    expected_voice_loaded_ = false;
    for (const auto& voice : voices) {
      if (voice.name == "Amy") {
        expected_voice_loaded_ = true;
        break;
      }
    }
  }

  void WaitUntilVoicesLoaded() {
    while (!expected_voice_loaded_) {
      GiveItSomeTime(base::Milliseconds(100));
    }
  }

  void WaitUntilTtsEventReceivedByLacrosUtteranceEventDelegate() {
    while (!tts_event_notified_in_lacros_) {
      GiveItSomeTime(base::Milliseconds(100));
    }
  }

  void NotifyTtsEventReceivedByLacros(content::TtsEventType tts_event) {
    tts_event_notified_in_lacros_ = true;
    tts_event_received_ = tts_event;
  }

  bool TtsEventNotifiedInLacros() { return tts_event_notified_in_lacros_; }
  bool TtsEventReceivedEq(content::TtsEventType expected_tts_event) {
    return tts_event_received_ == expected_tts_event;
  }

  bool VoicesChangedNotified() { return voices_changed_; }
  void ResetVoicesChanged() { voices_changed_ = false; }

  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  test::AshBrowserTestStarter ash_starter_;

  // Used to verify that the TtsEvent is received by the lacros utterance's
  // UtteranceEventDelegate in Lacros.
  class EventDelegate : public crosapi::mojom::TtsUtteranceClient {
   public:
    explicit EventDelegate(extensions::AshTtsApiTest* owner) : owner_(owner) {}
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
      owner_->NotifyTtsEventReceivedByLacros(tts_event);
    }

    mojo::PendingRemote<crosapi::mojom::TtsUtteranceClient>
    BindTtsUtteranceClient() {
      return receiver_.BindNewPipeAndPassRemoteWithVersion();
    }

   private:
    raw_ptr<extensions::AshTtsApiTest, ExperimentalAsh> owner_;
    mojo::Receiver<crosapi::mojom::TtsUtteranceClient> receiver_{this};
  };

 private:
  bool voices_changed_ = false;
  bool expected_voice_loaded_ = false;
  bool tts_event_notified_in_lacros_ = false;
  // TtsEvent received by Lacros UtteranceEventDelegate.
  content::TtsEventType tts_event_received_;
};

//
// TTS Engine tests.
//

IN_PROC_BROWSER_TEST_F(AshTtsApiTest, RegisterAshEngine) {
  if (!ash_starter_.HasLacrosArgument())
    return;

  EXPECT_FALSE(VoicesChangedNotified());
  EXPECT_FALSE(HasVoiceWithName("Amy"));
  EXPECT_FALSE(HasVoiceWithName("Alex"));
  EXPECT_FALSE(HasVoiceWithName("Amanda"));

  ResetVoicesChanged();
  content::TtsController::GetInstance()->AddVoicesChangedDelegate(this);
  ASSERT_TRUE(
      RunExtensionTest("tts_engine/lacros_tts_support/register_ash_engine", {},
                       {.ignore_manifest_warnings = true}))
      << message_;

  WaitUntilVoicesLoaded();

  EXPECT_TRUE(VoicesChangedNotified());

  // Verify all the voices from tts engine extension are returned by
  // TtsController::GetVoices().
  std::vector<content::VoiceData> voices;
  content::TtsController::GetInstance()->GetVoices(profile(), GURL(), &voices);
  EXPECT_TRUE(HasVoiceWithName("Amy"));
  EXPECT_TRUE(HasVoiceWithName("Alex"));
  EXPECT_TRUE(HasVoiceWithName("Amanda"));

  // Verify all the voices are loaded at Lacros side.
  crosapi::mojom::StandaloneBrowserTestControllerAsyncWaiter waiter(
      GetStandaloneBrowserTestController());

  std::vector<crosapi::mojom::TtsVoicePtr> mojo_voices;
  while (mojo_voices.size() == 0) {
    waiter.GetTtsVoices(&mojo_voices);
    if (mojo_voices.size() > 0)
      break;
    GiveItSomeTime(base::Milliseconds(100));
  }

  EXPECT_TRUE(FoundVoiceInMojoVoices("Amy", mojo_voices));
  EXPECT_TRUE(FoundVoiceInMojoVoices("Alex", mojo_voices));
  EXPECT_TRUE(FoundVoiceInMojoVoices("Amanda", mojo_voices));
}

IN_PROC_BROWSER_TEST_F(AshTtsApiTest, SpeakLacrosUtteranceWithAshSpeechEngine) {
  if (!ash_starter_.HasLacrosArgument())
    return;

  EXPECT_FALSE(VoicesChangedNotified());
  EXPECT_FALSE(HasVoiceWithName("Amy"));
  EXPECT_FALSE(HasVoiceWithName("Alex"));
  EXPECT_FALSE(HasVoiceWithName("Amanda"));

  ResetVoicesChanged();
  content::TtsController::GetInstance()->AddVoicesChangedDelegate(this);

  // Load speech engine extension in Ash.
  ASSERT_TRUE(
      RunExtensionTest("tts_engine/lacros_tts_support/"
                       "tts_speak_lacros_utterance_with_ash_engine",
                       {}, {.ignore_manifest_warnings = true}))
      << message_;

  WaitUntilVoicesLoaded();

  EXPECT_TRUE(VoicesChangedNotified());

  // Verify all the voices from tts engine extension are laded in Ash.
  std::vector<content::VoiceData> voices;
  content::TtsController::GetInstance()->GetVoices(profile(), GURL(), &voices);
  EXPECT_TRUE(HasVoiceWithName("Amy"));
  EXPECT_TRUE(HasVoiceWithName("Alex"));
  EXPECT_TRUE(HasVoiceWithName("Amanda"));
  // Verify a random dummy voice is not loaded.
  EXPECT_FALSE(HasVoiceWithName("Tommy"));

  // Verify all the voices are loaded in Lacros.
  crosapi::mojom::StandaloneBrowserTestControllerAsyncWaiter waiter(
      GetStandaloneBrowserTestController());

  std::vector<crosapi::mojom::TtsVoicePtr> mojo_voices;
  while (mojo_voices.size() == 0) {
    waiter.GetTtsVoices(&mojo_voices);
    if (mojo_voices.size() > 0)
      break;
    GiveItSomeTime(base::Milliseconds(100));
  }

  EXPECT_TRUE(FoundVoiceInMojoVoices("Amy", mojo_voices));
  EXPECT_TRUE(FoundVoiceInMojoVoices("Alex", mojo_voices));
  EXPECT_TRUE(FoundVoiceInMojoVoices("Amanda", mojo_voices));

  // Due to crbug/1368284, we can not write a Lacros browser test to load
  // a testing extension in Lacros calling tts.speak with lacros tts support
  // enabled. Instead, we make a workaround to have an ash browser test request
  // Lacros to speak a Lacros utterance with
  // StandaloneBrowserTestController::TtsSpeak.
  std::unique_ptr<content::TtsUtterance> utterance =
      content::TtsUtterance::Create();
  utterance->SetText("Hello from Lacros");
  utterance->SetVoiceName("Amy");
  crosapi::mojom::TtsUtterancePtr mojo_utterance =
      tts_crosapi_util::ToMojo(utterance.get());
  // Note: mojo_utterance requires a value for browser_context_id, but it will
  // not used in Lacros for the testing workaround case.
  mojo_utterance->browser_context_id = base::UnguessableToken::Create();
  auto pending_client = std::make_unique<EventDelegate>(this);
  GetStandaloneBrowserTestController()->TtsSpeak(
      std::move(mojo_utterance), pending_client->BindTtsUtteranceClient());

  // Verify that the tts event has been received by the UtteranceEventDelegate
  // in Lacros.
  WaitUntilTtsEventReceivedByLacrosUtteranceEventDelegate();
  EXPECT_TRUE(TtsEventNotifiedInLacros());
  EXPECT_TRUE(TtsEventReceivedEq(content::TTS_EVENT_END));
}

IN_PROC_BROWSER_TEST_F(AshTtsApiTest, IsSpeaking) {
  if (!ash_starter_.HasLacrosArgument()) {
    return;
  }

  // Load Ash tts engine extension, register the tts engine events, and
  // call tts.isSpeaking before/during/after tts.speak.
  ASSERT_TRUE(RunExtensionTest("tts/is_speaking/", {},
                               {.ignore_manifest_warnings = true}))
      << message_;
}

}  // namespace extensions
