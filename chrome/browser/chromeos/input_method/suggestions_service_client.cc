// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/suggestions_service_client.h"

#include "base/bind.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {
namespace {

using ::chromeos::ime::TextSuggestion;
using ::chromeos::ime::TextSuggestionMode;
using ::chromeos::ime::TextSuggestionType;
using ::chromeos::machine_learning::mojom::NextWordCompletionCandidate;
using ::chromeos::machine_learning::mojom::TextSuggesterQuery;
using ::chromeos::machine_learning::mojom::TextSuggesterResultPtr;
using ::chromeos::machine_learning::mojom::TextSuggestionCandidatePtr;

absl::optional<TextSuggestion> ToTextSuggestion(
    const TextSuggestionCandidatePtr& candidate) {
  if (!candidate->is_multi_word()) {
    // TODO(crbug/1146266): Handle emoji suggestions
    return absl::nullopt;
  }

  return TextSuggestion{
      // TODO(crbug/1146266): Introduce suggestion mode to suggestion service
      // interface. For the moment, everything is a completion.
      .mode = TextSuggestionMode::kCompletion,
      .type = TextSuggestionType::kMultiWord,
      .text = candidate->get_multi_word()->text};
}

}  // namespace

SuggestionsServiceClient::SuggestionsServiceClient() {
  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadTextSuggester(
          text_suggester_.BindNewPipeAndPassReceiver(),
          base::BindOnce(
              [](bool* text_suggester_loaded_,
                 chromeos::machine_learning::mojom::LoadModelResult result) {
                *text_suggester_loaded_ =
                    result ==
                    chromeos::machine_learning::mojom::LoadModelResult::OK;
              },
              &text_suggester_loaded_));
}

SuggestionsServiceClient::~SuggestionsServiceClient() = default;

void SuggestionsServiceClient::RequestSuggestions(
    const std::string& preceding_text,
    const ime::TextSuggestionMode& suggestion_mode,
    const std::vector<ime::TextCompletionCandidate>& completion_candidates,
    RequestSuggestionsCallback callback) {
  if (!IsAvailable() ||
      suggestion_mode != ime::TextSuggestionMode::kCompletion) {
    // TODO(crbug/1146266): Support prediction requests when suggestion mojo
    // service introduces suggestion_mode to interface.
    std::move(callback).Run({});
    return;
  }

  auto query = TextSuggesterQuery::New();
  query->text = preceding_text;

  for (const auto& candidate : completion_candidates) {
    auto next_word_candidate = NextWordCompletionCandidate::New();
    next_word_candidate->text = candidate.text;
    next_word_candidate->normalized_score = candidate.score;
    query->next_word_candidates.push_back(std::move(next_word_candidate));
  }

  text_suggester_->Suggest(
      std::move(query),
      base::BindOnce(&SuggestionsServiceClient::OnSuggestionsReturned,
                     base::Unretained(this), std::move(callback)));
}

void SuggestionsServiceClient::OnSuggestionsReturned(
    RequestSuggestionsCallback callback,
    chromeos::machine_learning::mojom::TextSuggesterResultPtr result) {
  std::vector<TextSuggestion> suggestions;

  for (const auto& candidate : result->candidates) {
    auto suggestion = ToTextSuggestion(std::move(candidate));
    if (suggestion) {
      // Drop any unknown suggestions
      suggestions.push_back(suggestion.value());
    }
  }

  std::move(callback).Run(suggestions);
}

bool SuggestionsServiceClient::IsAvailable() {
  return text_suggester_loaded_;
}

}  // namespace chromeos
