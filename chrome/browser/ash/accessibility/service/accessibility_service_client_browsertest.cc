// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/accessibility_service_client.h"

#include <optional>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/ui/accessibility_confirmation_dialog.h"
#include "ash/accessibility/ui/accessibility_focus_ring_controller_impl.h"
#include "ash/accessibility/ui/accessibility_highlight_layer.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/public/cpp/accessibility_focus_ring_info.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "chrome/browser/accessibility/service/accessibility_service_router_factory.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/service/automation_client_impl.h"
#include "chrome/browser/ash/accessibility/service/fake_accessibility_service.h"
#include "chrome/browser/ash/accessibility/service/speech_recognition_impl.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_recognition_test_helper.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/tts_utterance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom.h"
#include "services/accessibility/public/mojom/speech_recognition.mojom.h"
#include "services/accessibility/public/mojom/tts.mojom.h"
#include "services/accessibility/public/mojom/user_interface.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"

using ax::mojom::AssistiveTechnologyType;

class KeyboardVisibleWaiter : public ChromeKeyboardControllerClient::Observer {
 public:
  explicit KeyboardVisibleWaiter(bool visible) : visible_(visible) {
    ChromeKeyboardControllerClient::Get()->AddObserver(this);
  }

  KeyboardVisibleWaiter(const KeyboardVisibleWaiter&) = delete;
  KeyboardVisibleWaiter& operator=(const KeyboardVisibleWaiter&) = delete;

  ~KeyboardVisibleWaiter() override {
    ChromeKeyboardControllerClient::Get()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  // ChromeKeyboardControllerClient::Observer
  void OnKeyboardVisibilityChanged(bool visible) override {
    if (visible == visible_) {
      run_loop_.QuitWhenIdle();
    }
  }

 private:
  base::RunLoop run_loop_;
  const bool visible_;
};  // namespace

namespace ash {

class DialogShownWaiter {
 public:
  DialogShownWaiter() {
    ash::Shell::Get()
        ->accessibility_controller()
        ->AddShowConfirmationDialogCallbackForTesting(base::BindRepeating(
            &DialogShownWaiter::OnDialogShown, weak_factory_.GetWeakPtr()));
  }

  DialogShownWaiter(const DialogShownWaiter&) = delete;
  DialogShownWaiter& operator=(const DialogShownWaiter&) = delete;

  ~DialogShownWaiter() = default;

  void Wait() { run_loop_.Run(); }

  void OnDialogShown() { run_loop_.QuitWhenIdle(); }

 private:
  base::RunLoop run_loop_;
  base::WeakPtrFactory<DialogShownWaiter> weak_factory_{this};
};  // namespace

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

class TestEventHandler : public ui::EventHandler {
 public:
  explicit TestEventHandler(base::RepeatingClosure callback)
      : callback_(callback) {
    Shell::Get()->AddPreTargetHandler(this);
  }
  ~TestEventHandler() override { Shell::Get()->RemovePreTargetHandler(this); }

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override {
    // Make a copy of the event, so it's valid outside this function context.
    key_events.push_back(std::make_unique<ui::KeyEvent>(event));
    callback_.Run();
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    // Make a copy of the event, so it's valid outside this function context.
    mouse_events.push_back(std::make_unique<ui::MouseEvent>(event));
    callback_.Run();
  }

  std::vector<std::unique_ptr<ui::KeyEvent>> key_events;
  std::vector<std::unique_ptr<ui::MouseEvent>> mouse_events;

 private:
  base::RepeatingClosure callback_;
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
    sr_test_helper_ = std::make_unique<SpeechRecognitionTestHelper>(
        speech::SpeechRecognitionType::kNetwork,
        media::mojom::RecognizerClientType::kDictation);
    sr_test_helper_->SetUp(browser()->profile());
  }

  void TearDownOnMainThread() override {
    content::SpeechRecognitionManager::SetManagerForTesting(nullptr);
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  AccessibilityServiceClient* Client() {
    AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
    return accessibility_manager->accessibility_service_client_.get();
  }

  UserInputImpl* UserInputClient() {
    return Client()->user_input_client_.get();
  }

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

  // TODO(crbug.com/40936728): Toggle features on AccessibilityManager for
  // client test.
  void TurnOnAccessibilityService(AssistiveTechnologyType type) {
    switch (type) {
      case ax::mojom::AssistiveTechnologyType::kUnknown:
        NOTREACHED_IN_MIGRATION() << "Unknown AT type";
        break;
      case ax::mojom::AssistiveTechnologyType::kChromeVox:
        Client()->SetChromeVoxEnabled(true);
        break;
      case ax::mojom::AssistiveTechnologyType::kSelectToSpeak:
        Client()->SetSelectToSpeakEnabled(true);
        break;
      case ax::mojom::AssistiveTechnologyType::kSwitchAccess:
        Client()->SetSwitchAccessEnabled(true);
        break;
      case ax::mojom::AssistiveTechnologyType::kAutoClick:
        Client()->SetAutoclickEnabled(true);
        break;
      case ax::mojom::AssistiveTechnologyType::kMagnifier:
        Client()->SetMagnifierEnabled(true);
        break;
      case ax::mojom::AssistiveTechnologyType::kDictation:
        Client()->SetDictationEnabled(true);
        break;
    }
    EXPECT_TRUE(ServiceHasATEnabled(type));
  }

  // Unowned.
  raw_ptr<FakeAccessibilityService, DanglingUntriaged> fake_service_ = nullptr;

  std::unique_ptr<SpeechRecognitionTestHelper> sr_test_helper_;

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
  Client()->SetProfile(nullptr);
  Client()->SetChromeVoxEnabled(true);
  Client()->SetSelectToSpeakEnabled(true);

  EXPECT_FALSE(ServiceIsBound());
}

// AccessibilityServiceClient shouldn't try to use the service
// when features are all disabled.
IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       DoesNotCreateServiceForDisabledFeatures) {
  EXPECT_FALSE(ServiceIsBound());

  Client()->SetChromeVoxEnabled(false);
  EXPECT_FALSE(ServiceIsBound());

  Client()->SetDictationEnabled(false);
  EXPECT_FALSE(ServiceIsBound());
}

// Test that any previously enabled features are copied when
// the profile changes.
IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       CopiesFeaturesWhenProfileChanges) {
  Client()->SetProfile(nullptr);
  Client()->SetChromeVoxEnabled(true);
  Client()->SetSwitchAccessEnabled(true);
  Client()->SetAutoclickEnabled(true);
  Client()->SetAutoclickEnabled(false);

  // Service isn't constructed yet because there is no profile.
  EXPECT_FALSE(ServiceIsBound());

  Client()->SetProfile(browser()->profile());

  ASSERT_TRUE(ServiceIsBound());
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kChromeVox));
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kSwitchAccess));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kAutoClick));
}

// Test that the AccessibilityServiceClient can toggle features in the service
// using the mojom interface.
IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       TogglesAccessibilityFeatures) {
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kChromeVox));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kSelectToSpeak));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kSwitchAccess));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kAutoClick));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kDictation));
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kMagnifier));

  // The first time we enable/disable an AT, the AT controller should be bound
  // with the enabled AT type.
  Client()->SetChromeVoxEnabled(true);
  fake_service_->WaitForATChangeCount(1);
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kChromeVox));
  Client()->SetSelectToSpeakEnabled(true);
  fake_service_->WaitForATChangeCount(2);
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kSelectToSpeak));
  Client()->SetSwitchAccessEnabled(true);
  fake_service_->WaitForATChangeCount(3);
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kSwitchAccess));
  Client()->SetAutoclickEnabled(true);
  fake_service_->WaitForATChangeCount(4);
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kAutoClick));
  Client()->SetDictationEnabled(true);
  fake_service_->WaitForATChangeCount(5);
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kDictation));
  Client()->SetMagnifierEnabled(true);
  fake_service_->WaitForATChangeCount(6);
  EXPECT_TRUE(ServiceHasATEnabled(AssistiveTechnologyType::kMagnifier));
  Client()->SetChromeVoxEnabled(false);
  fake_service_->WaitForATChangeCount(7);
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kChromeVox));
  Client()->SetSelectToSpeakEnabled(false);
  fake_service_->WaitForATChangeCount(8);
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kSelectToSpeak));
  Client()->SetSwitchAccessEnabled(false);
  fake_service_->WaitForATChangeCount(9);
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kSwitchAccess));
  Client()->SetAutoclickEnabled(false);
  fake_service_->WaitForATChangeCount(10);
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kAutoClick));
  Client()->SetDictationEnabled(false);
  fake_service_->WaitForATChangeCount(11);
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kDictation));
  Client()->SetMagnifierEnabled(false);
  fake_service_->WaitForATChangeCount(12);
  EXPECT_FALSE(ServiceHasATEnabled(AssistiveTechnologyType::kMagnifier));
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       SendsAutomationToTheService) {
  // Enable an assistive technology. The service will not be started until
  // some AT needs it.
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);

  // The service may bind multiple Automations to the AutomationClient.
  for (int i = 0; i < 3; i++) {
    fake_service_->BindAnotherAutomation();
  }

  // TODO(crbug.com/1355633): Replace once mojom to Enable lands.
  ToggleAutomationEnabled(Client(), true);
  // Enable can be called multiple times (once for each bound Automation)
  // with no bad effects.
  // fake_service_->AutomationClientEnable(true);

  // Real accessibility events should have come through.
  fake_service_->WaitForAutomationEvents();

  // TODO(crbug.com/1355633): Replace once mojom to Disable lands.
  ToggleAutomationEnabled(Client(), false);
  // Disabling multiple times has no bad effect.
  // fake_service_->AutomationClientEnable(false);
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       DevToolsAgentHostCreated) {
  // Enable an assistive technology. The service will not be started until
  // some AT needs it.
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);

  // A single agent host should have been created for chromevox.
  auto count = fake_service_->GetDevtoolsConnectionCount(
      AssistiveTechnologyType::kChromeVox);
  EXPECT_EQ(count, 1);
  // Disable and re-enable
  Client()->SetChromeVoxEnabled(false);
  Client()->SetChromeVoxEnabled(true);
  count = fake_service_->GetDevtoolsConnectionCount(
      AssistiveTechnologyType::kChromeVox);
  EXPECT_EQ(count, 2);
  // Different AT
  Client()->SetSelectToSpeakEnabled(true);
  count = fake_service_->GetDevtoolsConnectionCount(
      AssistiveTechnologyType::kSelectToSpeak);
  EXPECT_EQ(count, 1);
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsGetVoices) {
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
  TurnOnAccessibilityService(AssistiveTechnologyType::kSelectToSpeak);
  test::SpeechMonitor sm;

  fake_service_->BindAnotherTts();
  fake_service_->RequestSpeak("Hello, world", base::DoNothing());
  sm.ExpectSpeech("Hello, world");
  sm.Replay();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, TtsSendsStartEndEvents) {
  test::SpeechMonitor sm;
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
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
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
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
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
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
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
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
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
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
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
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
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
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

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, DarkenScreen) {
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  fake_service_->BindAnotherUserInterface();

  base::RunLoop waiter;
  AccessibilityManager::Get()->SetScreenDarkenObserverForTest(
      base::BindLambdaForTesting([&waiter] {
        waiter.Quit();

        EXPECT_TRUE(
            chromeos::FakePowerManagerClient::Get()->backlights_forced_off());
      }));

  fake_service_->RequestDarkenScreen(true);

  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, OpenSettingsSubpage) {
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  fake_service_->BindAnotherUserInterface();

  base::RunLoop waiter;
  AccessibilityManager::Get()->SetOpenSettingsSubpageObserverForTest(
      base::BindLambdaForTesting([&waiter]() { waiter.Quit(); }));

  fake_service_->RequestOpenSettingsSubpage("manageAccessibility/tts");

  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       AcceptConfirmationDialog) {
  // Initialize ATP.
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  fake_service_->BindAnotherUserInterface();

  // Request confirmation dialog.
  base::RunLoop waiter;
  fake_service_->RequestShowConfirmationDialog(
      "Order Confirmation", "Ok to purchase 10,000 lbs of canned corn?",
      "No, thank you", base::BindLambdaForTesting([&waiter](bool confirmed) {
        waiter.Quit();
        EXPECT_TRUE(confirmed);
      }));

  // Wait for dialog shown.
  DialogShownWaiter().Wait();

  // Verify dialog was created.
  AccessibilityConfirmationDialog* dialog =
      Shell::Get()->accessibility_controller()->GetConfirmationDialogForTest();
  ASSERT_NE(dialog, nullptr);

  // Verify title.
  EXPECT_EQ(dialog->GetWindowTitle(), u"Order Confirmation");

  // Accept dialog.
  dialog->AcceptDialog();

  // Wait for dialog callback and closing.
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       CancelConfirmationDialog) {
  // Initialize ATP.
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  fake_service_->BindAnotherUserInterface();

  // Request confirmation dialog.
  base::RunLoop waiter;
  fake_service_->RequestShowConfirmationDialog(
      "Order Confirmation", "Ok to purchase 10,000 lbs of canned corn?",
      "No, thank you", base::BindLambdaForTesting([&waiter](bool confirmed) {
        waiter.Quit();
        EXPECT_FALSE(confirmed);
      }));

  // Wait for dialog shown.
  DialogShownWaiter().Wait();

  // Verify dialog was created.
  AccessibilityConfirmationDialog* dialog =
      Shell::Get()->accessibility_controller()->GetConfirmationDialogForTest();
  ASSERT_NE(dialog, nullptr);

  // Verify title.
  EXPECT_EQ(dialog->GetWindowTitle(), u"Order Confirmation");

  // Cancel dialog.
  dialog->CancelDialog();

  // Wait for dialog callback and closing.
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       CloseConfirmationDialog) {
  // Initialize ATP.
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  fake_service_->BindAnotherUserInterface();

  // Request confirmation dialog.
  base::RunLoop waiter;
  fake_service_->RequestShowConfirmationDialog(
      "Order Confirmation", "Ok to purchase 10,000 lbs of canned corn?",
      "No, thank you", base::BindLambdaForTesting([&waiter](bool confirmed) {
        waiter.Quit();
        EXPECT_FALSE(confirmed);
      }));

  // Wait for dialog shown.
  DialogShownWaiter().Wait();

  // Verify dialog was created.
  AccessibilityConfirmationDialog* dialog =
      Shell::Get()->accessibility_controller()->GetConfirmationDialogForTest();
  ASSERT_NE(dialog, nullptr);

  // Verify title.
  EXPECT_EQ(dialog->GetWindowTitle(), u"Order Confirmation");

  // Close dialog.
  dialog->GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);

  // Wait for callback and closing.
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       AutoCloseSecondConfirmationDialog) {
  // If a dialog is already being shown, we do not show a new one.
  // Instead, we return false through the callback on the new dialog
  // to indicate it was closed without the user taking any action.
  // (See the implementation in user_interface_impl.cc and
  // accessibility_controller_impl.cc)

  // Initialize ATP.
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  fake_service_->BindAnotherUserInterface();

  // Request first confirmation dialog.
  base::RunLoop waiter1;
  fake_service_->RequestShowConfirmationDialog(
      "Order Confirmation", "Ok to purchase 10,000 lbs of canned corn?",
      "No, thank you", base::BindLambdaForTesting([&waiter1](bool confirmed) {
        waiter1.Quit();
        EXPECT_TRUE(confirmed);
      }));

  // Wait for dialog shown.
  DialogShownWaiter().Wait();

  // Verify dialog was created.
  AccessibilityConfirmationDialog* dialog =
      Shell::Get()->accessibility_controller()->GetConfirmationDialogForTest();
  ASSERT_NE(dialog, nullptr);

  // Verify title.
  EXPECT_EQ(dialog->GetWindowTitle(), u"Order Confirmation");

  // Request second confirmation dialog.
  base::RunLoop waiter2;
  fake_service_->RequestShowConfirmationDialog(
      "Are we there yet?",
      "Do you confirm that the journey to the destination is completed?", "No",
      base::BindLambdaForTesting([&waiter2](bool confirmed) {
        waiter2.Quit();
        EXPECT_FALSE(confirmed);
      }));

  // Wait for second dialog to get automatically closed.
  waiter2.Run();

  // Verify current dialog is first dialog.
  dialog =
      Shell::Get()->accessibility_controller()->GetConfirmationDialogForTest();
  EXPECT_EQ(dialog->GetWindowTitle(), u"Order Confirmation");

  // Accept first dialog.
  dialog->AcceptDialog();

  // Wait for first dialog callback and closing.
  waiter1.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       OpenSecondConfirmationDialogAfterClosingFirst) {
  // Initialize ATP.
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  fake_service_->BindAnotherUserInterface();

  // Request first confirmation dialog.
  base::RunLoop waiter1;
  fake_service_->RequestShowConfirmationDialog(
      "Order Confirmation", "Ok to purchase 10,000 lbs of canned corn?",
      "No, thank you", base::BindLambdaForTesting([&waiter1](bool confirmed) {
        waiter1.Quit();
        EXPECT_TRUE(confirmed);
      }));

  // Wait for dialog shown.
  DialogShownWaiter().Wait();

  // Verify dialog was created.
  AccessibilityConfirmationDialog* dialog =
      Shell::Get()->accessibility_controller()->GetConfirmationDialogForTest();
  ASSERT_NE(dialog, nullptr);

  // Verify first dialog title.
  EXPECT_EQ(dialog->GetWindowTitle(), u"Order Confirmation");

  // Accept first dialog.
  dialog->AcceptDialog();

  // Wait for first dialog callback and closing.
  waiter1.Run();

  // Request second confirmation dialog.
  base::RunLoop waiter2;
  fake_service_->RequestShowConfirmationDialog(
      "Are we there yet?",
      "Do you confirm that the journey to the destination is completed?", "No",
      base::BindLambdaForTesting([&waiter2](bool confirmed) {
        waiter2.Quit();
        EXPECT_TRUE(confirmed);
      }));

  // Wait for second dialog shown.
  DialogShownWaiter().Wait();

  // Verify second dialog was created.
  dialog =
      Shell::Get()->accessibility_controller()->GetConfirmationDialogForTest();
  ASSERT_NE(dialog, nullptr);

  // Verify second dialog title.
  EXPECT_EQ(dialog->GetWindowTitle(), u"Are we there yet?");

  // Accept second dialog.
  dialog->AcceptDialog();

  // Wait for second dialog callback and closing.
  waiter2.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, SetFocusRings) {
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

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest, SetHighlights) {
      TurnOnAccessibilityService(AssistiveTechnologyType::kSwitchAccess);
  fake_service_->BindAnotherUserInterface();

  std::vector<gfx::Rect> rects;
  rects.emplace_back(gfx::Rect(0, 1, 22, 1973));

  base::RunLoop waiter;
  AccessibilityManager::Get()->SetHighlightsObserverForTest(
      base::BindLambdaForTesting([&waiter, &rects] {
        waiter.Quit();
        AccessibilityFocusRingControllerImpl* controller =
            Shell::Get()->accessibility_focus_ring_controller();
        AccessibilityHighlightLayer* highlight_layer =
            controller->highlight_layer_for_testing();
        EXPECT_TRUE(highlight_layer);
        ASSERT_EQ(1u, highlight_layer->rects_for_test().size());
        EXPECT_EQ(rects[0], highlight_layer->rects_for_test()[0]);
        EXPECT_EQ(SK_ColorMAGENTA, highlight_layer->color_for_test());
      }));

  fake_service_->RequestSetHighlights(rects, SK_ColorMAGENTA);

  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       RequestSetVirtualKeyboardVisible) {
  // Initialize ATP.
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  fake_service_->BindAnotherUserInterface();

  // Enable virtual keyboard.
  keyboard::SetAccessibilityKeyboardEnabled(true);

  // Verify keyboard is hidden.
  KeyboardControllerImpl* keyboard_controller_ =
      Shell::Get()->keyboard_controller();
  EXPECT_FALSE(keyboard_controller_->IsKeyboardVisible());

  // Show keyboard, and verify visible.
  fake_service_->RequestSetVirtualKeyboardVisible(true);
  KeyboardVisibleWaiter(true).Wait();
  EXPECT_TRUE(keyboard_controller_->IsKeyboardVisible());

  // Hide keyboard, and verify invisible.
  fake_service_->RequestSetVirtualKeyboardVisible(false);
  KeyboardVisibleWaiter(false).Wait();
  EXPECT_FALSE(keyboard_controller_->IsKeyboardVisible());
}

// Verifies that speech recognition can be started and stopped using the
// AccessibilityServiceClient.
IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       SpeechRecognitionStartAndStop) {
  TurnOnAccessibilityService(AssistiveTechnologyType::kDictation);
  fake_service_->BindAnotherSpeechRecognition();

  auto start_options = ax::mojom::StartOptions::New();
  start_options->type = ax::mojom::AssistiveTechnologyType::kDictation;
  fake_service_->RequestSpeechRecognitionStart(std::move(start_options),
                                               base::DoNothing());
  sr_test_helper_->WaitForRecognitionStarted();

  auto stop_options = ax::mojom::StopOptions::New();
  stop_options->type = ax::mojom::AssistiveTechnologyType::kDictation;
  fake_service_->RequestSpeechRecognitionStop(std::move(stop_options),
                                              base::DoNothing());
  sr_test_helper_->WaitForRecognitionStopped();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       SpeechRecognitionStartAndStopCallbacks) {
  TurnOnAccessibilityService(AssistiveTechnologyType::kDictation);
  fake_service_->BindAnotherSpeechRecognition();

  base::RunLoop start_waiter;
  auto start_options = ax::mojom::StartOptions::New();
  start_options->type = ax::mojom::AssistiveTechnologyType::kDictation;
  fake_service_->RequestSpeechRecognitionStart(
      std::move(start_options),
      base::BindLambdaForTesting(
          [&start_waiter](ax::mojom::SpeechRecognitionStartInfoPtr info) {
            EXPECT_EQ(ax::mojom::SpeechRecognitionType::kNetwork, info->type);
            ASSERT_FALSE(info->observer_or_error->is_error());
            start_waiter.Quit();
          }));
  start_waiter.Run();

  base::RunLoop stop_waiter;
  auto stop_options = ax::mojom::StopOptions::New();
  stop_options->type = ax::mojom::AssistiveTechnologyType::kDictation;
  fake_service_->RequestSpeechRecognitionStop(
      std::move(stop_options),
      base::BindLambdaForTesting(
          [&stop_waiter](const std::optional<std::string>& error) {
            ASSERT_FALSE(error.has_value());
            stop_waiter.Quit();
          }));
  stop_waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       AccessibilityServiceAsksClientToLoadAFile) {
  TurnOnAccessibilityService(AssistiveTechnologyType::kChromeVox);
  base::RunLoop loop;
  fake_service_->RequestLoadFile(
      base::FilePath("chromevox/common/closure_loader.js"),
      base::BindLambdaForTesting([&loop](base::File file) mutable {
        // Note: we post a task to the thread pool here because dealing with the
        // file causes blocking operations. Since this is a single process
        // browser test setup, this would run in the main UI thread, which can't
        // block. The destructor of the base::File here must be invoked in a
        // sequence that allows blocking (here, in the task itself). So we
        // return its value of is_valid() to be checked in the main thread, and
        // finally stop the loop.
        base::ThreadPool::PostTaskAndReplyWithResult(
            FROM_HERE, {base::MayBlock()},
            /*task=*/base::BindLambdaForTesting([file = std::move(file)]() {
              return file.IsValid();
            }),
            /*reply=*/
            base::BindLambdaForTesting(
                [quit_closure = loop.QuitClosure()](bool result) {
                  ASSERT_TRUE(result);
                  std::move(quit_closure).Run();
                }));
      }));
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       SendSyntheticKeyEventForShortcutOrNavigation) {
  TurnOnAccessibilityService(AssistiveTechnologyType::kSwitchAccess);
  fake_service_->BindAnotherUserInput();

  auto key_press_event = ax::mojom::SyntheticKeyEvent::New();
  key_press_event->type = ui::mojom::EventType::KEY_PRESSED;
  key_press_event->key_data = ui::mojom::KeyData::New();
  key_press_event->key_data->key_code = ui::VKEY_P;
  // TODO(b/307553499): Populate dom_code and dom_key for synthetic key events.
  key_press_event->key_data->dom_code = 0;
  key_press_event->key_data->dom_key = 0;
  key_press_event->key_data->is_char = false;

  auto key_release_event = ax::mojom::SyntheticKeyEvent::New();
  key_release_event->type = ui::mojom::EventType::KEY_RELEASED;
  key_release_event->key_data = ui::mojom::KeyData::New();
  key_release_event->key_data->key_code = ui::VKEY_P;
  // TODO(b/307553499): Populate dom_code and dom_key for synthetic key
  // events.
  key_release_event->key_data->dom_code = 0;
  key_release_event->key_data->dom_key = 0;
  key_release_event->key_data->is_char = false;

  base::RunLoop waiter;

  TestEventHandler test_event_handler(
      base::BindLambdaForTesting([&test_event_handler, &waiter]() {
        if (test_event_handler.key_events.size() != 2) {
          return;
        }

        ui::KeyEvent* press_event = test_event_handler.key_events[0].get();
        EXPECT_EQ(press_event->type(), ui::EventType::kKeyPressed);
        EXPECT_EQ(press_event->code(), ui::DomCode::US_P);
        EXPECT_FALSE(press_event->IsAltDown());
        EXPECT_FALSE(press_event->IsCommandDown());
        EXPECT_FALSE(press_event->IsControlDown());
        EXPECT_FALSE(press_event->IsShiftDown());

        ui::KeyEvent* release_event = test_event_handler.key_events[1].get();
        EXPECT_EQ(release_event->type(), ui::EventType::kKeyReleased);
        EXPECT_EQ(release_event->code(), ui::DomCode::US_P);
        EXPECT_FALSE(release_event->IsAltDown());
        EXPECT_FALSE(release_event->IsCommandDown());
        EXPECT_FALSE(release_event->IsControlDown());
        EXPECT_FALSE(release_event->IsShiftDown());

        waiter.Quit();
      }));

  // Send a press.
  fake_service_->RequestSendSyntheticKeyEventForShortcutOrNavigation(
      std::move(key_press_event));
  // Send a release.
  fake_service_->RequestSendSyntheticKeyEventForShortcutOrNavigation(
      std::move(key_release_event));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(
    AccessibilityServiceClientTest,
    SendSyntheticKeyEventForShortcutOrNavigationWithModifiers) {
  TurnOnAccessibilityService(AssistiveTechnologyType::kSwitchAccess);
  fake_service_->BindAnotherUserInput();

  auto key_press_event = ax::mojom::SyntheticKeyEvent::New();
  key_press_event->type = ui::mojom::EventType::KEY_PRESSED;
  key_press_event->key_data = ui::mojom::KeyData::New();
  key_press_event->key_data->key_code = ui::VKEY_S;
  // TODO(b/307553499): Populate dom_code and dom_key for synthetic key events.
  key_press_event->key_data->dom_code = 0;
  key_press_event->key_data->dom_key = 0;
  key_press_event->key_data->is_char = false;
  key_press_event->flags =
      ui::mojom::kEventFlagAltDown | ui::mojom::kEventFlagControlDown |
      ui::mojom::kEventFlagCommandDown | ui::mojom::kEventFlagShiftDown;

  auto key_release_event = ax::mojom::SyntheticKeyEvent::New();
  key_release_event->type = ui::mojom::EventType::KEY_RELEASED;
  key_release_event->key_data = ui::mojom::KeyData::New();
  key_release_event->key_data->key_code = ui::VKEY_S;
  // TODO(b/307553499): Populate dom_code and dom_key for synthetic key
  // events.
  key_release_event->key_data->dom_code = 0;
  key_release_event->key_data->dom_key = 0;
  key_release_event->key_data->is_char = false;
  key_release_event->flags =
      ui::mojom::kEventFlagAltDown | ui::mojom::kEventFlagControlDown |
      ui::mojom::kEventFlagCommandDown | ui::mojom::kEventFlagShiftDown;

  base::RunLoop waiter;

  TestEventHandler test_event_handler(
      base::BindLambdaForTesting([&test_event_handler, &waiter]() {
        if (test_event_handler.key_events.size() != 2) {
          return;
        }

        ui::KeyEvent* press_event = test_event_handler.key_events[0].get();
        EXPECT_EQ(press_event->type(), ui::EventType::kKeyPressed);
        EXPECT_EQ(press_event->code(), ui::DomCode::US_S);
        EXPECT_TRUE(press_event->IsAltDown());
        EXPECT_TRUE(press_event->IsCommandDown());
        EXPECT_TRUE(press_event->IsControlDown());
        EXPECT_TRUE(press_event->IsShiftDown());

        ui::KeyEvent* release_event = test_event_handler.key_events[1].get();
        EXPECT_EQ(release_event->type(), ui::EventType::kKeyReleased);
        EXPECT_EQ(release_event->code(), ui::DomCode::US_S);
        EXPECT_TRUE(release_event->IsAltDown());
        EXPECT_TRUE(release_event->IsCommandDown());
        EXPECT_TRUE(release_event->IsControlDown());
        EXPECT_TRUE(release_event->IsShiftDown());

        waiter.Quit();
      }));

  // Send a press.
  fake_service_->RequestSendSyntheticKeyEventForShortcutOrNavigation(
      std::move(key_press_event));
  // Send a release.
  fake_service_->RequestSendSyntheticKeyEventForShortcutOrNavigation(
      std::move(key_release_event));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       SendSyntheticMouseEventPress) {
  TurnOnAccessibilityService(AssistiveTechnologyType::kSwitchAccess);
  fake_service_->BindAnotherUserInput();

  auto mouse_event = ax::mojom::SyntheticMouseEvent::New();
  mouse_event->type = ui::mojom::EventType::MOUSE_PRESSED_EVENT;
  mouse_event->point = gfx::Point(0, 0);

  base::RunLoop waiter;

  TestEventHandler test_event_handler(base::BindLambdaForTesting([&]() {
    ASSERT_NE(0u, test_event_handler.mouse_events.size());
    ui::MouseEvent* mouse_event = test_event_handler.mouse_events.back().get();
    EXPECT_EQ(mouse_event->type(), ui::EventType::kMousePressed);
    EXPECT_FALSE(mouse_event->flags() & ui::EF_TOUCH_ACCESSIBILITY);
    EXPECT_TRUE(mouse_event->IsOnlyLeftMouseButton());
    waiter.Quit();
  }));

  fake_service_->RequestSendSyntheticMouseEvent(std::move(mouse_event));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       SendSyntheticMouseEventRelease) {
  TurnOnAccessibilityService(AssistiveTechnologyType::kSwitchAccess);
  fake_service_->BindAnotherUserInput();

  auto mouse_event = ax::mojom::SyntheticMouseEvent::New();
  mouse_event->type = ui::mojom::EventType::MOUSE_RELEASED_EVENT;
  mouse_event->point = gfx::Point(0, 0);
  mouse_event->mouse_button = ax::mojom::SyntheticMouseEventButton::kMiddle;
  mouse_event->touch_accessibility = false;

  base::RunLoop waiter;

  TestEventHandler test_event_handler(base::BindLambdaForTesting([&]() {
    ASSERT_NE(0u, test_event_handler.mouse_events.size());
    ui::MouseEvent* mouse_event = test_event_handler.mouse_events.back().get();
    EXPECT_EQ(mouse_event->type(), ui::EventType::kMouseReleased);
    EXPECT_FALSE(mouse_event->flags() & ui::EF_TOUCH_ACCESSIBILITY);
    EXPECT_EQ(mouse_event->button_flags(), ui::EF_MIDDLE_MOUSE_BUTTON);
    waiter.Quit();
  }));

  fake_service_->RequestSendSyntheticMouseEvent(std::move(mouse_event));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       SendSyntheticMouseEventDrag) {
  TurnOnAccessibilityService(AssistiveTechnologyType::kSwitchAccess);
  fake_service_->BindAnotherUserInput();

  auto mouse_event = ax::mojom::SyntheticMouseEvent::New();
  mouse_event->type = ui::mojom::EventType::MOUSE_DRAGGED_EVENT;
  mouse_event->point = gfx::Point(0, 0);
  mouse_event->mouse_button = ax::mojom::SyntheticMouseEventButton::kRight;
  mouse_event->touch_accessibility = true;

  base::RunLoop waiter;

  TestEventHandler test_event_handler(base::BindLambdaForTesting([&]() {
    ASSERT_NE(0u, test_event_handler.mouse_events.size());
    ui::MouseEvent* mouse_event = test_event_handler.mouse_events.back().get();
    EXPECT_EQ(mouse_event->type(), ui::EventType::kMouseDragged);
    EXPECT_TRUE(mouse_event->flags() & ui::EF_TOUCH_ACCESSIBILITY);
    EXPECT_EQ(mouse_event->button_flags(), ui::EF_RIGHT_MOUSE_BUTTON);
    waiter.Quit();
  }));

  fake_service_->RequestSendSyntheticMouseEvent(std::move(mouse_event));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       SendSyntheticMouseEventMove) {
  TurnOnAccessibilityService(AssistiveTechnologyType::kSwitchAccess);
  fake_service_->BindAnotherUserInput();

  auto mouse_event = ax::mojom::SyntheticMouseEvent::New();
  mouse_event->type = ui::mojom::EventType::MOUSE_MOVED_EVENT;
  mouse_event->point = gfx::Point(0, 0);

  base::RunLoop waiter;

  TestEventHandler test_event_handler(base::BindLambdaForTesting([&]() {
    ASSERT_NE(0u, test_event_handler.mouse_events.size());
    ui::MouseEvent* mouse_event = test_event_handler.mouse_events.back().get();
    // We may see an enter event fired before the actual move event.
    if (mouse_event->type() == ui::EventType::kMouseEntered) {
      return;
    }

    EXPECT_EQ(mouse_event->type(), ui::EventType::kMouseMoved);
    EXPECT_FALSE(mouse_event->flags() & ui::EF_TOUCH_ACCESSIBILITY);
    EXPECT_EQ(mouse_event->button_flags(), 0);
    waiter.Quit();
  }));

  fake_service_->RequestSendSyntheticMouseEvent(std::move(mouse_event));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       SendSyntheticMouseEventEnter) {
  TurnOnAccessibilityService(AssistiveTechnologyType::kSwitchAccess);
  fake_service_->BindAnotherUserInput();

  auto mouse_event = ax::mojom::SyntheticMouseEvent::New();
  mouse_event->type = ui::mojom::EventType::MOUSE_ENTERED_EVENT;
  mouse_event->point = gfx::Point(0, 0);
  mouse_event->mouse_button = ax::mojom::SyntheticMouseEventButton::kBack;

  base::RunLoop waiter;

  TestEventHandler test_event_handler(base::BindLambdaForTesting([&]() {
    ASSERT_NE(0u, test_event_handler.mouse_events.size());
    ui::MouseEvent* mouse_event = test_event_handler.mouse_events.back().get();
    EXPECT_EQ(mouse_event->type(), ui::EventType::kMouseEntered);
    EXPECT_FALSE(mouse_event->flags() & ui::EF_TOUCH_ACCESSIBILITY);
    EXPECT_EQ(mouse_event->button_flags(), ui::EF_BACK_MOUSE_BUTTON);
    waiter.Quit();
  }));

  fake_service_->RequestSendSyntheticMouseEvent(std::move(mouse_event));
  waiter.Run();
}

IN_PROC_BROWSER_TEST_F(AccessibilityServiceClientTest,
                       SendSyntheticMouseEventExit) {
  TurnOnAccessibilityService(AssistiveTechnologyType::kSwitchAccess);
  fake_service_->BindAnotherUserInput();

  auto mouse_event = ax::mojom::SyntheticMouseEvent::New();
  mouse_event->type = ui::mojom::EventType::MOUSE_EXITED_EVENT;
  mouse_event->point = gfx::Point(0, 0);
  mouse_event->mouse_button = ax::mojom::SyntheticMouseEventButton::kForward;

  base::RunLoop waiter;

  TestEventHandler test_event_handler(base::BindLambdaForTesting([&]() {
    ASSERT_NE(0u, test_event_handler.mouse_events.size());
    ui::MouseEvent* mouse_event = test_event_handler.mouse_events.back().get();
    EXPECT_EQ(mouse_event->type(), ui::EventType::kMouseExited);
    EXPECT_FALSE(mouse_event->flags() & ui::EF_TOUCH_ACCESSIBILITY);
    EXPECT_EQ(mouse_event->button_flags(), ui::EF_FORWARD_MOUSE_BUTTON);
    waiter.Quit();
  }));

  fake_service_->RequestSendSyntheticMouseEvent(std::move(mouse_event));
  waiter.Run();
}

}  // namespace ash
