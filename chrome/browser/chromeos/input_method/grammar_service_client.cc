// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/grammar_service_client.h"

#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/user_prefs/user_prefs.h"

namespace chromeos {
namespace {

using chromeos::machine_learning::mojom::GrammarCheckerQuery;
using chromeos::machine_learning::mojom::GrammarCheckerResult;
using chromeos::machine_learning::mojom::GrammarCheckerResultPtr;

}  // namespace

GrammarServiceClient::GrammarServiceClient() {
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->LoadGrammarChecker(
          grammar_checker_.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](bool* grammar_checker_loaded_,
                 chromeos::machine_learning::mojom::LoadModelResult result) {
                *grammar_checker_loaded_ =
                    result ==
                    chromeos::machine_learning::mojom::LoadModelResult::OK;
              },
              &grammar_checker_loaded_));
}

GrammarServiceClient::~GrammarServiceClient() = default;

bool GrammarServiceClient::RequestTextCheck(
    Profile* profile,
    const base::string16& text,
    TextCheckCompleteCallback callback) const {
  if (!profile || !IsAvailable(profile)) {
    std::move(callback).Run(false, {});
    return false;
  }

  auto query = GrammarCheckerQuery::New();
  query->text = base::UTF16ToUTF8(text);
  query->language = "en-US";
  grammar_checker_->Check(
      std::move(query),
      base::BindOnce(&GrammarServiceClient::ParseGrammarCheckerResult,
                     base::Unretained(this), text, std::move(callback)));

  return true;
}

void GrammarServiceClient::ParseGrammarCheckerResult(
    const base::string16& text,
    TextCheckCompleteCallback callback,
    chromeos::machine_learning::mojom::GrammarCheckerResultPtr result) const {
  if (result->status == GrammarCheckerResult::Status::OK &&
      !result->candidates.empty()) {
    std::vector<SpellCheckResult> grammar_results;
    grammar_results.emplace_back(
        SpellCheckResult::GRAMMAR, 0, text.size(),
        base::UTF8ToUTF16(result->candidates.at(0)->text));
    std::move(callback).Run(true, grammar_results);
  }
}

bool GrammarServiceClient::IsAvailable(Profile* profile) const {
  const PrefService* pref = profile->GetPrefs();
  DCHECK(pref);
  // If prefs don't allow spell checking, if enhanced spell check is disabled,
  // or if the profile is off the record, the grammar service should be
  // unavailable.
  if (!pref->GetBoolean(spellcheck::prefs::kSpellCheckEnable) ||
      !pref->GetBoolean(spellcheck::prefs::kSpellCheckUseSpellingService) ||
      profile->IsOffTheRecord())
    return false;

  return grammar_checker_loaded_ && grammar_checker_.is_bound();
}

}  // namespace chromeos
