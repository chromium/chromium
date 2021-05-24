// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/grammar_manager.h"

#include "chrome/browser/chromeos/input_method/grammar_service_client.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/ime_bridge.h"
#include "ui/base/ime/chromeos/mock_ime_input_context_handler.h"

namespace chromeos {
namespace {

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

class GrammarManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
    machine_learning::ServiceConnection::GetInstance()->Initialize();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<TestingProfile> profile_;
  machine_learning::FakeServiceConnectionImpl fake_service_connection_;
};

TEST_F(GrammarManagerTest, HandlesSingleGrammarCheckResult) {
  ui::IMEBridge::Initialize();
  ui::MockIMEInputContextHandler mock_ime_input_context_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_ime_input_context_handler);

  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>());

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"There is error.", 0, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1000));

  auto grammar_fragments =
      mock_ime_input_context_handler.get_grammar_fragments();
  EXPECT_EQ(grammar_fragments.size(), 1);
  EXPECT_EQ(grammar_fragments[0].range, gfx::Range(9, 14));
  EXPECT_EQ(grammar_fragments[0].suggestion, "correct");
}

TEST_F(GrammarManagerTest, HandlesMultipleGrammarCheckResults) {
  ui::IMEBridge::Initialize();
  ui::MockIMEInputContextHandler mock_ime_input_context_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_ime_input_context_handler);

  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>());

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"There is error error.", 0, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1000));

  auto grammar_fragments =
      mock_ime_input_context_handler.get_grammar_fragments();
  EXPECT_EQ(grammar_fragments.size(), 2);
  EXPECT_EQ(grammar_fragments[0].range, gfx::Range(9, 14));
  EXPECT_EQ(grammar_fragments[0].suggestion, "correct");
  EXPECT_EQ(grammar_fragments[1].range, gfx::Range(15, 20));
  EXPECT_EQ(grammar_fragments[0].suggestion, "correct");
}

TEST_F(GrammarManagerTest, ClearsPreviousMarkersUponGettingNewResults) {
  ui::IMEBridge::Initialize();
  ui::MockIMEInputContextHandler mock_ime_input_context_handler;
  ui::IMEBridge::Get()->SetInputContextHandler(&mock_ime_input_context_handler);

  GrammarManager manager(profile_.get(),
                         std::make_unique<TestGrammarServiceClient>());

  manager.OnFocus(1);
  manager.OnSurroundingTextChanged(u"There is error.", 0, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1000));

  auto grammar_fragments =
      mock_ime_input_context_handler.get_grammar_fragments();
  EXPECT_EQ(grammar_fragments.size(), 1);
  EXPECT_EQ(grammar_fragments[0].range, gfx::Range(9, 14));
  EXPECT_EQ(grammar_fragments[0].suggestion, "correct");

  manager.OnSurroundingTextChanged(u"There is a new error.", 0, 0);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1000));

  auto updated_grammar_fragments =
      mock_ime_input_context_handler.get_grammar_fragments();
  EXPECT_EQ(updated_grammar_fragments.size(), 1);
  EXPECT_EQ(updated_grammar_fragments[0].range, gfx::Range(15, 20));
  EXPECT_EQ(updated_grammar_fragments[0].suggestion, "correct");
}

}  // namespace
}  // namespace chromeos
