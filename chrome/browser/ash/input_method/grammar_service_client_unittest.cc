// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/grammar_service_client.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/grammar_checker.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/grammar_fragment.h"
#include "ui/gfx/range/range.h"

namespace ash {
namespace input_method {
namespace {

namespace machine_learning = ::chromeos::machine_learning;

class GrammarServiceClientTest : public testing::Test {
 public:
  GrammarServiceClientTest() = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(GrammarServiceClientTest, ReturnsEmptyResultWhenSpellCheckIsDiabled) {
  machine_learning::FakeServiceConnectionImpl fake_service_connection;
  machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  machine_learning::ServiceConnection::GetInstance()->Initialize();

  auto profile = std::make_unique<TestingProfile>();
  profile->GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable, false);

  GrammarServiceClient client;
  base::RunLoop().RunUntilIdle();

  client.RequestTextCheck(
      profile.get(), u"cat",
      base::BindOnce(
          [](bool success, const std::vector<ui::GrammarFragment>& results) {
            EXPECT_FALSE(success);
            EXPECT_TRUE(results.empty());
          }));

  base::RunLoop().RunUntilIdle();
}

TEST_F(GrammarServiceClientTest, ParsesResults) {
  machine_learning::FakeServiceConnectionImpl fake_service_connection;
  machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  machine_learning::ServiceConnection::GetInstance()->Initialize();

  auto profile = std::make_unique<TestingProfile>();
  profile->GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable, true);

  // Construct fake output
  machine_learning::mojom::GrammarCheckerResultPtr result =
      machine_learning::mojom::GrammarCheckerResult::New();
  result->status = machine_learning::mojom::GrammarCheckerResult::Status::OK;
  machine_learning::mojom::GrammarCheckerCandidatePtr candidate =
      machine_learning::mojom::GrammarCheckerCandidate::New();
  candidate->text = "fake output";
  candidate->score = 0.5f;
  machine_learning::mojom::GrammarCorrectionFragmentPtr fragment =
      machine_learning::mojom::GrammarCorrectionFragment::New();
  fragment->offset = 3;
  fragment->length = 5;
  fragment->replacement = "fake replacement";
  candidate->fragments.emplace_back(std::move(fragment));
  result->candidates.emplace_back(std::move(candidate));
  fake_service_connection.SetOutputGrammarCheckerResult(result);

  std::vector<machine_learning::mojom::TextLanguagePtr> languages;
  languages.push_back(
      machine_learning::mojom::TextLanguage::New("en", /*confidence=*/1));
  fake_service_connection.SetOutputLanguages(languages);

  GrammarServiceClient client;
  base::RunLoop().RunUntilIdle();

  client.RequestTextCheck(
      profile.get(), u"fake input",
      base::BindOnce(
          [](bool success, const std::vector<ui::GrammarFragment>& results) {
            EXPECT_TRUE(success);
            ASSERT_EQ(results.size(), 1U);
            EXPECT_EQ(results[0].range, gfx::Range(3, 8));
            EXPECT_EQ(results[0].suggestion, "fake replacement");
          }));

  base::RunLoop().RunUntilIdle();
}

TEST_F(GrammarServiceClientTest, RejectsNonEnglishQuery) {
  machine_learning::FakeServiceConnectionImpl fake_service_connection;
  machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  machine_learning::ServiceConnection::GetInstance()->Initialize();

  auto profile = std::make_unique<TestingProfile>();
  profile->GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable, true);

  // Construct fake output
  std::vector<machine_learning::mojom::TextLanguagePtr> languages;
  languages.push_back(
      machine_learning::mojom::TextLanguage::New("jp", /* confidence */ 1));
  fake_service_connection.SetOutputLanguages(languages);

  GrammarServiceClient client;
  base::RunLoop().RunUntilIdle();

  client.RequestTextCheck(
      profile.get(), u"fake input",
      base::BindOnce(
          [](bool success, const std::vector<ui::GrammarFragment>& results) {
            EXPECT_FALSE(success);
          }));

  base::RunLoop().RunUntilIdle();
}

TEST_F(GrammarServiceClientTest, RejectsLongQueries) {
  machine_learning::FakeServiceConnectionImpl fake_service_connection;
  machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);
  machine_learning::ServiceConnection::GetInstance()->Initialize();

  auto profile = std::make_unique<TestingProfile>();
  profile->GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable, true);

  GrammarServiceClient client;
  base::RunLoop().RunUntilIdle();

  const std::u16string long_text =
      u"This is a very very very very very very very very very very very very "
      "very very loooooooooooong sentence, indeed very very very very very "
      "very very very very very very very very very loooooooooooong. Followed "
      "by a fake input sentence.";

  client.RequestTextCheck(
      profile.get(), long_text,
      base::BindOnce(
          [](bool success, const std::vector<ui::GrammarFragment>& results) {
            EXPECT_FALSE(success);
          }));

  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace input_method
}  // namespace ash
