// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/grammar_manager.h"

#include "chrome/browser/chromeos/input_method/grammar_service_client.h"
#include "chrome/browser/chromeos/input_method/ui/suggestion_details.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/mock_ime_input_context_handler.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace chromeos {
namespace {

using ::testing::_;

class TestGrammarServiceClient : public GrammarServiceClient {
 public:
  TestGrammarServiceClient() {}
  ~TestGrammarServiceClient() override = default;

  bool RequestTextCheck(Profile* profile,
                        const std::u16string& text,
                        TextCheckCompleteCallback callback) const override {
    std::vector<ui::GrammarFragment> grammar_results;
    for (int i = 0; i < text.size(); i++) {
      if (text.substr(i, 5) == u"error") {
        grammar_results.emplace_back(gfx::Range(i, i + 5), "correct");
      }
    }
    std::move(callback).Run(true, grammar_results);
    return true;
  }
};

ui::KeyEvent CreateKeyEvent(const ui::DomCode& code) {
  return ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN, code, ui::EF_NONE,
                      ui::DomKey::NONE, ui::EventTimeForNow());
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
               std::string* error),
              (override));
  MOCK_METHOD(bool,
              SetAssistiveWindowProperties,
              (int context_id,
               const AssistiveWindowProperties& assistive_window,
               std::string* error),
              (override));
};

class GrammarManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    ui::IMEBridge::Initialize();
    ui::IMEBridge::Get()->SetInputContextHandler(
        &mock_ime_input_context_handler_);
    machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
    machine_learning::ServiceConnection::GetInstance()->Initialize();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<TestingProfile> profile_;
  machine_learning::FakeServiceConnectionImpl fake_service_connection_;
  ui::MockIMEInputContextHandler mock_ime_input_context_handler_;
};

TEST_F(GrammarManagerTest, HandlesSingleGrammarCheckResult) {
  MockSuggestionHandler mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"There is error.", 0, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1000));

  auto grammar_fragments =
      mock_ime_input_context_handler_.get_grammar_fragments();
  EXPECT_EQ(grammar_fragments.size(), 1);
  EXPECT_EQ(grammar_fragments[0].range, gfx::Range(9, 14));
  EXPECT_EQ(grammar_fragments[0].suggestion, "correct");
}

TEST_F(GrammarManagerTest, HandlesMultipleGrammarCheckResults) {
  MockSuggestionHandler mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"There is error error.", 0, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1000));

  auto grammar_fragments =
      mock_ime_input_context_handler_.get_grammar_fragments();
  EXPECT_EQ(grammar_fragments.size(), 2);
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
  manager.OnSurroundingTextChanged(u"There is error.", 0, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1000));

  auto grammar_fragments =
      mock_ime_input_context_handler_.get_grammar_fragments();
  EXPECT_EQ(grammar_fragments.size(), 1);
  EXPECT_EQ(grammar_fragments[0].range, gfx::Range(9, 14));
  EXPECT_EQ(grammar_fragments[0].suggestion, "correct");

  manager.OnSurroundingTextChanged(u"There is a new error.", 0, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1000));

  auto updated_grammar_fragments =
      mock_ime_input_context_handler_.get_grammar_fragments();
  EXPECT_EQ(updated_grammar_fragments.size(), 1);
  EXPECT_EQ(updated_grammar_fragments[0].range, gfx::Range(15, 20));
  EXPECT_EQ(updated_grammar_fragments[0].suggestion, "correct");
}

TEST_F(GrammarManagerTest, ShowsAndDismissesGrammarSuggestion) {
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"There is error.", 0, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1000));

  ui::ime::SuggestionDetails expected_details;
  expected_details.text = u"correct";
  expected_details.confirmed_length = 0;
  expected_details.show_accept_annotation = false;
  expected_details.show_setting_link = false;

  EXPECT_CALL(mock_suggestion_handler, SetSuggestion(1, expected_details, _));

  manager.OnSurroundingTextChanged(u"There is error.", 10, 10);

  EXPECT_CALL(mock_suggestion_handler, DismissSuggestion(1, _));

  manager.OnSurroundingTextChanged(u"There is error.", 3, 3);
}

TEST_F(GrammarManagerTest, HighlightsAndCommitsGrammarSuggestion) {
  ::testing::StrictMock<MockSuggestionHandler> mock_suggestion_handler;
  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>(),
                         &mock_suggestion_handler);

  mock_ime_input_context_handler_.Reset();

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"There is error.", 0, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1000));

  EXPECT_CALL(mock_suggestion_handler, SetSuggestion(1, _, _));
  manager.OnSurroundingTextChanged(u"There is error.", 10, 10);
  EXPECT_CALL(mock_suggestion_handler, SetButtonHighlighted(1, _, true, _));
  manager.OnKeyEvent(CreateKeyEvent(ui::DomCode::TAB));
  EXPECT_CALL(mock_suggestion_handler, DismissSuggestion(1, _));
  manager.OnKeyEvent(CreateKeyEvent(ui::DomCode::ENTER));

  EXPECT_EQ(
      mock_ime_input_context_handler_.delete_surrounding_text_call_count(), 1);
  auto deleteSurroundingTextArg =
      mock_ime_input_context_handler_.last_delete_surrounding_text_arg();
  EXPECT_EQ(deleteSurroundingTextArg.offset, 9);
  EXPECT_EQ(deleteSurroundingTextArg.length, 5);

  EXPECT_EQ(mock_ime_input_context_handler_.commit_text_call_count(), 1);
  EXPECT_EQ(mock_ime_input_context_handler_.last_commit_text(), u"correct");
}

}  // namespace
}  // namespace chromeos
