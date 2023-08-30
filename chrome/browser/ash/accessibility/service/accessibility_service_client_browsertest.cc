// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/accessibility_focus_ring_controller_impl.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "chrome/browser/accessibility/service/accessibility_service_router_factory.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/service/accessibility_service_client.h"
#include "chrome/browser/ash/accessibility/service/automation_client_impl.h"
#include "chrome/browser/ash/accessibility/service/fake_accessibility_service.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/tts_utterance.h"
#include "content/public/test/browser_test.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/tts.mojom.h"
#include "services/accessibility/public/mojom/user_interface.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/compositor/layer.h"

using ax::mojom::AssistiveTechnologyType;

namespace ash {

namespace {
// Matches max utterance from the TTS extension API.
const int kMaxUtteranceLength = 32768;

// TtsUtteranceClient that will pass along TtsEvents to a repeating callback.
class TtsUtteranceClientImpl : public ax::mojom::TtsUtteranceClient {
 public:
  TtsUtteranceClientImpl(
      mojo::PendingReceiver<ax::mojom::TtsUtteranceClient> pending_receiver,
      base::RepeatingCallback<void(ax::mojom::TtsEventPtr)> event_callback)
      : receiver_(this, std::move(pending_receiver)),
        callback_(std::move(event_callback)) {}

  TtsUtteranceClientImpl(const TtsUtteranceClientImpl&) = delete;
  TtsUtteranceClientImpl& operator=(const TtsUtteranceClientImpl&) = delete;
  ~TtsUtteranceClientImpl() override {}

  void OnEvent(ax::mojom::TtsEventPtr event) override {
    callback_.Run(std::move(event));
  }

 private:
  mojo::Receiver<ax::mojom::TtsUtteranceClient> receiver_;
  base::RepeatingCallback<void(ax::mojom::TtsEventPtr)> callback_;
};

// Mock TtsPlatform that can keep some state about an utterance and
// send events.
class MockTtsPlatformImpl : public content::TtsPlatform {
 public:
  MockTtsPlatformImpl() {
    content::TtsController::SkipAddNetworkChangeObserverForTests(true);
    content::TtsController::GetInstance()->SetTtsPlatform(this);
  }

  MockTtsPlatformImpl(const MockTtsPlatformImpl&) = delete;
  MockTtsPlatformImpl& operator=(const MockTtsPlatformImpl&) = delete;
  ~MockTtsPlatformImpl() {
    content::TtsController::GetInstance()->SetTtsPlatform(
        content::TtsPlatform::GetInstance());
  }

  // content::TtsPlatform:
  bool PlatformImplSupported() override { return true; }

  bool PlatformImplInitialized() override { return true; }

  void WillSpeakUtteranceWithVoice(
      content::TtsUtterance* utterance,
      const content::VoiceData& voice_data) override {}

  void LoadBuiltInTtsEngine(content::BrowserContext* browser_context) override {
  }

  void ClearError() override { error_ = ""; }

  void SetError(const std::string& error) override { error_ = error; }

  std::string GetError() override { return error_; }

  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const content::VoiceData& voice,
             const content::UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> speech_started_callback) override {
    utterance_id_ = utterance_id;
    utterance_ = utterance;
    lang_ = lang;
    voice_ = voice;
    params_ = params;
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id, /*event_type=*/content::TTS_EVENT_START, /*char_index=*/0,
        /*length=*/static_cast<int>(utterance.size()),
        /*error_message=*/std::string());
    if (next_utterance_error_.empty()) {
      std::move(speech_started_callback).Run(true);
      return;
    }
    SetError(next_utterance_error_);
    next_utterance_error_ = "";
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id, /*event_type=*/content::TTS_EVENT_ERROR, /*char_index=*/0,
        /*length=*/-1, /*error_message=*/GetError());
    std::move(speech_started_callback).Run(false);
  }

  bool StopSpeaking() override {
    if (utterance_id_ != -1) {
      content::TtsController::GetInstance()->OnTtsEvent(
          utterance_id_, /*event_type*/ content::TTS_EVENT_INTERRUPTED,
          /*char_index=*/0, /*length=*/0, /*error_message=*/"");
      utterance_id_ = -1;
      utterance_ = "";
      return true;
    }
    return false;
  }

  void Pause() override {
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id_, /*event_type=*/content::TTS_EVENT_PAUSE,
        /*char_index=*/3, /*length=*/4, /*error_message=*/"");
  }

  void Resume() override {
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id_, /*event_type=*/content::TTS_EVENT_RESUME,
        /*char_index=*/3, /*length=*/4, /*error_message=*/"");
  }

  bool IsSpeaking() override { return utterance_id_ != -1; }

  void GetVoices(std::vector<content::VoiceData>* voices) override {
    for (int i = 0; i < 3; i++) {
      voices->emplace_back();
      content::VoiceData& voice = voices->back();
      voice.native = true;
      voice.name = "TestyMcTestFace" + base::NumberToString(i);
      voice.lang = "en-NZ";
      voice.engine_id = extension_misc::kGoogleSpeechSynthesisExtensionId;
      voice.events.insert(content::TTS_EVENT_END);
      voice.events.insert(content::TTS_EVENT_START);
      voice.events.insert(content::TTS_EVENT_PAUSE);
      voice.events.insert(content::TTS_EVENT_RESUME);
      voice.events.insert(content::TTS_EVENT_INTERRUPTED);
      voice.events.insert(content::TTS_EVENT_WORD);
      voice.events.insert(content::TTS_EVENT_SENTENCE);
      voice.events.insert(content::TTS_EVENT_MARKER);
      voice.events.insert(content::TTS_EVENT_CANCELLED);
      voice.events.insert(content::TTS_EVENT_ERROR);
    }
  }

  void Shutdown() override {}

  void FinalizeVoiceOrdering(std::vector<content::VoiceData>& voices) override {
  }

  void RefreshVoices() override {}

  content::ExternalPlatformDelegate* GetExternalPlatformDelegate() override {
    return nullptr;
  }

  // Methods for testing.
  void SendEvent(content::TtsEventType event_type,
                 int char_index,
                 int length,
                 const std::string& error_message) {
    ASSERT_NE(utterance_id_, -1);
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id_, event_type, char_index, length, error_message);
  }
  void SetNextUtteranceError(const std::string& error) {
    next_utterance_error_ = error;
  }
  const std::string& lang() { return lang_; }
  const content::VoiceData& voice() { return voice_; }
  const content::UtteranceContinuousParameters& params() { return params_; }

 private:
  std::string utterance_ = "";
  int utterance_id_ = -1;
  std::string lang_ = "";
  content::VoiceData voice_;
  content::UtteranceContinuousParameters params_;
  std::string error_ = "";
  std::string next_utterance_error_ = "";
};

}  // namespace

// Tests for the AccessibilityServiceClientTest using a fake service
// implemented in FakeAccessibilityService.
class AccessibilityServiceClientTest : public InProcessBrowserTest {
 public:
  AccessibilityServiceClientTest() = default;
  AccessibilityServiceClientTest(const AccessibilityServiceClientTest&) =
      delete;
  AccessibilityServiceClientTest& operator=(
      const AccessibilityServiceClientTest&) = delete;
  ~AccessibilityServiceClientTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityService);
  }

  void SetUp() override {
    content::TtsController::SkipAddNetworkChangeObserverForTests(true);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Replaces normal AccessibilityService with a fake one.
    ax::AccessibilityServiceRouterFactory::GetInstanceForTest()
        ->SetTestingFactoryAndUse(
            browser()->profile(),
            base::BindRepeating(
                &AccessibilityServiceClientTest::CreateTestAccessibilityService,
                base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  bool ServiceHasATEnabled(AssistiveTechnologyType type) {
    std::set<AssistiveTechnologyType> enabled_ATs =
        fake_service_->GetEnabledATs();
    return enabled_ATs.find(type) != enabled_ATs.end();
  }

  bool ServiceIsBound() { return fake_service_->IsBound(); }

  void ToggleAutomationEnabled(AccessibilityServiceClient* client,
                               bool enabled) {
    if (enabled)
      client->automation_client_->Enable(base::DoNothing());
    else
      client->automation_client_->Disable();
  }

  std::unique_ptr<AccessibilityServiceClient> TurnOnAccessibilityService(
      AssistiveTechnologyType type) {
    auto client = std::make_unique<AccessibilityServiceClient>();
    client->SetProfile(browser()->profile());
    switch (type) {
      case ax::mojom::AssistiveTechnologyType::kUnknown:
        NOTREACHED() << "Unknown AT type";
        break;
      case ax::mojom::AssistiveTechnologyType::kChromeVox:
        client->SetChromeVoxEnabled(true);
        break;
      case ax::mojom::AssistiveTechnologyType::kSelectToSpeak:
        client->SetSelectToSpeakEnabled(true);
        break;
      case ax::mojom::AssistiveTechnologyType::kSwitchAccess:
        client->SetSwitchAccessEnabled(true);
        break;
      case ax::mojom::AssistiveTechnologyType::kAutoClick:
        client->SetAutoclickEnabled(true);
        break;
      case ax::mojom::AssistiveTechnologyType::kMagnifier:
        client->SetMagnifierEnabled(true);
        break;
      case ax::mojom::AssistiveTechnologyType::kDictation:
        client->SetDictationEnabled(true);
        break;
    }
    EXPECT_TRUE(ServiceHasATEnabled(type));
    return client;
  }

  // Unowned.
  raw_ptr<FakeAccessibilityService, DanglingUntriaged | ExperimentalAsh>
      fake_service_ = nullptr;

 private:
  std::unique_ptr<KeyedService> CreateTestAccessibilityService(
      content::BrowserContext* context) {
    std::unique_ptr<FakeAccessibilityService> fake_service =
        std::make_unique<FakeAccessibilityService>();
    fake_service_ = fake_service.get();
    return std::move(fake_service);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that nothing crashes if the profile isn't set yet.
// Note that this should never happen as enabling/disabling
// features from AccessibilityManager will only happen when
// there is a profile.
IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       DoesNotCrashWithNoProfile) {
  AccessibilityServiceClient client;
  client.SetChromeVoxEnabled(true);

  client.SetProfile(nullptr);
  client.SetSelectToSpeakEnabled(true);

  EXPECT_FALSE(ServiceIsBound());
}

// AccessibilityServiceClient shouldn't try to use the service
// when features are all disabled.
IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       DoesNotCreateServiceForDisabledFeatures) {
  AccessibilityServiceClient client;
  EXPECT_FALSE(ServiceIsBound());

  client.SetProfile(browser()->profile());
  EXPECT_FALSE(ServiceIsBound());

  client.SetChromeVoxEnabled(false);
  EXPECT_FALSE(ServiceIsBound());

  client.SetDictationEnabled(false);
  EXPECT_FALSE(ServiceIsBound());
}

// Test that any previously enabled features are copied when
// the profile changes.
IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       CopiesFeaturesWhenProfileChanges) {
  AccessibilityServiceClient client;
  client.SetChromeVoxEnabled(true);
  client.SetSwitchAccessEnabled(true);
  client.SetAutoclickEnabled(true);
  client.SetAutoclickEnabled(false);

  // Service isn't constructed yet.
  EXPECT_FALSE(ServiceIsBound());

  client.SetProfile(browser()->profile());

  ASSERT_TRUE(ServiceIsBound());
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kChromeVox));
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kSwitchAccess));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kAutoClick));
}

// Test that the AccessibilityServiceClient can toggle features in the service
// using the mojom interface.
IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       TogglesAccessibilityFeatures) {
  AccessibilityServiceClient client;
  client.SetProfile(browser()->profile());
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kChromeVox));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kSelectToSpeak));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kSwitchAccess));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kAutoClick));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kDictation));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kMagnifier));

  // The first time we enable/disable an AT, the AT controller should be bound
  // with the enabled AT type.
  client.SetChromeVoxEnabled(true);
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kChromeVox));
  client.SetSelectToSpeakEnabled(true);
  fake_service_->WaitForATChanged();
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kSelectToSpeak));
  client.SetSwitchAccessEnabled(true);
  fake_service_->WaitForATChanged();
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kSwitchAccess));
  client.SetAutoclickEnabled(true);
  fake_service_->WaitForATChanged();
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kAutoClick));
  client.SetDictationEnabled(true);
  fake_service_->WaitForATChanged();
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kDictation));
  client.SetMagnifierEnabled(true);
  fake_service_->WaitForATChanged();
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kMagnifier));
  client.SetChromeVoxEnabled(false);
  fake_service_->WaitForATChanged();
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kChromeVox));
  client.SetSelectToSpeakEnabled(false);
  fake_service_->WaitForATChanged();
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kSelectToSpeak));
  client.SetSwitchAccessEnabled(false);
  fake_service_->WaitForATChanged();
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kSwitchAccess));
  client.SetAutoclickEnabled(false);
  fake_service_->WaitForATChanged();
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kAutoClick));
  client.SetDictationEnabled(false);
  fake_service_->WaitForATChanged();
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kDictation));
  client.SetMagnifierEnabled(false);
  fake_service_->WaitForATChanged();
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kMagnifier));
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       SendsAutomationToTheService) {
  // Enable an assistive technology. The service will not be started until
  // some AT needs it.
  auto client = TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);

  // The service may bind multiple Automations to the AutomationClient.
  for (int i = 0; i < 3; i++) {
    fake_service_->BindAnotherAutomation();
  }

  // TODO(crbug.com/1355633): Replace once mojom to Enable lands.
  ToggleAutomationEnabled(client.get(), true);
  // Enable can be called multiple times (once for each bound Automation)
  // with no bad effects.
  // fake_service_->AutomationClientEnable(true);

  // Real accessibility events should have come through.
  fake_service_->WaitForAutomationEvents();

  // TODO(crbug.com/1355633): Replace once mojom to Disable lands.
  ToggleAutomationEnabled(client.get(), false);
  // Disabling multiple times has no bad effect.
  // fake_service_->AutomationClientEnable(false);
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       DevToolsAgentHostCreated) {
  // Enable an assistive technology. The service will not be started until
  // some AT needs it.
  auto client = TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  client->SetChromeVoxEnabled(true);
  // A single agent host should have been created for chromevox.
  auto count = fake_service_->GetDevtoolsConnectionCount(
      AssistiveTechnologyType::kChromeVox);
  EXPECT_EQ(count, 1);
  // Disable and re-enable
  client->SetChromeVoxEnabled(false);
  client->SetChromeVoxEnabled(true);
  count = fake_service_->GetDevtoolsConnectionCount(
      AssistiveTechnologyType::kChromeVox);
  EXPECT_EQ(count, 2);
  // Different AT
  client->SetSelectToSpeakEnabled(true);
  count = fake_service_->GetDevtoolsConnectionCount(
      AssistiveTechnologyType::kSelectToSpeak);
  EXPECT_EQ(count, 1);
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsGetVoices) {
  auto client =
      TurnOnAccessibilityService(AssistiveTechnologyType::kSelectToSpeak);
  MockTtsPlatformImpl tts_platform;

  fake_service_->BindAnotherTts();

  base::RunLoop waiter;
  fake_service_->RequestTtsVoices(base::BindLambdaForTesting(
      [&waiter](std::vector<ax::mojom::TtsVoicePtr> voices) {
        waiter.Quit();
        ASSERT_EQ(voices.size(), 3u);
        auto& voice = voices[0];
        EXPECT_EQ(voice->voice_name, "TestyMcTestFace0");
        EXPECT_EQ(voice->engine_id,
                  extension_misc::kGoogleSpeechSynthesisExtensionId);
        ASSERT_TRUE(voice->event_types);
        ASSERT_EQ(voice->event_types.value().size(), 10u);
        // Spot check.
        EXPECT_EQ(voice->event_types.value()[0],
                  ax::mojom::TtsEventType::kStart);
        EXPECT_EQ(voice->event_types.value()[1], ax::mojom::TtsEventType::kEnd);
      }));
  waiter.Run();

  // The service may bind multiple TTS without crashing.
  for (int i = 0; i < 2; i++) {
    fake_service_->BindAnotherTts();
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsSpeakSimple) {
  auto client =
      TurnOnAccessibilityService(AssistiveTechnologyType::kSelectToSpeak);
  test::SpeechMonitor sm;

  fake_service_->BindAnotherTts();
  fake_service_->RequestSpeak("Hello, world", base::DoNothing());
  sm.ExpectSpeech("Hello, world");
  sm.Replay();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsSendsStartEndEvents) {
  test::SpeechMonitor sm;
  auto client = TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  fake_service_->BindAnotherTts();

  base::RunLoop waiter;
  int start_count = 0;
  int end_count = 0;
  std::string text = "Hello, world";

  // This callback is called on tts events.
  // See SpeechMonitor for when tts events are sent.
  base::RepeatingCallback<void(ax::mojom::TtsEventPtr event)> callback =
      base::BindLambdaForTesting([&waiter, &start_count, &end_count,
                                  &text](ax::mojom::TtsEventPtr event) {
        if (event->type == ax::mojom::TtsEventType::kStart) {
          start_count++;
          EXPECT_EQ(end_count, 0);
          EXPECT_EQ(0, event->char_index);
          EXPECT_FALSE(event->is_final);
        } else if (event->type == ax::mojom::TtsEventType::kEnd) {
          end_count++;
          EXPECT_EQ(start_count, 1);
          EXPECT_EQ(static_cast<int>(text.size()), event->char_index);
          EXPECT_TRUE(event->is_final);
          waiter.Quit();
        }
      });

  std::unique_ptr<TtsUtteranceClientImpl> utterance_client;
  fake_service_->RequestSpeak(
      text,
      base::BindLambdaForTesting(
          [&utterance_client, &callback](ax::mojom::TtsSpeakResultPtr result) {
            EXPECT_EQ(result->error, ax::mojom::TtsError::kNoError);
            utterance_client = std::make_unique<TtsUtteranceClientImpl>(
                std::move(result->utterance_client), std::move(callback));
          }));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsPauseResume) {
  MockTtsPlatformImpl tts_platform;
  auto client =
      TurnOnAccessibilityService(AssistiveTechnologyType::kSelectToSpeak);
  fake_service_->BindAnotherTts();

  base::RunLoop waiter;
  int start_count = 0;
  int pause_count = 0;
  int resume_count = 0;
  int interrupted_count = 0;
  std::string text = "Hello, world";

  // This callback is called on tts events.
  base::RepeatingCallback<void(ax::mojom::TtsEventPtr event)> callback =
      base::BindLambdaForTesting(
          [&waiter, &start_count, &pause_count, &resume_count,
           &interrupted_count](ax::mojom::TtsEventPtr event) {
            if (event->type == ax::mojom::TtsEventType::kStart) {
              start_count++;
              EXPECT_EQ(pause_count, 0);
              EXPECT_EQ(resume_count, 0);
              EXPECT_EQ(interrupted_count, 0);
              EXPECT_EQ(0, event->char_index);
              EXPECT_FALSE(event->is_final);
            } else if (event->type == ax::mojom::TtsEventType::kPause) {
              pause_count++;
              EXPECT_EQ(resume_count, 0);
              EXPECT_EQ(interrupted_count, 0);
              EXPECT_FALSE(event->is_final);
            } else if (event->type == ax::mojom::TtsEventType::kResume) {
              resume_count++;
              EXPECT_EQ(interrupted_count, 0);
              EXPECT_FALSE(event->is_final);
            } else if (event->type == ax::mojom::TtsEventType::kInterrupted) {
              interrupted_count++;
              EXPECT_TRUE(event->is_final);
              waiter.Quit();
            }
          });

  std::unique_ptr<TtsUtteranceClientImpl> utterance_client;
  fake_service_->RequestSpeak(
      text,
      base::BindLambdaForTesting([&utterance_client, &callback,
                                  this](ax::mojom::TtsSpeakResultPtr result) {
        ASSERT_EQ(result->error, ax::mojom::TtsError::kNoError);
        utterance_client = std::make_unique<TtsUtteranceClientImpl>(
            std::move(result->utterance_client), std::move(callback));
        fake_service_->RequestPause();
        fake_service_->RequestResume();
        fake_service_->RequestStop();
      }));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsIsSpeaking) {
  MockTtsPlatformImpl tts_platform;
  auto client = TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  fake_service_->BindAnotherTts();

  base::RunLoop waiter;
  std::string text = "Hello, world";

  fake_service_->RequestSpeak(
      text, base::BindLambdaForTesting(
                [&waiter, this](ax::mojom::TtsSpeakResultPtr result) {
                  ASSERT_EQ(result->error, ax::mojom::TtsError::kNoError);
                  fake_service_->IsTtsSpeaking(
                      base::BindLambdaForTesting([&waiter](bool is_speaking) {
                        EXPECT_TRUE(is_speaking);
                        waiter.Quit();
                      }));
                }));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsIsNotSpeaking) {
  MockTtsPlatformImpl tts_platform;
  auto client =
      TurnOnAccessibilityService(AssistiveTechnologyType::kSelectToSpeak);
  fake_service_->BindAnotherTts();

  base::RunLoop waiter;

  fake_service_->IsTtsSpeaking(
      base::BindLambdaForTesting([&waiter](bool is_speaking) {
        EXPECT_FALSE(is_speaking);
        waiter.Quit();
      }));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsMaxUtteranceError) {
  auto client =
      TurnOnAccessibilityService(AssistiveTechnologyType::kSelectToSpeak);
  fake_service_->BindAnotherTts();
  base::RunLoop waiter;

  fake_service_->RequestSpeak(
      std::string(kMaxUtteranceLength + 1, 'a'),
      base::BindLambdaForTesting([&waiter](
                                     ax::mojom::TtsSpeakResultPtr result) {
        EXPECT_EQ(result->error, ax::mojom::TtsError::kErrorUtteranceTooLong);
        waiter.Quit();
      }));

  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsUtteranceError) {
  MockTtsPlatformImpl tts_platform;
  tts_platform.SetNextUtteranceError("One does not simply walk into Mordor");
  auto client = TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  fake_service_->BindAnotherTts();

  base::RunLoop waiter;

  // This callback is called on tts events.
  base::RepeatingCallback<void(ax::mojom::TtsEventPtr event)> callback =
      base::BindLambdaForTesting([&waiter](ax::mojom::TtsEventPtr event) {
        if (event->type == ax::mojom::TtsEventType::kStart) {
          return;
        }
        EXPECT_EQ(event->type, ax::mojom::TtsEventType::kError);
        EXPECT_EQ(event->error_message, "One does not simply walk into Mordor");
        waiter.Quit();
      });

  std::unique_ptr<TtsUtteranceClientImpl> utterance_client;
  fake_service_->RequestSpeak(
      "All we have to decide is what to do with the time that is given to us.",
      base::BindLambdaForTesting(
          [&utterance_client, &callback](ax::mojom::TtsSpeakResultPtr result) {
            utterance_client = std::make_unique<TtsUtteranceClientImpl>(
                std::move(result->utterance_client), std::move(callback));
          }));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsOptions) {
  MockTtsPlatformImpl tts_platform;
  auto client = TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  fake_service_->BindAnotherTts();
  base::RunLoop waiter;

  auto options = ax::mojom::TtsOptions::New();
  options->rate = .5;
  options->pitch = 1.5;
  options->volume = .8;
  options->enqueue = true;
  options->voice_name = "TestyMcTestFace2";
  options->engine_id = extension_misc::kGoogleSpeechSynthesisExtensionId;
  options->lang = "en-NZ";
  options->on_event = false;

  fake_service_->RequestSpeak(
      "I can't recall the taste of strawberries", std::move(options),
      base::BindLambdaForTesting([&waiter, &tts_platform](
                                     ax::mojom::TtsSpeakResultPtr result) {
        waiter.Quit();
        content::UtteranceContinuousParameters params = tts_platform.params();
        EXPECT_EQ(params.rate, .5);
        EXPECT_EQ(params.pitch, 1.5);
        EXPECT_EQ(params.volume, .8);
        content::VoiceData voice = tts_platform.voice();
        EXPECT_EQ(voice.name, "TestyMcTestFace2");
        EXPECT_EQ(tts_platform.lang(), "en-NZ");
      }));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsOptionsPitchError) {
  auto client = TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  fake_service_->BindAnotherTts();
  base::RunLoop waiter;
  auto options = ax::mojom::TtsOptions::New();
  options->pitch = 3.0;

  fake_service_->RequestSpeak(
      "You shall not pass", std::move(options),
      base::BindLambdaForTesting(
          [&waiter](ax::mojom::TtsSpeakResultPtr result) {
            waiter.Quit();
            EXPECT_EQ(result->error, ax::mojom::TtsError::kErrorInvalidPitch);
          }));

  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsOptionsRateError) {
  auto client = TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  fake_service_->BindAnotherTts();
  base::RunLoop waiter;
  auto options = ax::mojom::TtsOptions::New();
  options->rate = 0.01;
  fake_service_->RequestSpeak(
      "For frodo", std::move(options),
      base::BindLambdaForTesting(
          [&waiter](ax::mojom::TtsSpeakResultPtr result) {
            waiter.Quit();
            EXPECT_EQ(result->error, ax::mojom::TtsError::kErrorInvalidRate);
          }));

  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsOptionsVolumeError) {
  auto client = TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  fake_service_->BindAnotherTts();
  base::RunLoop waiter;
  auto options = ax::mojom::TtsOptions::New();
  options->volume = 1.5;
  fake_service_->RequestSpeak(
      "The board is set. The pieces are moving.", std::move(options),
      base::BindLambdaForTesting(
          [&waiter](ax::mojom::TtsSpeakResultPtr result) {
            waiter.Quit();
            EXPECT_EQ(result->error, ax::mojom::TtsError::kErrorInvalidVolume);
          }));

  waiter.Run();
}

// Starts two requests for speech, the second starting just after the first
// is in progress. With the option to enqueue, they should not interrupt.
IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsEnqueue) {
  MockTtsPlatformImpl tts_platform;
  auto client =
      TurnOnAccessibilityService(AssistiveTechnologyType::kSelectToSpeak);
  fake_service_->BindAnotherTts();
  base::RunLoop waiter;

  base::RepeatingCallback<void(ax::mojom::TtsEventPtr event)> first_callback =
      base::BindLambdaForTesting([](ax::mojom::TtsEventPtr event) {
        EXPECT_EQ(event->type, ax::mojom::TtsEventType::kStart);
      });
  auto first_options = ax::mojom::TtsOptions::New();
  first_options->enqueue = true;
  first_options->on_event = true;
  std::unique_ptr<TtsUtteranceClientImpl> first_utterance_client;
  fake_service_->RequestSpeak(
      "Shadowfax, show us the meaning of haste.", std::move(first_options),
      base::BindLambdaForTesting([&first_callback, &first_utterance_client](
                                     ax::mojom::TtsSpeakResultPtr result) {
        first_utterance_client = std::make_unique<TtsUtteranceClientImpl>(
            std::move(result->utterance_client), std::move(first_callback));
      }));
  EXPECT_EQ(content::TtsController::GetInstance()->QueueSize(), 0);

  auto second_options = ax::mojom::TtsOptions::New();
  second_options->enqueue = true;
  second_options->on_event = true;
  std::unique_ptr<TtsUtteranceClientImpl> second_utterance_client;
  fake_service_->RequestSpeak(
      "Keep it secret. Keep it safe.", std::move(second_options),
      base::BindLambdaForTesting(
          [&waiter](ax::mojom::TtsSpeakResultPtr result) {
            EXPECT_EQ(content::TtsController::GetInstance()->QueueSize(), 1);
            waiter.Quit();
          }));
  waiter.Run();
}

// Starts two requests for speech, the second starting just after the first
// is in progress. With the the option to enqueue false, the second interrupts
// the first.
IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsInterrupt) {
  MockTtsPlatformImpl tts_platform;
  auto client =
      TurnOnAccessibilityService(AssistiveTechnologyType::kSelectToSpeak);
  fake_service_->BindAnotherTts();
  base::RunLoop waiter;
  int start_count = 0;
  base::RepeatingCallback<void(ax::mojom::TtsEventPtr event)> first_callback =
      base::BindLambdaForTesting([&start_count](ax::mojom::TtsEventPtr event) {
        if (event->type == ax::mojom::TtsEventType::kStart) {
          // The first event should be started.
          EXPECT_EQ(start_count, 0);
          start_count++;
          return;
        }
        // And then interrupted.
        EXPECT_EQ(event->type, ax::mojom::TtsEventType::kInterrupted);
      });
  auto first_options = ax::mojom::TtsOptions::New();
  first_options->enqueue = true;
  first_options->on_event = true;
  std::unique_ptr<TtsUtteranceClientImpl> first_utterance_client;
  fake_service_->RequestSpeak(
      "Shadowfax, show us the meaning of haste.", std::move(first_options),
      base::BindLambdaForTesting([&first_callback, &first_utterance_client](
                                     ax::mojom::TtsSpeakResultPtr result) {
        first_utterance_client = std::make_unique<TtsUtteranceClientImpl>(
            std::move(result->utterance_client), std::move(first_callback));
      }));

  base::RepeatingCallback<void(ax::mojom::TtsEventPtr event)> second_callback =
      base::BindLambdaForTesting(
          [&start_count, &waiter](ax::mojom::TtsEventPtr event) {
            EXPECT_EQ(event->type, ax::mojom::TtsEventType::kStart);
            // The second utterance should start after the first started.
            EXPECT_EQ(start_count, 1);
            waiter.Quit();
          });

  auto second_options = ax::mojom::TtsOptions::New();
  second_options->enqueue = false;
  second_options->on_event = true;
  std::unique_ptr<TtsUtteranceClientImpl> second_utterance_client;
  fake_service_->RequestSpeak(
      "Keep it secret. Keep it safe.", std::move(second_options),
      base::BindLambdaForTesting([&second_utterance_client, &second_callback](
                                     ax::mojom::TtsSpeakResultPtr result) {
        EXPECT_EQ(content::TtsController::GetInstance()->QueueSize(), 0);
        second_utterance_client = std::make_unique<TtsUtteranceClientImpl>(
            std::move(result->utterance_client), std::move(second_callback));
      }));

  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, SetFocusRings) {
  auto client =
      TurnOnAccessibilityService(AssistiveTechnologyType::kSwitchAccess);
  fake_service_->BindAnotherUserInterface();

  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  controller->SetNoFadeForTesting();

  std::string focus_ring_id1 = AccessibilityManager::Get()->GetFocusRingId(
      ax::mojom::AssistiveTechnologyType::kSwitchAccess, "");
  const AccessibilityFocusRingGroup* focus_ring_group1 =
      controller->GetFocusRingGroupForTesting(focus_ring_id1);
  std::string focus_ring_id2 = AccessibilityManager::Get()->GetFocusRingId(
      ax::mojom::AssistiveTechnologyType::kSwitchAccess, "mySpoonIsTooBig");
  const AccessibilityFocusRingGroup* focus_ring_group2 =
      controller->GetFocusRingGroupForTesting(focus_ring_id2);
  // No focus rings to start.
  EXPECT_EQ(nullptr, focus_ring_group1);
  EXPECT_EQ(nullptr, focus_ring_group2);

  // Number of times the focus ring observer is called.
  int count = 0;

  base::RunLoop waiter;
  AccessibilityManager::Get()->SetFocusRingObserverForTest(
      base::BindLambdaForTesting([&waiter, &controller, &focus_ring_id1,
                                  &focus_ring_group1, &focus_ring_id2,
                                  &focus_ring_group2, &count]() {
        if (count == 0) {
          // Wait for this to be called twice.
          count++;
          return;
        }
        waiter.Quit();
        // Check that the focus rings have been set appropriately.
        focus_ring_group1 =
            controller->GetFocusRingGroupForTesting(focus_ring_id1);
        ASSERT_NE(nullptr, focus_ring_group1);
        std::vector<std::unique_ptr<AccessibilityFocusRingLayer>> const&
            focus_rings = focus_ring_group1->focus_layers_for_testing();
        EXPECT_EQ(focus_rings.size(), 1u);
        gfx::Rect target_bounds = focus_rings.at(0)->layer()->GetTargetBounds();
        EXPECT_EQ(target_bounds.CenterPoint(),
                  gfx::Rect(50, 100, 42, 84).CenterPoint());
        AccessibilityFocusRingInfo* focus_ring_info =
            focus_ring_group1->focus_ring_info_for_testing();
        EXPECT_EQ(focus_ring_info->type, FocusRingType::GLOW);
        EXPECT_EQ(focus_ring_info->color, SK_ColorRED);
        EXPECT_EQ(focus_ring_info->behavior, FocusRingBehavior::PERSIST);

        focus_ring_group2 =
            controller->GetFocusRingGroupForTesting(focus_ring_id2);
        ASSERT_NE(nullptr, focus_ring_group2);
        std::vector<std::unique_ptr<AccessibilityFocusRingLayer>> const&
            focus_rings2 = focus_ring_group2->focus_layers_for_testing();
        EXPECT_EQ(focus_rings2.size(), 1u);
        target_bounds = focus_rings2.at(0)->layer()->GetTargetBounds();
        EXPECT_EQ(target_bounds.CenterPoint(),
                  gfx::Rect(500, 200, 84, 42).CenterPoint());
        focus_ring_info = focus_ring_group2->focus_ring_info_for_testing();
        EXPECT_EQ(focus_ring_info->type, FocusRingType::DASHED);
        EXPECT_EQ(focus_ring_info->color, SK_ColorBLUE);
        EXPECT_EQ(focus_ring_info->background_color, SK_ColorGREEN);
        EXPECT_EQ(focus_ring_info->secondary_color, SK_ColorBLACK);
        EXPECT_EQ(focus_ring_info->behavior, FocusRingBehavior::PERSIST);
      }));

  // Set two focus rings.
  std::vector<ax::mojom::FocusRingInfoPtr> focus_rings;
  auto focus_ring1 = ax::mojom::FocusRingInfo::New();
  focus_ring1->color = SK_ColorRED;
  focus_ring1->rects.emplace_back(50, 100, 42, 84);
  focus_ring1->type = ax::mojom::FocusType::kGlow;
  focus_rings.emplace_back(std::move(focus_ring1));

  auto focus_ring2 = ax::mojom::FocusRingInfo::New();
  focus_ring2->color = SK_ColorBLUE;
  focus_ring2->rects.emplace_back(500, 200, 84, 42);
  focus_ring2->type = ax::mojom::FocusType::kDashed;
  focus_ring2->background_color = SK_ColorGREEN;
  focus_ring2->secondary_color = SK_ColorBLACK;
  focus_ring2->stacking_order =
      ax::mojom::FocusRingStackingOrder::kBelowAccessibilityBubbles;
  focus_ring2->id = "mySpoonIsTooBig";
  focus_rings.emplace_back(std::move(focus_ring2));
  fake_service_->RequestSetFocusRings(
      std::move(focus_rings),
      ax::mojom::AssistiveTechnologyType::kSwitchAccess);

  waiter.Run();
}

}  // namespace ash
