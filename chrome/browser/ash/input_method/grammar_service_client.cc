// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/grammar_service_client.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "ui/gfx/range/range.h"

namespace ash {
namespace input_method {
namespace {

using ::chromeos::machine_learning::mojom::GrammarCheckerQuery;
using ::chromeos::machine_learning::mojom::GrammarCheckerQueryPtr;
using ::chromeos::machine_learning::mojom::GrammarCheckerResult;
using ::chromeos::machine_learning::mojom::GrammarCheckerResultPtr;
using ::chromeos::machine_learning::mojom::LoadModelResult;
using ::chromeos::machine_learning::mojom::TextLanguagePtr;

const uint32_t kMaxQueryLength = 200;
const uint32_t kMinQueryLength = 5;
const double kLanguageConfidenceThreshold = 0.9;
const char kEnglishLocale[] = "en";

}  // namespace

GrammarServiceClient::GrammarServiceClient() {
  weak_this_ = weak_factory_.GetWeakPtr();
}

GrammarServiceClient::~GrammarServiceClient() = default;

void GrammarServiceClient::OnLoadGrammarCheckerDone(
    GrammarCheckerQueryPtr query,
    const std::string& query_text,
    TextCheckCompleteCallback callback,
    LoadModelResult result) {
  grammar_checker_loaded_ = result == LoadModelResult::OK;
  if (!grammar_checker_loaded_) {
    std::move(callback).Run(false, {});
    return;
  }
  grammar_checker_->Check(
      std::move(query),
      base::BindOnce(&GrammarServiceClient::ParseGrammarCheckerResult,
                     weak_this_, query_text, std::move(callback)));
}

void GrammarServiceClient::OnLoadTextClassifierDone(
    const std::string& query_text,
    TextCheckCompleteCallback callback,
    LoadModelResult result) {
  text_classifier_loaded_ = result == LoadModelResult::OK;
  if (!text_classifier_loaded_) {
    std::move(callback).Run(false, {});
    return;
  }
  text_classifier_->FindLanguages(
      query_text, base::BindOnce(&GrammarServiceClient::OnLanguageDetectionDone,
                                 weak_this_, query_text, std::move(callback)));
}

bool GrammarServiceClient::RequestTextCheck(
    Profile* profile,
    const std::u16string& text,
    TextCheckCompleteCallback callback) {
  if (!profile || !IsAvailable(profile) || text.size() > kMaxQueryLength ||
      text.size() < kMinQueryLength) {
    std::move(callback).Run(false, {});
    return false;
  }

  if (text_classifier_loaded_) {
    text_classifier_->FindLanguages(
        base::UTF16ToUTF8(text),
        base::BindOnce(&GrammarServiceClient::OnLanguageDetectionDone,
                       weak_this_, base::UTF16ToUTF8(text),
                       std::move(callback)));
    return true;
  }

  if (!text_classifier_.is_bound()) {
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->GetMachineLearningService()
        .LoadTextClassifier(
            text_classifier_.BindNewPipeAndPassReceiver(),
            base::BindOnce(&GrammarServiceClient::OnLoadTextClassifierDone,
                           weak_this_, base::UTF16ToUTF8(text),
                           std::move(callback)));
    return true;
  }

  std::move(callback).Run(false, {});
  return false;
}

void GrammarServiceClient::OnLanguageDetectionDone(
    const std::string& query_text,
    TextCheckCompleteCallback callback,
    std::vector<TextLanguagePtr> languages) {
  if (languages.empty() ||
      languages[0]->confidence < kLanguageConfidenceThreshold ||
      languages[0]->locale != kEnglishLocale) {
    std::move(callback).Run(false, {});
    return;
  }

  auto query = GrammarCheckerQuery::New();
  query->text = query_text;
  query->language = languages[0]->locale;

  if (grammar_checker_loaded_) {
    grammar_checker_->Check(
        std::move(query),
        base::BindOnce(&GrammarServiceClient::ParseGrammarCheckerResult,
                       weak_this_, query_text, std::move(callback)));
    return;
  }

  if (!grammar_checker_.is_bound()) {
    chromeos::machine_learning::ServiceConnection::GetInstance()
        ->GetMachineLearningService()
        .LoadGrammarChecker(
            grammar_checker_.BindNewPipeAndPassReceiver(),
            base::BindOnce(&GrammarServiceClient::OnLoadGrammarCheckerDone,
                           weak_this_, std::move(query), query_text,
                           std::move(callback)));
    return;
  }

  std::move(callback).Run(false, {});
}

void GrammarServiceClient::ParseGrammarCheckerResult(
    const std::string& query_text,
    TextCheckCompleteCallback callback,
    GrammarCheckerResultPtr result) const {
  if (result->status == GrammarCheckerResult::Status::OK &&
      !result->candidates.empty()) {
    const auto& top_candidate = result->candidates.front();
    if (!top_candidate->text.empty() && !top_candidate->fragments.empty()) {
      std::vector<ui::GrammarFragment> grammar_results;
      for (const auto& fragment : top_candidate->fragments) {
        uint32_t end;
        if (!base::CheckAdd(fragment->offset, fragment->length)
                 .AssignIfValid(&end) ||
            end > query_text.size()) {
          DLOG(ERROR) << "Grammar checker returns invalid correction "
                         "fragment, offset: "
                      << fragment->offset << ", length: " << fragment->length
                      << ", but the text length is " << query_text.size();
        } else {
          // Compute the offsets in string16.
          std::vector<size_t> offsets = {fragment->offset, end};
          base::UTF8ToUTF16AndAdjustOffsets(query_text, &offsets);
          grammar_results.emplace_back(gfx::Range(offsets[0], offsets[1]),
                                       fragment->replacement);
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
  // If prefs don't allow spell checking, if the profile is off the record, the
  // grammar service should be unavailable.
  return pref->GetBoolean(spellcheck::prefs::kSpellCheckEnable) &&
         !profile->IsOffTheRecord();
}

}  // namespace input_method
}  // namespace ash
