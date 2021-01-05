// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/grammar_service_client.h"

#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/services/machine_learning/public/cpp/fake_service_connection.h"
#include "chromeos/services/machine_learning/public/mojom/grammar_checker.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

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

  auto profile = std::make_unique<TestingProfile>();
  profile->GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable, false);
  profile->GetPrefs()->SetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService, false);

  GrammarServiceClient client;
  base::RunLoop().RunUntilIdle();

  client.RequestTextCheck(
      profile.get(), base::UTF8ToUTF16("cat"),
      base::BindOnce(
          [](bool success, const std::vector<SpellCheckResult>& results) {
            EXPECT_FALSE(success);
            EXPECT_TRUE(results.empty());
          }));

  base::RunLoop().RunUntilIdle();
}

TEST_F(GrammarServiceClientTest, ParsesResults) {
  machine_learning::FakeServiceConnectionImpl fake_service_connection;
  machine_learning::ServiceConnection::UseFakeServiceConnectionForTesting(
      &fake_service_connection);

  auto profile = std::make_unique<TestingProfile>();
  profile->GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable, true);
  profile->GetPrefs()->SetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService, true);

  // Construct fake output
  const base::string16 input_text = base::UTF8ToUTF16("fake input");
  const base::string16 expected_output = base::UTF8ToUTF16("fake output");
  machine_learning::mojom::GrammarCheckerResultPtr result =
      machine_learning::mojom::GrammarCheckerResult::New();
  result->status = machine_learning::mojom::GrammarCheckerResult::Status::OK;
  machine_learning::mojom::GrammarCheckerCandidatePtr candidate =
      machine_learning::mojom::GrammarCheckerCandidate::New();
  candidate->text = base::UTF16ToUTF8(expected_output);
  candidate->score = 0.5f;
  result->candidates.emplace_back(std::move(candidate));
  fake_service_connection.SetOutputGrammarCheckerResult(result);

  GrammarServiceClient client;
  base::RunLoop().RunUntilIdle();

  client.RequestTextCheck(
      profile.get(), input_text,
      base::BindOnce(
          [](const base::string16& text, const base::string16& expected_output,
             bool success, const std::vector<SpellCheckResult>& results) {
            EXPECT_TRUE(success);
            ASSERT_EQ(results.size(), 1U);
            EXPECT_EQ(results[0].decoration, SpellCheckResult::GRAMMAR);
            EXPECT_EQ(results[0].location, 0);
            EXPECT_EQ(results[0].length, static_cast<int>(text.size()));
            ASSERT_EQ(results[0].replacements.size(), 1U);
            EXPECT_EQ(results[0].replacements[0], expected_output);
          },
          input_text, expected_output));

  base::RunLoop().RunUntilIdle();
}

}  // namespace
}  // namespace chromeos
