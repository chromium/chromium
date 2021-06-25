// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/suggestions_service_client.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
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

constexpr size_t kMaxNumberCharsSent = 100;

machine_learning::mojom::TextSuggestionMode ToTextSuggestionModeMojom(
    TextSuggestionMode suggestion_mode) {
  switch (suggestion_mode) {
    case TextSuggestionMode::kCompletion:
      return machine_learning::mojom::TextSuggestionMode::kCompletion;
    case TextSuggestionMode::kPrediction:
      return machine_learning::mojom::TextSuggestionMode::kPrediction;
  }
}

absl::optional<TextSuggestion> ToTextSuggestion(
    const TextSuggestionCandidatePtr& candidate,
    const TextSuggestionMode& suggestion_mode) {
  if (!candidate->is_multi_word()) {
    // TODO(crbug/1146266): Handle emoji suggestions
    return absl::nullopt;
  }

  return TextSuggestion{.mode = suggestion_mode,
                        .type = TextSuggestionType::kMultiWord,
                        .text = candidate->get_multi_word()->text};
}

std::string TrimText(const std::string& text) {
  size_t text_length = text.length();
  return text_length > kMaxNumberCharsSent
             ? text.substr(text_length - kMaxNumberCharsSent)
             : text;
}

void RecordRequestLatency(base::TimeDelta delta) {
  base::UmaHistogramTimes(
      "InputMethod.Assistive.CandidateGenerationTime.MultiWord", delta);
}

}  // namespace

SuggestionsServiceClient::SuggestionsServiceClient() {
  auto spec = chromeos::machine_learning::mojom::TextSuggesterSpec::New(
      chromeos::machine_learning::mojom::MultiWordExperimentGroup::kGboard);

  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadTextSuggester(
          text_suggester_.BindNewPipeAndPassReceiver(), std::move(spec),
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
  if (!IsAvailable()) {
    std::move(callback).Run({});
    return;
  }

  auto query = TextSuggesterQuery::New();
  query->text = TrimText(preceding_text);
  query->suggestion_mode = ToTextSuggestionModeMojom(suggestion_mode);

  for (const auto& candidate : completion_candidates) {
    auto next_word_candidate = NextWordCompletionCandidate::New();
    next_word_candidate->text = candidate.text;
    next_word_candidate->normalized_score = candidate.score;
    query->next_word_candidates.push_back(std::move(next_word_candidate));
  }

  text_suggester_->Suggest(
      std::move(query),
      base::BindOnce(&SuggestionsServiceClient::OnSuggestionsReturned,
                     base::Unretained(this), base::TimeTicks::Now(),
                     std::move(callback), suggestion_mode));
}

void SuggestionsServiceClient::OnSuggestionsReturned(
    base::TimeTicks time_request_was_made,
    RequestSuggestionsCallback callback,
    TextSuggestionMode suggestion_mode_requested,
    chromeos::machine_learning::mojom::TextSuggesterResultPtr result) {
  std::vector<TextSuggestion> suggestions;

  for (const auto& candidate : result->candidates) {
    auto suggestion =
        ToTextSuggestion(std::move(candidate), suggestion_mode_requested);
    if (suggestion) {
      // Drop any unknown suggestions
      suggestions.push_back(suggestion.value());
    }
  }

  RecordRequestLatency(base::TimeTicks::Now() - time_request_was_made);
  std::move(callback).Run(suggestions);
}

bool SuggestionsServiceClient::IsAvailable() {
  return text_suggester_loaded_;
}

}  // namespace chromeos
