// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/native_input_method_engine.h"

#include "base/feature_list.h"
#include "chrome/browser/chromeos/input_method/stub_input_method_engine_observer.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/ime/mock_input_channel.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/mock_input_method_manager.h"
#include "ui/base/ime/text_input_flags.h"

namespace chromeos {
namespace {

MATCHER_P(MojoEq, value, "") {
  return *arg == value;
}

using input_method::InputMethodManager;
using input_method::StubInputMethodEngineObserver;
using testing::StrictMock;

constexpr char kEngineIdUs[] = "xkb:us::eng";

class TestInputEngineManager : public ime::mojom::InputEngineManager {
 public:
  explicit TestInputEngineManager(ime::mojom::InputChannel* engine)
      : receiver_(engine) {}

  void ConnectToImeEngine(
      const std::string& ime_spec,
      mojo::PendingReceiver<ime::mojom::InputChannel> to_engine_request,
      mojo::PendingRemote<ime::mojom::InputChannel> from_engine,
      const std::vector<uint8_t>& extra,
      ConnectToImeEngineCallback callback) override {
    receiver_.Bind(std::move(to_engine_request));
    remote_.Bind(std::move(from_engine));
    std::move(callback).Run(/*bound=*/true);
  }

 private:
  mojo::Receiver<ime::mojom::InputChannel> receiver_;
  mojo::Remote<ime::mojom::InputChannel> remote_;
};

class TestInputMethodManager : public input_method::MockInputMethodManager {
 public:
  explicit TestInputMethodManager(ime::mojom::InputChannel* engine)
      : test_input_engine_manager_(engine),
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
    keyboard_controller_client_ =
        ChromeKeyboardControllerClient::CreateForTest();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ChromeKeyboardControllerClient> keyboard_controller_client_;
};

TEST_F(NativeInputMethodEngineTest, EnableCallsRightMojoFunctions) {
  testing::StrictMock<ime::MockInputChannel> mock_input_channel;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_channel));
  NativeInputMethodEngine engine;
  TestingProfile testing_profile;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  EXPECT_CALL(mock_input_channel, OnInputMethodChanged(kEngineIdUs));

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, FocusCallsRightMojoFunctions) {
  testing::StrictMock<ime::MockInputChannel> mock_input_channel;
  input_method::InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_channel));
  NativeInputMethodEngine engine;
  TestingProfile testing_profile;
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

}  // namespace
}  // namespace chromeos
