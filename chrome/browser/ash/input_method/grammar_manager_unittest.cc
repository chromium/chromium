// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/grammar_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/input_method/assistive_window_properties.h"
#include "chrome/browser/ash/input_method/grammar_service_client.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/ash/mock_ime_input_context_handler.h"
#include "ui/base/ime/text_input_flags.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {
namespace input_method {
namespace {

using ::testing::_;

const char16_t kShowGrammarSuggestionMessage[] =
    u"Grammar correction suggested. Press tab to access; escape to dismiss.";
const char16_t kDismissGrammarSuggestionMessage[] = u"Suggestion dismissed.";
const char16_t kAcceptGrammarSuggestionMessage[] = u"Suggestion accepted.";
const char16_t kIgnoreGrammarSuggestionMessage[] = u"Suggestion ignored.";
const char16_t kSuggestionButtonMessage[] =
    u"Suggestion correct. Button. Press enter to accept; escape to dismiss.";
const char16_t kIgnoreButtonMessage[] =
    u"Ignore suggestion. Button. Press enter to ignore the suggestion; escape "
    u"to dismiss.";

class TestGrammarServiceClient : public GrammarServiceClient {
 public:
  TestGrammarServiceClient() {}
  ~TestGrammarServiceClient() override = default;

  bool RequestTextCheck(Profile* profile,
                        const std::u16string& text,
                        TextCheckCompleteCallback callback) override {
    std::vector<ui::GrammarFragment> grammar_results;
    for (size_t i = 0; i < text.size(); i++) {
      if (text.substr(i, 5) == u"error") {
        grammar_results.emplace_back(gfx::Range(i, i + 5), "correct");
      }
    }
    std::move(callback).Run(true, grammar_results);
    return true;
  }
};

ui::KeyEvent CreateKeyEvent(const ui::DomCode& code) {
  return ui::KeyEvent(ui::EventType::kKeyPressed, ui::VKEY_UNKNOWN, code,
                      ui::EF_NONE, ui::DomKey::NONE, ui::EventTimeForNow());
}

class MockSuggestionHandler : public SuggestionHandlerInterface {
 public:
  MOCK_METHOD(bool,
              DismissSuggestion,
              (int context_id, std::string* error),
              (override));
  MOCK_METHOD(bool,
              SetSuggestion,
              (int context_id,
               const ui::ime::SuggestionDetails& details,
               std::string* error),
              (override));
  MOCK_METHOD(bool,
              AcceptSuggestion,
              (int context_id, std::string* error),
              (override));
  MOCK_METHOD(void,
              OnSuggestionsChanged,
              (const std::vector<std::string>& suggestions),
              (override));
  MOCK_METHOD(bool,
              SetButtonHighlighted,
              (int context_id,
               const ui::ime::AssistiveWindowButton& button,
               bool highlighted,
               std::string* error),
              (override));
  MOCK_METHOD(void,
              ClickButton,
              (const ui::ime::AssistiveWindowButton& button),
              (override));
  MOCK_METHOD(bool,
              AcceptSuggestionCandidate,
              (int context_id,
               const std::u16string& candidate,
               size_t delete_previous_utf16_len,
               bool use_replace_surrounding_text,
               std::string* error),
              (override));
  MOCK_METHOD(bool,
              SetAssistiveWindowProperties,
              (int context_id,
               const AssistiveWindowProperties& assistive_window,
               std::string* error),
              (override));
  MOCK_METHOD(void, Announce, (const std::u16string& message), (override));
};

class GrammarManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    IMEBridge::Get()->SetInputContextHandler(&mock_ime_input_context_handler_);
    chromeos::machine_learning::ServiceConnection::
        UseFakeServiceConnectionForTesting(&fake_service_connection_);
    chromeos::machine_learning::ServiceConnection::GetInstance()->Initialize();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<TestingProfile> profile_;
  chromeos::machine_learning::FakeServiceConnectionImpl
      fake_service_connection_;
  MockIMEInputContextHandler mock_ime_input_context_handler_;
};

TEST_F(GrammarManagerTest, HandlesSingleGrammarCheckResult) {
  MockSuggestionHandler mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);
  base::HistogramTester histogram_tester;

  manager.OnFocus(1, SpellcheckMode::kUnspecified);
  manager.OnSurroundingTextChanged(u"", gfx::Range(0));
  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(0));
  task_environment_.FastForwardBy(base::Milliseconds(2500));

  auto grammar_fragments =
      mock_ime_input_context_handler_.get_grammar_fragments();
  EXPECT_EQ(grammar_fragments.size(), 1u);
  EXPECT_EQ(grammar_fragments[0].range, gfx::Range(9, 14));
  EXPECT_EQ(grammar_fragments[0].suggestion, "correct");
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Grammar.Actions",
                                      0 /*GrammarAction::kUnderlined*/, 1);
}

TEST_F(GrammarManagerTest, RecordsUnderlinesMetricsWithoutDups) {
  MockSuggestionHandler mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);
  base::HistogramTester histogram_tester;

  manager.OnFocus(1, SpellcheckMode::kUnspecified);
  manager.OnSurroundingTextChanged(u"", gfx::Range(0));
  manager.OnSurroundingTextChanged(u"There is error error", gfx::Range(0));
  task_environment_.FastForwardBy(base::Milliseconds(2500));
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Grammar.Actions",
                                      0 /*GrammarAction::kUnderlined*/, 2);

  manager.OnSurroundingTextChanged(u"There is error error error",
                                   gfx::Range(0));
  task_environment_.FastForwardBy(base::Milliseconds(2500));
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Grammar.Actions",
                                      0 /*GrammarAction::kUnderlined*/, 3);
}

TEST_F(GrammarManagerTest, DoesNotRunGrammarCheckOnTextFieldWithSpellcheckOff) {
  MockSuggestionHandler mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);
  base::HistogramTester histogram_tester;

  manager.OnFocus(1, SpellcheckMode::kDisabled);
  manager.OnSurroundingTextChanged(u"", gfx::Range(0));
  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(0));
  task_environment_.FastForwardBy(base::Milliseconds(2500));

  auto grammar_fragments =
      mock_ime_input_context_handler_.get_grammar_fragments();
  EXPECT_EQ(grammar_fragments.size(), 0u);
  histogram_tester.ExpectUniqueSample("InputMethod.Assistive.Grammar.Actions",
                                      0 /*GrammarAction::kUnderlined*/, 0);
}

TEST_F(GrammarManagerTest, ChecksLastSentenceImmediately) {
  MockSuggestionHandler mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"", gfx::Range(0));
  manager.OnSurroundingTextChanged(u"There is error. And another error.",
                                   gfx::Range(20));
  task_environment_.FastForwardBy(base::Milliseconds(250));

  auto grammar_fragments =
      mock_ime_input_context_handler_.get_grammar_fragments();
  EXPECT_EQ(grammar_fragments.size(), 1u);
  EXPECT_EQ(grammar_fragments[0].range, gfx::Range(9, 14));
  EXPECT_EQ(grammar_fragments[0].suggestion, "correct");
}

TEST_F(GrammarManagerTest, ChecksBothLastAndCurrentSentence) {
  MockSuggestionHandler mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"", gfx::Range(0));
  manager.OnSurroundingTextChanged(u"There is error. And another error.",
                                   gfx::Range(20));
  task_environment_.FastForwardBy(base::Milliseconds(2500));

  auto grammar_fragments =
      mock_ime_input_context_handler_.get_grammar_fragments();
  EXPECT_EQ(grammar_fragments.size(), 2u);
  EXPECT_EQ(grammar_fragments[0].range, gfx::Range(9, 14));
  EXPECT_EQ(grammar_fragments[0].suggestion, "correct");
  EXPECT_EQ(grammar_fragments[1].range, gfx::Range(28, 33));
  EXPECT_EQ(grammar_fragments[1].suggestion, "correct");
}

TEST_F(GrammarManagerTest, HandlesMultipleGrammarCheckResults) {
  MockSuggestionHandler mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"", gfx::Range(0));
  manager.OnSurroundingTextChanged(u"There is error error.", gfx::Range(0));
  task_environment_.FastForwardBy(base::Milliseconds(2500));

  auto grammar_fragments =
      mock_ime_input_context_handler_.get_grammar_fragments();
  EXPECT_EQ(grammar_fragments.size(), 2u);
  EXPECT_EQ(grammar_fragments[0].range, gfx::Range(9, 14));
  EXPECT_EQ(grammar_fragments[0].suggestion, "correct");
  EXPECT_EQ(grammar_fragments[1].range, gfx::Range(15, 20));
  EXPECT_EQ(grammar_fragments[0].suggestion, "correct");
}

TEST_F(GrammarManagerTest, ClearsPreviousMarkersUponGettingNewResults) {
  MockSuggestionHandler mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"", gfx::Range(0));
  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(0));
  task_environment_.FastForwardBy(base::Milliseconds(2500));

  auto grammar_fragments =
      mock_ime_input_context_handler_.get_grammar_fragments();
  EXPECT_EQ(grammar_fragments.size(), 1u);
  EXPECT_EQ(grammar_fragments[0].range, gfx::Range(9, 14));
  EXPECT_EQ(grammar_fragments[0].suggestion, "correct");

  manager.OnSurroundingTextChanged(u"There is a new error.", gfx::Range(0));
  task_environment_.FastForwardBy(base::Milliseconds(2500));

  auto updated_grammar_fragments =
      mock_ime_input_context_handler_.get_grammar_fragments();
  EXPECT_EQ(updated_grammar_fragments.size(), 1u);
  EXPECT_EQ(updated_grammar_fragments[0].range, gfx::Range(15, 20));
  EXPECT_EQ(updated_grammar_fragments[0].suggestion, "correct");
}

TEST_F(GrammarManagerTest, ShowsAndDismissesGrammarSuggestion) {
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);
  base::HistogramTester histogram_tester;

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"", gfx::Range(0));
  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(0));
  task_environment_.FastForwardBy(base::Milliseconds(2500));

  AssistiveWindowProperties expected_properties;
  expected_properties.type = ash::ime::AssistiveWindowType::kGrammarSuggestion;
  expected_properties.candidates = {u"correct"};
  expected_properties.visible = true;
  expected_properties.announce_string = kShowGrammarSuggestionMessage;

  EXPECT_CALL(mock_suggestion_handler,
              SetAssistiveWindowProperties(1, expected_properties, _));

  mock_ime_input_context_handler_.set_cursor_range(gfx::Range(10, 10));
  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(10));
  histogram_tester.ExpectBucketCount("InputMethod.Assistive.Grammar.Actions",
                                     1 /*GrammarAction::kWindowShown*/, 1);

  EXPECT_CALL(mock_suggestion_handler, DismissSuggestion(1, _));
  EXPECT_CALL(mock_suggestion_handler,
              Announce(std::u16string(kDismissGrammarSuggestionMessage)));

  manager.OnKeyEvent(CreateKeyEvent(ui::DomCode::ESCAPE));
}

TEST_F(GrammarManagerTest, DoesntShowGrammarSuggestionWhenUndoWindowIsShown) {
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);
  base::HistogramTester histogram_tester;

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"", gfx::Range(0));
  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(0));
  mock_ime_input_context_handler_.SetAutocorrectRange(gfx::Range(9, 14),
                                                      base::DoNothing());
  task_environment_.FastForwardBy(base::Milliseconds(2500));

  auto grammar_fragments =
      mock_ime_input_context_handler_.get_grammar_fragments();
  EXPECT_EQ(grammar_fragments.size(), 1u);
  EXPECT_EQ(grammar_fragments[0].range, gfx::Range(9, 14));
  EXPECT_EQ(grammar_fragments[0].suggestion, "correct");

  // No EXPECT_CALL comparing with the last test case because suggestion window
  // should not show.

  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(10));
  histogram_tester.ExpectBucketCount("InputMethod.Assistive.Grammar.Actions",
                                     1 /*GrammarAction::kWindowShown*/, 0);
}

TEST_F(GrammarManagerTest, DismissesSuggestionWhenSelectingARange) {
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"", gfx::Range(0));
  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(0));
  task_environment_.FastForwardBy(base::Milliseconds(2500));

  AssistiveWindowProperties expected_properties;
  expected_properties.type = ash::ime::AssistiveWindowType::kGrammarSuggestion;
  expected_properties.candidates = {u"correct"};
  expected_properties.visible = true;
  expected_properties.announce_string = kShowGrammarSuggestionMessage;

  EXPECT_CALL(mock_suggestion_handler,
              SetAssistiveWindowProperties(1, expected_properties, _));

  mock_ime_input_context_handler_.set_cursor_range(gfx::Range(10, 10));
  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(10));

  EXPECT_CALL(mock_suggestion_handler, DismissSuggestion(1, _));

  mock_ime_input_context_handler_.set_cursor_range(gfx::Range(9, 10));
  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(9, 10));
}

TEST_F(GrammarManagerTest, HighlightsAndCommitsGrammarSuggestionWithTab) {
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);
  base::HistogramTester histogram_tester;

  mock_ime_input_context_handler_.Reset();

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"", gfx::Range(0));
  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(0));
  task_environment_.FastForwardBy(base::Milliseconds(2500));

  EXPECT_CALL(mock_suggestion_handler, SetAssistiveWindowProperties(1, _, _));
  mock_ime_input_context_handler_.set_cursor_range(gfx::Range(10, 10));
  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(10));

  ui::ime::AssistiveWindowButton suggestion_button{
      .id = ui::ime::ButtonId::kSuggestion,
      .window_type = ash::ime::AssistiveWindowType::kGrammarSuggestion,
      .announce_string = kSuggestionButtonMessage,
  };
  EXPECT_CALL(mock_suggestion_handler,
              SetButtonHighlighted(1, suggestion_button, true, _));
  manager.OnKeyEvent(CreateKeyEvent(ui::DomCode::TAB));
  EXPECT_CALL(mock_suggestion_handler, DismissSuggestion(1, _));
  EXPECT_CALL(mock_suggestion_handler,
              Announce(std::u16string(kAcceptGrammarSuggestionMessage)));
  manager.OnKeyEvent(CreateKeyEvent(ui::DomCode::ENTER));
  task_environment_.FastForwardBy(base::Milliseconds(200));

  EXPECT_EQ(
      mock_ime_input_context_handler_.delete_surrounding_text_call_count(), 1);
  auto deleteSurroundingTextArg =
      mock_ime_input_context_handler_.last_delete_surrounding_text_arg();
  EXPECT_EQ(deleteSurroundingTextArg.num_char16s_before_cursor, 1u);
  EXPECT_EQ(deleteSurroundingTextArg.num_char16s_after_cursor, 4u);

  EXPECT_EQ(mock_ime_input_context_handler_.commit_text_call_count(), 1);
  EXPECT_EQ(mock_ime_input_context_handler_.last_commit_text(), u"correct");
  histogram_tester.ExpectBucketCount("InputMethod.Assistive.Grammar.Actions",
                                     2 /*GrammarAction::kAccepted*/, 1);
}

TEST_F(GrammarManagerTest, HighlightsAndCommitsGrammarSuggestionWithUpArrow) {
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);
  base::HistogramTester histogram_tester;

  mock_ime_input_context_handler_.Reset();

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"", gfx::Range(0));
  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(0));
  task_environment_.FastForwardBy(base::Milliseconds(2500));

  EXPECT_CALL(mock_suggestion_handler, SetAssistiveWindowProperties(1, _, _));
  mock_ime_input_context_handler_.set_cursor_range(gfx::Range(10, 10));
  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(10));

  ui::ime::AssistiveWindowButton suggestion_button{
      .id = ui::ime::ButtonId::kSuggestion,
      .window_type = ash::ime::AssistiveWindowType::kGrammarSuggestion,
      .announce_string = kSuggestionButtonMessage,
  };
  EXPECT_CALL(mock_suggestion_handler,
              SetButtonHighlighted(1, suggestion_button, true, _));
  manager.OnKeyEvent(CreateKeyEvent(ui::DomCode::ARROW_UP));
  EXPECT_CALL(mock_suggestion_handler, DismissSuggestion(1, _));
  EXPECT_CALL(mock_suggestion_handler,
              Announce(std::u16string(kAcceptGrammarSuggestionMessage)));
  manager.OnKeyEvent(CreateKeyEvent(ui::DomCode::ENTER));
  task_environment_.FastForwardBy(base::Milliseconds(200));

  EXPECT_EQ(
      mock_ime_input_context_handler_.delete_surrounding_text_call_count(), 1);
  auto deleteSurroundingTextArg =
      mock_ime_input_context_handler_.last_delete_surrounding_text_arg();
  EXPECT_EQ(deleteSurroundingTextArg.num_char16s_before_cursor, 1u);
  EXPECT_EQ(deleteSurroundingTextArg.num_char16s_after_cursor, 4u);

  EXPECT_EQ(mock_ime_input_context_handler_.commit_text_call_count(), 1);
  EXPECT_EQ(mock_ime_input_context_handler_.last_commit_text(), u"correct");
  histogram_tester.ExpectBucketCount("InputMethod.Assistive.Grammar.Actions",
                                     2 /*GrammarAction::kAccepted*/, 1);
}

TEST_F(GrammarManagerTest, IgnoresGrammarSuggestion) {
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);
  base::HistogramTester histogram_tester;

  mock_ime_input_context_handler_.Reset();

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"", gfx::Range(0));
  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(0));
  task_environment_.FastForwardBy(base::Milliseconds(2500));

  EXPECT_EQ(mock_ime_input_context_handler_.get_grammar_fragments().size(), 1u);
  EXPECT_CALL(mock_suggestion_handler, SetAssistiveWindowProperties(1, _, _));
  mock_ime_input_context_handler_.set_cursor_range(gfx::Range(10, 10));
  manager.OnSurroundingTextChanged(u"There is error.", gfx::Range(10));

  ui::ime::AssistiveWindowButton suggestion_button{
      .id = ui::ime::ButtonId::kSuggestion,
      .window_type = ash::ime::AssistiveWindowType::kGrammarSuggestion,
      .announce_string = kSuggestionButtonMessage,
  };
  ui::ime::AssistiveWindowButton ignore_button{
      .id = ui::ime::ButtonId::kIgnoreSuggestion,
      .window_type = ash::ime::AssistiveWindowType::kGrammarSuggestion,
      .announce_string = kIgnoreButtonMessage,
  };

  EXPECT_CALL(mock_suggestion_handler,
              SetButtonHighlighted(1, suggestion_button, true, _));
  manager.OnKeyEvent(CreateKeyEvent(ui::DomCode::TAB));
  EXPECT_CALL(mock_suggestion_handler,
              SetButtonHighlighted(1, ignore_button, true, _));
  manager.OnKeyEvent(CreateKeyEvent(ui::DomCode::TAB));
  EXPECT_CALL(mock_suggestion_handler, DismissSuggestion(1, _));
  EXPECT_CALL(mock_suggestion_handler,
              Announce(std::u16string(kIgnoreGrammarSuggestionMessage)));
  manager.OnKeyEvent(CreateKeyEvent(ui::DomCode::ENTER));

  EXPECT_EQ(mock_ime_input_context_handler_.get_grammar_fragments().size(), 0u);
  EXPECT_EQ(
      mock_ime_input_context_handler_.delete_surrounding_text_call_count(), 0);
  EXPECT_EQ(mock_ime_input_context_handler_.commit_text_call_count(), 0);
  histogram_tester.ExpectBucketCount("InputMethod.Assistive.Grammar.Actions",
                                     3 /*GrammarAction::kIgnored*/, 1);

  manager.OnSurroundingTextChanged(u"There is error. There is error.",
                                   gfx::Range(20));
  task_environment_.FastForwardBy(base::Milliseconds(2500));
  EXPECT_EQ(mock_ime_input_context_handler_.get_grammar_fragments().size(), 0u);
}

}  // namespace
}  // namespace input_method
}  // namespace ash
