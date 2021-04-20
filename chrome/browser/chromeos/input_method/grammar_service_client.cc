// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/grammar_service_client.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_offset_string_conversions.h"
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

const uint32_t kMaxQueryLength = 200;

bool IsSentenceEndCharacter(const char ch) {
  return ch == '.' || ch == '!' || ch == '?';
}

bool IsSectionEndCharacter(const char ch) {
  return ch == ')' || ch == ']' || ch == '}' || ch == '\'' || ch == '\"';
}

bool EndsWithSpecialPeriodWord(const std::string& text) {
  // Do not include [Dr.], or [etc.] since they can occur at the end of a
  // proper sentence.
  return base::EndsWith(text, " c.f.") || base::EndsWith(text, " cf.") ||
         base::EndsWith(text, " e.g.") || base::EndsWith(text, " eg.") ||
         base::EndsWith(text, " i.e.") || base::EndsWith(text, " ie.") ||
         base::EndsWith(text, " Mmes.") || base::EndsWith(text, " Mr.") ||
         base::EndsWith(text, " Mrs.") || base::EndsWith(text, " Ms.") ||
         base::EndsWith(text, " Mses.") || base::EndsWith(text, " Mssrs.") ||
         base::EndsWith(text, " Prof.") || base::EndsWith(text, " n.b.") ||
         base::EndsWith(text, " nb.");
}

bool IsSentenceEnding(const std::string& text, uint32_t idx) {
  if (idx < 2 || text[idx] != ' ')
    return false;
  return (IsSentenceEndCharacter(text[idx - 1]) &&
          !EndsWithSpecialPeriodWord(text.substr(0, idx))) ||
         (IsSentenceEndCharacter(text[idx - 2]) &&
          IsSectionEndCharacter(text[idx - 1]));
}

}  // namespace

GrammarServiceClient::GrammarServiceClient() {
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadGrammarChecker(
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
    const std::u16string& text,
    TextCheckCompleteCallback callback) const {
  if (!profile || !IsAvailable(profile)) {
    std::move(callback).Run(false, {});
    return false;
  }

  // We need to trim the query if it is too long, otherwise it may overwhelm
  // CPU.
  uint32_t query_offset = 0;
  std::string query_text = base::UTF16ToUTF8(text);
  if (query_text.size() > kMaxQueryLength) {
    query_offset = query_text.size() - kMaxQueryLength;
    while (query_offset < query_text.size() &&
           !IsSentenceEnding(query_text, query_offset))
      query_offset++;
    // Change index from sentence ending to the next sentence start.
    query_offset++;
    if (query_offset >= query_text.size()) {
      // If we cannot isolate a sentence from a long query, we don't process
      // the query.
      std::move(callback).Run(false, {});
      return false;
    }
  }

  auto query = GrammarCheckerQuery::New();
  query->text = query_text.substr(query_offset);
  query->language = "en-US";

  grammar_checker_->Check(
      std::move(query),
      base::BindOnce(&GrammarServiceClient::ParseGrammarCheckerResult,
                     base::Unretained(this), query_text, query_offset,
                     std::move(callback)));

  return true;
}

void GrammarServiceClient::ParseGrammarCheckerResult(
    const std::string& query_text,
    const uint32_t query_offset,
    TextCheckCompleteCallback callback,
    chromeos::machine_learning::mojom::GrammarCheckerResultPtr result) const {
  if (result->status == GrammarCheckerResult::Status::OK &&
      !result->candidates.empty()) {
    const auto& top_candidate = result->candidates.front();
    if (!top_candidate->text.empty() && !top_candidate->fragments.empty()) {
      std::vector<SpellCheckResult> grammar_results;
      for (const auto& fragment : top_candidate->fragments) {
        uint32_t start;
        uint32_t end;
        if (!base::CheckAdd(query_offset, fragment->offset)
                 .AssignIfValid(&start) ||
            !base::CheckAdd(start, fragment->length).AssignIfValid(&end) ||
            end > query_text.size()) {
          DLOG(ERROR) << "Grammar checker returns invalid correction "
                         "fragment, offset: "
                      << query_offset + fragment->offset
                      << ", length: " << fragment->length
                      << ", but the text length is " << query_text.size();
        } else {
          // Compute the offsets in string16.
          std::vector<size_t> offsets = {start, end};
          base::UTF8ToUTF16AndAdjustOffsets(query_text, &offsets);
          grammar_results.emplace_back(
              SpellCheckResult::GRAMMAR, offsets[0], offsets[1] - offsets[0],
              base::UTF8ToUTF16(fragment->replacement));
        }
      }
      std::move(callback).Run(true, grammar_results);
      return;
    }
  }
  std::move(callback).Run(false, {});
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
