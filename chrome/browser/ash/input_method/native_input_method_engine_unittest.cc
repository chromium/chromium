// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/native_input_method_engine.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/input_method/autocorrect_prefs.h"
#include "chrome/browser/ash/input_method/input_method_settings.h"
#include "chrome/browser/ash/input_method/stub_input_method_engine_observer.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client_test_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/services/ime/public/cpp/autocorrect.h"
#include "chromeos/ash/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/japanese_settings.mojom.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/fake_ime_keyboard.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/ash/mock_ime_input_context_handler.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {
namespace {

MATCHER_P(MojoEq, value, "") {
  return *arg == value;
}

using ::testing::_;
using ::testing::NiceMock;
using ::testing::StrictMock;

using ime::AutocorrectSuggestionProvider;
using OnFocusCallback = ime::mojom::InputMethod::OnFocusCallback;

constexpr char kEngineIdUs[] = "xkb:us::eng";
constexpr char kEngineIdEs[] = "xkb:es::spa";
constexpr char kEngineIdPinyin[] = "zh-t-i0-pinyin";

// This is a local copy of the JapaneseStartupActions used privately by
// NativeInputMethodObserver.
enum class JapaneseStartupAction {
  kStillLegacy = 0,
  kAlreadyMigrated = 1,
  kPerformMigration = 2,
  kUndoMigration = 3,
  kMaxValue = kUndoMigration
};

class FakeSuggesterSwitch : public AssistiveSuggesterSwitch {
 public:
  explicit FakeSuggesterSwitch(EnabledSuggestions enabled_suggestions)
      : enabled_suggestions_(enabled_suggestions) {}
  ~FakeSuggesterSwitch() override = default;

  // AssistiveSuggesterSwitch overrides
  void FetchEnabledSuggestionsThen(
      FetchEnabledSuggestionsCallback callback,
      const TextInputMethod::InputContext& context) override {
    std::move(callback).Run(enabled_suggestions_);
  }

 private:
  EnabledSuggestions enabled_suggestions_;
};

class MockInputMethod : public ime::mojom::InputMethod {
 public:
  void Bind(
      mojo::PendingAssociatedReceiver<ime::mojom::InputMethod> receiver,
      mojo::PendingAssociatedRemote<ime::mojom::InputMethodHost> pending_host) {
    receiver_.Bind(std::move(receiver));
    host.Bind(std::move(pending_host));
  }

  // ime::mojom::InputMethod:
  MOCK_METHOD(void,
              OnFocusDeprecated,
              (ime::mojom::InputFieldInfoPtr input_field_info,
               ime::mojom::InputMethodSettingsPtr settings),
              (override));
  MOCK_METHOD(void,
              OnFocus,
              (ime::mojom::InputFieldInfoPtr input_field_info,
               ime::mojom::InputMethodSettingsPtr settings,
               OnFocusCallback callback),
              (override));
  MOCK_METHOD(void, OnBlur, (), (override));
  MOCK_METHOD(void,
              ProcessKeyEvent,
              (const ime::mojom::PhysicalKeyEventPtr event,
               ProcessKeyEventCallback),
              (override));
  MOCK_METHOD(void,
              OnSurroundingTextChanged,
              (const std::string& text,
               uint32_t offset,
               ime::mojom::SelectionRangePtr selection_range),
              (override));
  MOCK_METHOD(void,
              OnCandidateSelected,
              (uint32_t selected_candidate_index),
              (override));
  MOCK_METHOD(void, OnCompositionCanceledBySystem, (), (override));
  MOCK_METHOD(void,
              OnQuickSettingsUpdated,
              (ime::mojom::InputMethodQuickSettingsPtr quick_settings),
              (override));
  MOCK_METHOD(void,
              OnAssistiveWindowChanged,
              (const ash::ime::AssistiveWindow& window),
              (override));
  MOCK_METHOD(void,
              IsReadyForTesting,
              (IsReadyForTestingCallback callback),
              (override));

  mojo::AssociatedRemote<ime::mojom::InputMethodHost> host;

 private:
  mojo::AssociatedReceiver<ime::mojom::InputMethod> receiver_{this};
};

void SetEmptyPrefs(Profile& profile) {
  profile.GetPrefs()->SetDict(::prefs::kLanguageInputMethodSpecificSettings,
                              base::Value::Dict());
}

void SetInputMethodOptions(Profile& profile,
                           bool autocorrect_enabled,
                           bool predictive_writing_enabled) {
  base::Value::Dict input_method_setting;
  input_method_setting.SetByDottedPath(
      std::string(kEngineIdUs) + ".physicalKeyboardAutoCorrectionLevel",
      autocorrect_enabled ? 1 : 0);
  input_method_setting.SetByDottedPath(
      std::string(kEngineIdUs) + ".physicalKeyboardEnablePredictiveWriting",
      base::Value(predictive_writing_enabled));
  profile.GetPrefs()->SetDict(::prefs::kLanguageInputMethodSpecificSettings,
                              std::move(input_method_setting));
}

void SetPinyinLayoutPrefs(Profile& profile, const std::string& layout) {
  base::Value::Dict input_method_setting;
  input_method_setting.SetByDottedPath("zh-t-i0-pinyin.xkbLayout", layout);
  profile.GetPrefs()->SetDict(::prefs::kLanguageInputMethodSpecificSettings,
                              std::move(input_method_setting));
}

ime::mojom::InputMethodMetadataPtr EmptyInputMethodMetadata() {
  return ime::mojom::InputMethodMetadataPtr(nullptr);
}

std::vector<base::test::FeatureRef> DisabledFeatures() {
  return {ash::features::kImeRuleConfig};
}

class FakeConnectionFactory : public ime::mojom::ConnectionFactory {
 public:
  explicit FakeConnectionFactory(MockInputMethod* mock_input_method)
      : mock_input_method_(mock_input_method) {}

  // overrides ime::mojom::ConnectionFactory
  void ConnectToInputMethod(
      const std::string& ime_spec,
      mojo::PendingAssociatedReceiver<ime::mojom::InputMethod> input_method,
      mojo::PendingAssociatedRemote<ime::mojom::InputMethodHost>
          input_method_host,
      ime::mojom::InputMethodSettingsPtr settings,
      ConnectToInputMethodCallback callback) override {
    mock_input_method_->Bind(std::move(input_method),
                             std::move(input_method_host));
    std::move(callback).Run(/*bound=*/true);
  }

  void Unused(
      mojo::PendingAssociatedReceiver<ime::mojom::JpUnused> japanese_decoder,
      UnusedCallback callback) override {
    std::move(callback).Run(true);
  }

  void Bind(mojo::PendingReceiver<ime::mojom::ConnectionFactory> receiver) {
    connection_factory_.Bind(std::move(receiver));
  }

 private:
  raw_ptr<MockInputMethod> mock_input_method_;
  mojo::Receiver<ime::mojom::ConnectionFactory> connection_factory_{this};
};

class TestInputEngineManager : public ime::mojom::InputEngineManager {
 public:
  explicit TestInputEngineManager(MockInputMethod* mock_input_method)
      : fake_connection_factory_(mock_input_method) {}

  void ConnectToImeEngine(
      const std::string& ime_spec,
      mojo::PendingReceiver<ime::mojom::InputChannel> to_engine_request,
      mojo::PendingRemote<ime::mojom::InputChannel> from_engine,
      const std::vector<uint8_t>& extra,
      ConnectToImeEngineCallback callback) override {
    // Not used by NativeInputMethodEngine.
    std::move(callback).Run(/*bound=*/false);
  }

  void InitializeConnectionFactory(
      mojo::PendingReceiver<ime::mojom::ConnectionFactory> connection_factory,
      InitializeConnectionFactoryCallback callback) override {
    fake_connection_factory_.Bind(std::move(connection_factory));
    std::move(callback).Run(/*bound=*/true);
  }

 private:
  FakeConnectionFactory fake_connection_factory_;
};

class TestInputMethodManager : public MockInputMethodManager {
 public:
  // TestInputMethodManager is responsible for connecting
  // NativeInputMethodEngine with an InputMethod.
  explicit TestInputMethodManager(MockInputMethod* mock_input_method)
      : test_input_engine_manager_(mock_input_method),
        receiver_(&test_input_engine_manager_) {}

  void ConnectInputEngineManager(
      mojo::PendingReceiver<ime::mojom::InputEngineManager> receiver) override {
    receiver_.Bind(std::move(receiver));
  }

  ImeKeyboard* GetImeKeyboard() override { return &ime_keyboard_; }

 private:
  FakeImeKeyboard ime_keyboard_;
  TestInputEngineManager test_input_engine_manager_;
  mojo::Receiver<ime::mojom::InputEngineManager> receiver_;
};

class NativeInputMethodEngineTest : public ::testing::Test {
 public:
  void SetUp() override {
    EnableDefaultFeatureList();

    // Needed by NativeInputMethodEngine for the virtual keyboard.
    keyboard_controller_client_test_helper_ =
        ChromeKeyboardControllerClientTestHelper::InitializeWithFake();

    chromeos::machine_learning::ServiceConnection::
        UseFakeServiceConnectionForTesting(&fake_service_connection_);
    chromeos::machine_learning::ServiceConnection::GetInstance()->Initialize();
  }

  // TODO(b/264817001): Refactor EnableDefaultFeature*() functions to be
  // a single function. This was not done in a parent to keep the change
  // simple to review.
  void EnableDefaultFeatureList() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/DisabledFeatures());
  }

  void EnableDefaultFeatureListWithMultiWord() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            features::kAssistMultiWord,
        },
        /*disabled_features=*/DisabledFeatures());
  }

  void EnableDefaultFeatureListWithMultiwordDisabled() {
    feature_list_.Reset();

    std::vector<base::test::FeatureRef> disabled_features = DisabledFeatures();
    disabled_features.push_back(features::kAssistMultiWord);

    feature_list_.InitWithFeatures(/*enabled_features=*/{},
                                   /*disabled_features=*/disabled_features);
  }

  void EnableDefaultFeatureListWithJapaneseSystemPk() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            features::kSystemJapanesePhysicalTyping,
        },
        /*disabled_features=*/DisabledFeatures());
  }

  base::test::ScopedFeatureList feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
      keyboard_controller_client_test_helper_;
  chromeos::machine_learning::FakeServiceConnectionImpl
      fake_service_connection_;
};

TEST_F(NativeInputMethodEngineTest,
       DoesNotLaunchImeServiceIfAutocorrectAndPredictiveWritingAreOff) {
  TestingProfile testing_profile;
  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/false,
                        /*predictive_writing_enabled=*/false);

  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();  // ensure input_method is connected.
  EXPECT_FALSE(engine.IsConnectedForTesting());

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, LaunchesImeServiceIfAutocorrectIsOn) {
  TestingProfile testing_profile;
  SetEmptyPrefs(testing_profile);
  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/true,
                        /*predictive_writing_enabled=*/false);

  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();  // ensure input_method is connected.
  EXPECT_TRUE(engine.IsConnectedForTesting());

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest,
       PredictiveWritingDoesNotLaunchImeServiceWithMultiWordFlagDisabled) {
  TestingProfile testing_profile;
  SetEmptyPrefs(testing_profile);
  EnableDefaultFeatureListWithMultiwordDisabled();
  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/false,
                        /*predictive_writing_enabled=*/true);

  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();  // ensure input_method is connected.
  EXPECT_FALSE(engine.IsConnectedForTesting());

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest,
       PredictiveWritingDoesNotLaunchImeServiceWithNonEnUsEngineId) {
  TestingProfile testing_profile;
  SetEmptyPrefs(testing_profile);
  EnableDefaultFeatureListWithMultiWord();
  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/false,
                        /*predictive_writing_enabled=*/true);

  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  engine.Enable(kEngineIdEs);
  engine.FlushForTesting();  // ensure input_method is connected.
  EXPECT_FALSE(engine.IsConnectedForTesting());

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest,
       PredictiveWritingLaunchesImeServiceWithEnglishEngineId) {
  TestingProfile testing_profile;
  SetEmptyPrefs(testing_profile);
  EnableDefaultFeatureListWithMultiWord();
  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/false,
                        /*predictive_writing_enabled=*/true);

  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();  // ensure input_method connected.
  EXPECT_TRUE(engine.IsConnectedForTesting());

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, TogglesImeServiceWhenAutocorrectChanges) {
  TestingProfile testing_profile;
  SetEmptyPrefs(testing_profile);

  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAutocorrectByDefault,
                             features::kAssistMultiWord});

  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);
  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();  // ensure input_method is connected.

  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/true,
                        /*predictive_writing_enabled=*/false);
  engine.FlushForTesting();
  EXPECT_TRUE(engine.IsConnectedForTesting());

  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/false,
                        /*predictive_writing_enabled=*/false);
  engine.FlushForTesting();
  EXPECT_FALSE(engine.IsConnectedForTesting());

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, EnableInitializesConnection) {
  TestingProfile testing_profile;
  SetEmptyPrefs(testing_profile);
  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/true,
                        /*predictive_writing_enabled=*/false);

  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();

  EXPECT_TRUE(engine.IsConnectedForTesting());
  EXPECT_TRUE(mock_input_method.host.is_bound());

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, FocusCallsRightMojoFunctions) {
  TestingProfile testing_profile;
  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/true,
                        /*predictive_writing_enabled=*/false);

  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_method,
                OnFocus(MojoEq(ime::mojom::InputFieldInfo(
                            ime::mojom::InputFieldType::kText,
                            ime::mojom::AutocorrectMode::kEnabled,
                            ime::mojom::PersonalizationMode::kDisabled,
                            ime::mojom::TextPredictionMode::kDisabled)),
                        _, _))
        .WillOnce(
            ::testing::Invoke([](ime::mojom::InputFieldInfoPtr info,
                                 ime::mojom::InputMethodSettingsPtr settings,
                                 OnFocusCallback callback) {
              EXPECT_EQ(*settings,
                        *ime::mojom::InputMethodSettings::NewLatinSettings(
                            ime::mojom::LatinSettings::New(
                                /*autocorrect=*/true,
                                /*predictive_writing=*/false)));
              std::move(callback).Run(true, EmptyInputMethodMetadata());
            }));
    EXPECT_CALL(mock_input_method, OnSurroundingTextChanged(_, _, _));
  }

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();  // ensure input_method connected.
  engine.Focus(TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest,
       DisablesAutocorrectAndLearningAndIMEAtLockScreen) {
  TestingProfile testing_profile;
  SetEmptyPrefs(testing_profile);
  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/true,
                        /*predictive_writing_enabled=*/false);

  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  InputMethodManager::Get()->GetActiveIMEState()->SetUIStyle(
      InputMethodManager::UIStyle::kLock);
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_method,
                OnFocus(MojoEq(ime::mojom::InputFieldInfo(
                            ime::mojom::InputFieldType::kNoIME,
                            ime::mojom::AutocorrectMode::kDisabled,
                            ime::mojom::PersonalizationMode::kDisabled)),
                        _, _))
        .WillOnce(
            ::testing::Invoke([](ime::mojom::InputFieldInfoPtr info,
                                 ime::mojom::InputMethodSettingsPtr settings,
                                 OnFocusCallback callback) {
              EXPECT_EQ(*settings,
                        *ime::mojom::InputMethodSettings::NewLatinSettings(
                            ime::mojom::LatinSettings::New(
                                /*autocorrect=*/true,
                                /*predictive_writing=*/false)));
              std::move(callback).Run(true, EmptyInputMethodMetadata());
            }));
    EXPECT_CALL(mock_input_method, OnSurroundingTextChanged(_, _, _));
  }

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();  // ensure input_method connected.
  engine.Focus(TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));
  engine.FlushForTesting();

  InputMethodManager::Get()->GetActiveIMEState()->SetUIStyle(
      InputMethodManager::UIStyle::kNormal);
  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, FocusUpdatesXkbLayout) {
  TestingProfile testing_profile;
  SetPinyinLayoutPrefs(testing_profile, "Colemak");

  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  engine.Enable(kEngineIdPinyin);
  engine.FlushForTesting();  // ensure input_method is connected.
  engine.Focus(TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));

  EXPECT_EQ(InputMethodManager::Get()
                ->GetImeKeyboard()
                ->GetCurrentKeyboardLayoutName(),
            "us(colemak)");

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest,
       FocusCallsPassPredictiveWritingPrefWhenEnabled) {
  TestingProfile testing_profile;
  SetEmptyPrefs(testing_profile);
  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/true,
                        /*predictive_writing_enabled=*/true);
  EnableDefaultFeatureListWithMultiWord();

  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine(std::make_unique<FakeSuggesterSwitch>(
      FakeSuggesterSwitch::EnabledSuggestions{.multi_word_suggestions = true}));
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_method,
                OnFocus(MojoEq(ime::mojom::InputFieldInfo(
                            ime::mojom::InputFieldType::kText,
                            ime::mojom::AutocorrectMode::kEnabled,
                            ime::mojom::PersonalizationMode::kDisabled,
                            ime::mojom::TextPredictionMode::kEnabled)),
                        _, _))
        .WillOnce(
            ::testing::Invoke([](ime::mojom::InputFieldInfoPtr info,
                                 ime::mojom::InputMethodSettingsPtr settings,
                                 OnFocusCallback callback) {
              EXPECT_EQ(*settings,
                        *ime::mojom::InputMethodSettings::NewLatinSettings(
                            ime::mojom::LatinSettings::New(
                                /*autocorrect=*/true,
                                /*predictive_writing=*/true)));
              std::move(callback).Run(true, EmptyInputMethodMetadata());
            }));
    EXPECT_CALL(mock_input_method, OnSurroundingTextChanged(_, _, _));
  }

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();  // ensure input_method is connected.
  engine.Focus(TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

TEST_F(
    NativeInputMethodEngineTest,
    FocusCallsPassPredictiveWritingDisabledWhenMultiDisabledInBrowserContext) {
  TestingProfile testing_profile;
  SetEmptyPrefs(testing_profile);
  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/true,
                        /*predictive_writing_enabled=*/true);
  EnableDefaultFeatureListWithMultiWord();
  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine(std::make_unique<FakeSuggesterSwitch>(
      FakeSuggesterSwitch::EnabledSuggestions{.multi_word_suggestions =
                                                  false}));
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_method,
                OnFocus(MojoEq(ime::mojom::InputFieldInfo(
                            ime::mojom::InputFieldType::kText,
                            ime::mojom::AutocorrectMode::kEnabled,
                            ime::mojom::PersonalizationMode::kDisabled,
                            ime::mojom::TextPredictionMode::kDisabled)),
                        _, _))
        .WillOnce(
            ::testing::Invoke([](ime::mojom::InputFieldInfoPtr info,
                                 ime::mojom::InputMethodSettingsPtr settings,
                                 OnFocusCallback callback) {
              EXPECT_EQ(*settings,
                        *ime::mojom::InputMethodSettings::NewLatinSettings(
                            ime::mojom::LatinSettings::New(
                                /*autocorrect=*/true,
                                /*predictive_writing=*/true)));
              std::move(callback).Run(true, EmptyInputMethodMetadata());
            }));
    EXPECT_CALL(mock_input_method, OnSurroundingTextChanged(_, _, _));
  }

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();  // ensure input_method is connected.
  engine.Focus(TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

struct InputMethodMetadataCase {
  std::string test_name;
  AutocorrectSuggestionProvider provider;
  bool autocorrect_enabled;
};

class AutocorrectByDefaultDisabledByInputMethodMetadata
    : public NativeInputMethodEngineTest,
      public testing::WithParamInterface<InputMethodMetadataCase> {};

INSTANTIATE_TEST_SUITE_P(
    NativeInputMethodEngineTest,
    AutocorrectByDefaultDisabledByInputMethodMetadata,
    testing::ValuesIn<InputMethodMetadataCase>({
        InputMethodMetadataCase{
            "Unknown",
            /*provider=*/AutocorrectSuggestionProvider::kUnknown,
            /*autocorrect_enabled=*/false},
        InputMethodMetadataCase{
            "PrebundledUsEnglish",
            /*provider=*/AutocorrectSuggestionProvider::kUsEnglishPrebundled,
            /*autocorrect_enabled=*/false},
        InputMethodMetadataCase{
            "DownloadedUsEnglish",
            /*provider=*/AutocorrectSuggestionProvider::kUsEnglishDownloaded,
            /*autocorrect_enabled=*/false},
        InputMethodMetadataCase{
            "DownloadedUs840",
            /*provider=*/AutocorrectSuggestionProvider::kUsEnglish840,
            /*autocorrect_enabled=*/true},
        InputMethodMetadataCase{
            "DownloadedUs840V2",
            /*provider=*/AutocorrectSuggestionProvider::kUsEnglish840V2,
            /*autocorrect_enabled=*/true},
    }),
    [](const testing::TestParamInfo<InputMethodMetadataCase> info) {
      return info.param.test_name;
    });

TEST_P(AutocorrectByDefaultDisabledByInputMethodMetadata,
       DisablesAutocorrectInInputFieldSettingsWhenInvalidModelActivated) {
  const InputMethodMetadataCase& test_case = GetParam();
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      {features::kAutocorrectByDefault, features::kImeFstDecoderParamsUpdate},
      {features::kImeRuleConfig, features::kAssistMultiWord});
  TestingProfile testing_profile;
  SetPhysicalKeyboardAutocorrectAsEnabledByDefault(testing_profile.GetPrefs(),
                                                   kEngineIdUs);
  MockIMEInputContextHandler mock_handler;
  IMEBridge::Get()->SetInputContextHandler(&mock_handler);
  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_method,
                OnFocus(MojoEq(ime::mojom::InputFieldInfo(
                            ime::mojom::InputFieldType::kText,
                            ime::mojom::AutocorrectMode::kEnabled,
                            ime::mojom::PersonalizationMode::kDisabled,
                            ime::mojom::TextPredictionMode::kDisabled)),
                        _, _))
        .WillOnce(
            ::testing::Invoke([&](ime::mojom::InputFieldInfoPtr info,
                                  ime::mojom::InputMethodSettingsPtr settings,
                                  OnFocusCallback callback) {
              // Because we are retrieving the model details from the OnFocus
              // callback, when we first make a call to OnFocus the model
              // details wont be available. Thus autocorrect should be disabled
              // for the first OnFocus call.
              EXPECT_EQ(*settings,
                        *ime::mojom::InputMethodSettings::NewLatinSettings(
                            ime::mojom::LatinSettings::New(
                                /*autocorrect=*/false,
                                /*predictive_writing=*/false)));
              std::move(callback).Run(
                  true,
                  ime::mojom::InputMethodMetadata::New(
                      /*autocorrect_suggestion_provider=*/test_case.provider));
            }));
    EXPECT_CALL(mock_input_method, OnSurroundingTextChanged(_, _, _));
    EXPECT_CALL(mock_input_method, OnBlur());

    EXPECT_CALL(mock_input_method,
                OnFocus(MojoEq(ime::mojom::InputFieldInfo(
                            ime::mojom::InputFieldType::kText,
                            ime::mojom::AutocorrectMode::kEnabled,
                            ime::mojom::PersonalizationMode::kDisabled,
                            ime::mojom::TextPredictionMode::kDisabled)),
                        _, _))
        .WillOnce(
            ::testing::Invoke([&](ime::mojom::InputFieldInfoPtr info,
                                  ime::mojom::InputMethodSettingsPtr settings,
                                  OnFocusCallback callback) {
              // Now that we have received the model details from the first
              // OnFocus callback, we can validate the expected autocorrect
              // enabled/disabled state sent with the OnFocus call.
              EXPECT_EQ(*settings,
                        *ime::mojom::InputMethodSettings::NewLatinSettings(
                            ime::mojom::LatinSettings::New(
                                /*autocorrect=*/test_case.autocorrect_enabled,
                                /*predictive_writing=*/false)));
              std::move(callback).Run(
                  true,
                  ime::mojom::InputMethodMetadata::New(
                      /*autocorrect_suggestion_provider=*/test_case.provider));
            }));
    EXPECT_CALL(mock_input_method, OnSurroundingTextChanged(_, _, _));
    EXPECT_CALL(mock_input_method, OnBlur());
  }

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();  // ensure input_method is connected.
  engine.Focus(TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));
  engine.FlushForTesting();
  engine.Blur();
  engine.FlushForTesting();
  engine.Focus(TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));
  engine.FlushForTesting();
  engine.Blur();
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, HandleAutocorrectChangesAutocorrectRange) {
  TestingProfile testing_profile;
  SetEmptyPrefs(testing_profile);
  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/true,
                        /*predictive_writing_enabled=*/false);

  testing::NiceMock<MockInputMethod> mock_input_method;
  EXPECT_CALL(mock_input_method, OnFocus(_, _, _))
      .WillOnce(
          ::testing::Invoke([](ime::mojom::InputFieldInfoPtr info,
                               ime::mojom::InputMethodSettingsPtr settings,
                               OnFocusCallback callback) {
            std::move(callback).Run(true, EmptyInputMethodMetadata());
          }));

  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);
  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();  // ensure input_method is connected.
  engine.Focus(TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));
  engine.FlushForTesting();
  MockIMEInputContextHandler mock_handler;
  IMEBridge::Get()->SetInputContextHandler(&mock_handler);

  mock_input_method.host->HandleAutocorrect(
      ime::mojom::AutocorrectSpan::New(gfx::Range(0, 3), u"teh", u"the"));
  mock_input_method.host.FlushForTesting();

  EXPECT_EQ(mock_handler.GetAutocorrectRange(), gfx::Range(0, 3));

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest,
       SurroundingTextChangeConvertsToUtf8Correctly) {
  TestingProfile testing_profile;
  SetEmptyPrefs(testing_profile);
  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/true,
                        /*predictive_writing_enabled=*/false);

  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  MockIMEInputContextHandler mock_handler;
  IMEBridge::Get()->SetInputContextHandler(&mock_handler);
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_method, OnFocus(_, _, _))
        .WillOnce(
            ::testing::Invoke([](ime::mojom::InputFieldInfoPtr info,
                                 ime::mojom::InputMethodSettingsPtr settings,
                                 OnFocusCallback callback) {
              std::move(callback).Run(true, EmptyInputMethodMetadata());
            }));
    EXPECT_CALL(mock_input_method, OnSurroundingTextChanged("", _, _));

    // Each character in "你好" is three UTF-8 code units.
    EXPECT_CALL(mock_input_method,
                OnSurroundingTextChanged("你好",
                                         /*offset=*/0,
                                         MojoEq(ime::mojom::SelectionRange(
                                             /*anchor=*/6, /*focus=*/6))));
  }

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();  // ensure input_method is connected.
  engine.Focus(TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));
  // Each character in "你好" is one UTF-16 code unit.
  engine.SetSurroundingText(u"你好", gfx::Range(2),
                            /*offset=*/0);
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, ProcessesDeadKeysCorrectly) {
  TestingProfile testing_profile;
  SetEmptyPrefs(testing_profile);
  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/true,
                        /*predictive_writing_enabled=*/false);

  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  MockIMEInputContextHandler mock_handler;
  IMEBridge::Get()->SetInputContextHandler(&mock_handler);
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_method, OnFocus(_, _, _))
        .WillOnce(
            ::testing::Invoke([](ime::mojom::InputFieldInfoPtr info,
                                 ime::mojom::InputMethodSettingsPtr settings,
                                 OnFocusCallback callback) {
              std::move(callback).Run(true, EmptyInputMethodMetadata());
            }));

    EXPECT_CALL(mock_input_method, OnSurroundingTextChanged(_, _, _));

    // TODO(crbug.com/40173140): Expect the actual arguments to the call
    // once the Mojo API is replaced with protos. GMock does not play well with
    // move-only types like PhysicalKeyEvent.
    EXPECT_CALL(mock_input_method, ProcessKeyEvent(_, _))
        .Times(2)
        .WillRepeatedly(::testing::Invoke(
            [](ime::mojom::PhysicalKeyEventPtr,
               ime::mojom::InputMethod::ProcessKeyEventCallback callback) {
              std::move(callback).Run(
                  ime::mojom::KeyEventResult::kNeedsHandlingBySystem);
            }));
  }

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();  // ensure input_method is connected.
  engine.Focus(TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));

  // Quote ("VKEY_OEM_7") + A is a dead key combination.
  engine.ProcessKeyEvent(
      {ui::EventType::kKeyPressed, ui::VKEY_OEM_7, ui::DomCode::QUOTE,
       ui::EF_NONE, ui::DomKey::DeadKeyFromCombiningCharacter(u'\u0301'),
       base::TimeTicks()},
      base::DoNothing());
  engine.ProcessKeyEvent(
      {ui::EventType::kKeyReleased, ui::VKEY_OEM_7, ui::DomCode::QUOTE,
       ui::EF_NONE, ui::DomKey::DeadKeyFromCombiningCharacter(u'\u0301'),
       base::TimeTicks()},
      base::DoNothing());
  engine.ProcessKeyEvent({ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE},
                         base::DoNothing());
  engine.ProcessKeyEvent({ui::EventType::kKeyReleased, ui::VKEY_A, ui::EF_NONE},
                         base::DoNothing());
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, ProcessesNamedKeysCorrectly) {
  TestingProfile testing_profile;
  SetEmptyPrefs(testing_profile);
  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/true,
                        /*predictive_writing_enabled=*/false);

  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  MockIMEInputContextHandler mock_handler;
  IMEBridge::Get()->SetInputContextHandler(&mock_handler);
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_method, OnFocus(_, _, _))
        .WillOnce(
            ::testing::Invoke([](ime::mojom::InputFieldInfoPtr info,
                                 ime::mojom::InputMethodSettingsPtr settings,
                                 OnFocusCallback callback) {
              std::move(callback).Run(true, EmptyInputMethodMetadata());
            }));

    EXPECT_CALL(mock_input_method, OnSurroundingTextChanged(_, _, _));

    // TODO(crbug.com/40173140): Expect the actual arguments to the call
    // once the Mojo API is replaced with protos. GMock does not play well with
    // move-only types like PhysicalKeyEvent.
    EXPECT_CALL(mock_input_method, ProcessKeyEvent(_, _))
        .Times(4)
        .WillRepeatedly(::testing::Invoke(
            [](ime::mojom::PhysicalKeyEventPtr event,
               ime::mojom::InputMethod::ProcessKeyEventCallback callback) {
              EXPECT_TRUE(event->key->is_named_key());
              std::move(callback).Run(
                  ime::mojom::KeyEventResult::kNeedsHandlingBySystem);
            }));
  }

  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();  // ensure input_method is connected.
  engine.Focus(TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));

  // Enter and Backspace are named keys with Unicode representation.
  engine.ProcessKeyEvent(
      {ui::EventType::kKeyPressed, ui::VKEY_RETURN, ui::DomCode::ENTER,
       ui::EF_NONE, ui::DomKey::ENTER, base::TimeTicks()},
      base::DoNothing());
  engine.ProcessKeyEvent(
      {ui::EventType::kKeyReleased, ui::VKEY_RETURN, ui::EF_NONE},
      base::DoNothing());
  engine.ProcessKeyEvent(
      {ui::EventType::kKeyPressed, ui::VKEY_BACK, ui::DomCode::BACKSPACE,
       ui::EF_NONE, ui::DomKey::BACKSPACE, base::TimeTicks()},
      base::DoNothing());
  engine.ProcessKeyEvent(
      {ui::EventType::kKeyReleased, ui::VKEY_BACK, ui::EF_NONE},
      base::DoNothing());
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineTest, DoesNotSendUnhandledNamedKeys) {
  TestingProfile testing_profile;
  SetEmptyPrefs(testing_profile);
  SetInputMethodOptions(testing_profile, /*autocorrect_enabled=*/true,
                        /*predictive_writing_enabled=*/false);

  testing::StrictMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  MockIMEInputContextHandler mock_handler;
  IMEBridge::Get()->SetInputContextHandler(&mock_handler);
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", &testing_profile);

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_input_method, OnFocus(_, _, _))
        .WillOnce(
            ::testing::Invoke([](ime::mojom::InputFieldInfoPtr info,
                                 ime::mojom::InputMethodSettingsPtr settings,
                                 OnFocusCallback callback) {
              std::move(callback).Run(true, EmptyInputMethodMetadata());
            }));
    EXPECT_CALL(mock_input_method, OnSurroundingTextChanged(_, _, _));
    EXPECT_CALL(mock_input_method, ProcessKeyEvent(_, _)).Times(0);
  }

  engine.Enable(kEngineIdUs);
  engine.Focus(TextInputMethod::InputContext(ui::TEXT_INPUT_TYPE_TEXT));

  // Help is a named DOM key, but is not used by IMEs.
  engine.ProcessKeyEvent(
      {ui::EventType::kKeyPressed, ui::VKEY_HELP, ui::DomCode::HELP,
       ui::EF_NONE, ui::DomKey::HELP, base::TimeTicks()},
      base::DoNothing());
  engine.ProcessKeyEvent(
      {ui::EventType::kKeyReleased, ui::VKEY_HELP, ui::EF_NONE},
      base::DoNothing());
  engine.FlushForTesting();

  InputMethodManager::Shutdown();
}

class NativeInputMethodEngineWithRenderViewHostTest
    : public content::RenderViewHostTestHarness {
  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());

    // Needed by NativeInputMethodEngine for the virtual keyboard.
    keyboard_controller_client_test_helper_ =
        ChromeKeyboardControllerClientTestHelper::InitializeWithFake();

    chromeos::machine_learning::ServiceConnection::
        UseFakeServiceConnectionForTesting(&fake_service_connection_);
    chromeos::machine_learning::ServiceConnection::GetInstance()->Initialize();
  }

  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
      keyboard_controller_client_test_helper_;
  chromeos::machine_learning::FakeServiceConnectionImpl
      fake_service_connection_;
};

TEST_F(NativeInputMethodEngineWithRenderViewHostTest,
       RecordUkmAddsNonCompliantApiUkmEntry) {
  GURL url("https://www.example.com/");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  auto* testing_profile = static_cast<TestingProfile*>(browser_context());
  SetInputMethodOptions(*testing_profile, /*autocorrect_enabled=*/true,
                        /*predictive_writing_enabled=*/false);

  testing::NiceMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", testing_profile);
  TextInputMethod::InputContext input_context(ui::TEXT_INPUT_TYPE_TEXT);
  engine.Enable(kEngineIdUs);
  engine.FlushForTesting();

  ui::FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  fake_text_input_client.set_source_id(main_rfh()->GetPageUkmSourceId());

  InputMethodAsh ime(nullptr);
  ime.SetFocusedTextInputClient(&fake_text_input_client);
  IMEBridge::Get()->SetInputContextHandler(&ime);

  ukm::TestAutoSetUkmRecorder test_recorder;
  test_recorder.UpdateRecording({ukm::UkmConsentType::MSBB});
  ASSERT_EQ(0u, test_recorder.entries_count());

  auto metric = ime::mojom::NonCompliantApiMetric::New();
  metric->non_compliant_operation =
      ime::mojom::InputMethodApiOperation::kSetCompositionText;
  auto entry = ime::mojom::UkmEntry::NewNonCompliantApi(std::move(metric));
  mock_input_method.host->RecordUkm(std::move(entry));
  mock_input_method.host.FlushForTesting();

  EXPECT_EQ(0u, test_recorder.sources_count());
  EXPECT_EQ(1u, test_recorder.entries_count());
  const auto entries =
      test_recorder.GetEntriesByName("InputMethod.NonCompliantApi");
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0], "NonCompliantOperation", 1);  // kSetCompositionText

  InputMethodManager::Shutdown();
}

TEST_F(NativeInputMethodEngineWithRenderViewHostTest,
       RecordUkmAddsAssistiveMatchUkmEntry) {
  GURL url("https://www.example.com/");
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             url);

  auto* testing_profile = static_cast<TestingProfile*>(browser_context());
  testing::NiceMock<MockInputMethod> mock_input_method;
  InputMethodManager::Initialize(
      new TestInputMethodManager(&mock_input_method));
  NativeInputMethodEngine engine;
  engine.Initialize(std::make_unique<StubInputMethodEngineObserver>(),
                    /*extension_id=*/"", testing_profile);
  engine.get_assistive_suggester_for_testing()
      ->get_emoji_suggester_for_testing()
      ->LoadEmojiMapForTesting("happy,😀;😃;😄");

  ui::FakeTextInputClient fake_text_input_client(ui::TEXT_INPUT_TYPE_TEXT);
  fake_text_input_client.set_source_id(main_rfh()->GetPageUkmSourceId());

  InputMethodAsh ime(nullptr);
  ime.SetFocusedTextInputClient(&fake_text_input_client);
  IMEBridge::Get()->SetInputContextHandler(&ime);

  ukm::TestAutoSetUkmRecorder test_recorder;
  test_recorder.UpdateRecording({ukm::UkmConsentType::MSBB});
  ASSERT_EQ(0u, test_recorder.entries_count());

  // Should not record when random text is entered.
  engine.SetSurroundingText(u"random text ", gfx::Range(12), 0);
  EXPECT_EQ(0u, test_recorder.entries_count());

  // Should record when match is triggered.
  engine.SetSurroundingText(u"happy ", gfx::Range(6), 0);
  EXPECT_EQ(0u, test_recorder.sources_count());
  EXPECT_EQ(1u, test_recorder.entries_count());
  const auto entries =
      test_recorder.GetEntriesByName("InputMethod.Assistive.Match");
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(entries[0], "Type",
                                                 (int)AssistiveType::kEmoji);

  InputMethodManager::Shutdown();
}

}  // namespace
}  // namespace input_method
}  // namespace ash
