// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/native_input_method_engine.h"

#include "ash/constants/ash_features.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "chrome/browser/chromeos/input_method/stub_input_method_engine_observer.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/services/ime/mock_input_channel.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/mock_ime_input_context_handler.h"
#include "ui/base/ime/chromeos/mock_input_method_manager.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace chromeos {
namespace {

MATCHER_P(MojoEq, value, "") {
  return *arg == value;
}

using input_method::InputMethodManager;
using input_method::StubInputMethodEngineObserver;
using testing::_;
using testing::NiceMock;
using testing::StrictMock;

constexpr char kEngineIdUs[] = "xkb:us::eng";

void SetPhysicalTypingAutocorrectEnabled(TestingProfile& profile,
                                         bool enabled) {
  base::Value input_method_setting(base::Value::Type::DICTIONARY);
  input_method_setting.SetPath(
      std::string(kEngineIdUs) + ".physicalKeyboardAutoCorrectionLevel",
      base::Value(enabled ? 1 : 0));
  profile.GetPrefs()->Set(prefs::kLanguageInputMethodSpecificSettings,
                          input_method_setting);
}

class TestInputEngineManager : public ime::mojom::InputEngineManager {
 public:
  TestInputEngineManager(ime::mojom::InputChannel* engine,
                         mojo::Remote<ime::mojom::InputChannel>* remote)
      : receiver_(engine), remote_(remote) {}

  void ConnectToImeEngine(
      const std::string& ime_spec,
      mojo::PendingReceiver<ime::mojom::InputChannel> to_engine_request,
      mojo::PendingRemote<ime::mojom::InputChannel> from_engine,
      const std::vector<uint8_t>& extra,
      ConnectToImeEngineCallback callback) override {
    receiver_.Bind(std::move(to_engine_request));
    if (remote_) {
      remote_->Bind(std::move(from_engine));
    }
    std::move(callback).Run(/*bound=*/true);
  }

 private:
  mojo::Receiver<ime::mojom::InputChannel> receiver_;
  mojo::Remote<ime::mojom::InputChannel>* remote_;
};

class TestInputMethodManager : public input_method::MockInputMethodManager {
 public:
  // TestInputMethodManager is responsible for connecting
  // NativeInputMethodEngine with an InputChannel.
  TestInputMethodManager(
      ime::mojom::InputChannel* engine,
      mojo::Remote<ime::mojom::InputChannel>* remote = nullptr)
      : test_input_engine_manager_(engine, remote),
        receiver_(&test_input_engine_manager_) {}

  void ConnectInputEngineManager(
      mojo::PendingReceiver<ime::mojom::InputEngineManager> receiver) override {
    receiver_.Bind(std::move(receiver));
  }

 private:
  TestInputEngineManager test_input_engine_manager_;
  mojo::Receiver<ime::mojom::InputEngineManager> receiver_;
};

// TODO(crbug.com/1148157): Refactor NativeInputMethodEngine etc. to avoid
// hidden dependencies on globals such as ImeBridge.
class NativeInputMethodEngineTest : public ::testing::Test {
  void SetUp() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAssistPersonalInfo,
                              features::kAssistPersonalInfoEmail,
                              features::kAssistPersonalInfoName,
                              features::kEmojiSuggestAddition,
                              features::kSystemLatinPhysicalTyping},
        /*disabled_features=*/{});

    // Needed by NativeInputMethodEngine to interact with the input field.
    ui::IMEBridge::Initialize();

    // Needed by NativeInputMethodEngine for the virtual keyboard.
    keyboard_controller_client_test_helper_ =
        ChromeKeyboardControllerClientTestHelper::InitializeWithFake();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
      keyboard_controller_client_test_helper_;
};

TEST_F(NativeInputMethodEngineTest, DoesNotLaunchImeServiceIfAutocorrectIsOff) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, false);

  testing::StrictMock<ime::MockInputChannel> mock_input_channel;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_channel));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  engine.Enable(kEngineIdUs);
  EXPECT_FALSE(engine.IsConnectedForTesting());

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, LaunchesImeServiceIfAutocorrectIsOn) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);

  testing::StrictMock<ime::MockInputChannel> mock_input_channel;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_channel));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  engine.Enable(kEngineIdUs);
  EXPECT_TRUE(engine.IsConnectedForTesting());

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, TogglesImeServiceWhenAutocorrectChanges) {
  TestingProfile testing_profile;
  testing::StrictMock<ime::MockInputChannel> mock_input_channel;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_channel));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);
  engine.Enable(kEngineIdUs);

  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);
  EXPECT_TRUE(engine.IsConnectedForTesting());
  SetPhysicalTypingAutocorrectEnabled(testing_profile, false);
  EXPECT_FALSE(engine.IsConnectedForTesting());

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, EnableCallsRightMojoFunctions) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);

  testing::StrictMock<ime::MockInputChannel> mock_input_channel;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_channel));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  EXPECT_CALL(mock_input_channel, OnInputMethodChanged(kEngineIdUs));

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, FocusCallsRightMojoFunctions) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);

  testing::StrictMock<ime::MockInputChannel> mock_input_channel;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_channel));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_channel, OnInputMethodChanged(kEngineIdUs));
    EXPECT_CALL(mock_input_channel,
                OnFocus(MojoEq(ime::mojom::InputFieldInfo(
                    ime::mojom::InputFieldType::kText,
                    ime::mojom::AutocorrectMode::kEnabled,
                    ime::mojom::PersonalizationMode::kEnabled))));
  }

  ui::IMEEngineHandlerInterface::InputContext input_context(
      ui::TEXT_INPUT_TYPE_TEXT, ui::TEXT_INPUT_MODE_DEFAULT,
      ui::TEXT_INPUT_FLAG_NONE, ui::TextInputClient::FOCUS_REASON_MOUSE,
      /*should_do_learning=*/true);
  engine.Enable(kEngineIdUs);
  engine.FocusIn(input_context);
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, HandleAutocorrectChangesAutocorrectRange) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);

  testing::NiceMock<ime::MockInputChannel> mock_input_channel;
  mojo::Remote<ime::mojom::InputChannel> remote;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_channel, &remote));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);
  ui::IMEEngineHandlerInterface::InputContext input_context(
      ui::TEXT_INPUT_TYPE_TEXT, ui::TEXT_INPUT_MODE_DEFAULT,
      ui::TEXT_INPUT_FLAG_NONE, ui::TextInputClient::FOCUS_REASON_MOUSE,
      /*should_do_learning=*/true);
  engine.Enable(kEngineIdUs);
  engine.FocusIn(input_context);
  engine.FlushForTesting();
  ui::MockIMEInputContextHandler mock_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_handler);

  remote->HandleAutocorrect(
      ime::mojom::AutocorrectSpan::New(gfx::Range(0, 5), "teh", "the"));
  engine.FlushForTesting();

  EXPECT_EQ(mock_handler.GetAutocorrectRange(), gfx::Range(0, 5));

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest,
       SurroundingTextChangeConvertsToUtf8Correctly) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);

  testing::StrictMock<ime::MockInputChannel> mock_input_channel;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_channel));
  ui::MockIMEInputContextHandler mock_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_handler);
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_channel, OnInputMethodChanged(kEngineIdUs));
    EXPECT_CALL(mock_input_channel, OnFocus(_));
    // Each character in "你好" is three UTF-8 code units.
    EXPECT_CALL(mock_input_channel,
                OnSurroundingTextChanged(u8"你好",
                                         /*offset=*/0,
                                         MojoEq(ime::mojom::SelectionRange(
                                             /*anchor=*/6, /*focus=*/6))));
  }

  engine.Enable(kEngineIdUs);
  engine.FocusIn(ui::IMEEngineHandlerInterface::InputContext(
      ui::TEXT_INPUT_TYPE_TEXT, ui::TEXT_INPUT_MODE_DEFAULT,
      ui::TEXT_INPUT_FLAG_NONE, ui::TextInputClient::FOCUS_REASON_MOUSE,
      /*should_do_learning=*/true));
  // Each character in "你好" is one UTF-16 code unit.
  engine.SetSurroundingText(u"你好",
                            /*cursor_pos=*/2,
                            /*anchor_pos=*/2,
                            /*offset=*/0);
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, ProcessesDeadKeysCorrectly) {
  TestingProfile testing_profile;
  SetPhysicalTypingAutocorrectEnabled(testing_profile, true);

  testing::StrictMock<ime::MockInputChannel> mock_input_channel;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_channel));
  ui::MockIMEInputContextHandler mock_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_handler);
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_channel, OnInputMethodChanged(kEngineIdUs));
    EXPECT_CALL(mock_input_channel, OnFocus(_));

    // TODO(https://crbug.com/1187982): Expect the actual arguments to the call
    // once the Mojo API is replaced with protos. GMock does not play well with
    // move-only types like PhysicalKeyEvent.
    EXPECT_CALL(mock_input_channel, OnKeyEvent(_, _))
        .Times(2)
        .WillRepeatedly(::testing::Invoke(
            [](ime::mojom::PhysicalKeyEventPtr,
               ime::mojom::InputChannel::OnKeyEventCallback callback) {
              std::move(callback).Run(false);
            }));
  }

  engine.Enable(kEngineIdUs);
  engine.FocusIn(ui::IMEEngineHandlerInterface::InputContext(
      ui::TEXT_INPUT_TYPE_TEXT, ui::TEXT_INPUT_MODE_DEFAULT,
      ui::TEXT_INPUT_FLAG_NONE, ui::TextInputClient::FOCUS_REASON_MOUSE,
      /*should_do_learning=*/true));

  // Quote ("VKEY_OEM_7") + A is a dead key combination.
  engine.ProcessKeyEvent(
      {ui::ET_KEY_PRESSED, ui::VKEY_OEM_7, ui::DomCode::QUOTE, ui::EF_NONE,
       ui::DomKey::DeadKeyFromCombiningCharacter(u'\u0301'), base::TimeTicks()},
      base::DoNothing());
  engine.ProcessKeyEvent(
      {ui::ET_KEY_RELEASED, ui::VKEY_OEM_7, ui::DomCode::QUOTE, ui::EF_NONE,
       ui::DomKey::DeadKeyFromCombiningCharacter(u'\u0301'), base::TimeTicks()},
      base::DoNothing());
  engine.ProcessKeyEvent({ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE},
                         base::DoNothing());
  engine.ProcessKeyEvent({ui::ET_KEY_RELEASED, ui::VKEY_A, ui::EF_NONE},
                         base::DoNothing());
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

}  // namespace
}  // namespace chromeos
