// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/suggestions_service_client.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/input_method/suggestion_enums.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"

namespace ash {
namespace input_method {
namespace {

using ::chromeos::machine_learning::mojom::MultiWordExperimentGroup;
using ::chromeos::machine_learning::mojom::NextWordCompletionCandidate;
using ::chromeos::machine_learning::mojom::TextSuggesterQuery;
using ::chromeos::machine_learning::mojom::TextSuggesterResultPtr;
using ::chromeos::machine_learning::mojom::TextSuggesterSpec;
using ::chromeos::machine_learning::mojom::TextSuggestionCandidatePtr;
using ime::AssistiveSuggestion;
using ime::AssistiveSuggestionMode;
using ime::AssistiveSuggestionType;

constexpr size_t kMaxNumberCharsSent = 100;

MultiWordExperimentGroup GetExperimentGroup(const std::string& finch_trial) {
  if (finch_trial == "gboard") {
    return MultiWordExperimentGroup::kGboard;
  }
  if (finch_trial == "gboard_relaxed_a") {
    return MultiWordExperimentGroup::kGboardRelaxedA;
  }
  if (finch_trial == "gboard_relaxed_b") {
    return MultiWordExperimentGroup::kGboardRelaxedB;
  }
  if (finch_trial == "gboard_relaxed_c") {
    return MultiWordExperimentGroup::kGboardRelaxedC;
  }
  if (finch_trial == "gboard_d") {
    return MultiWordExperimentGroup::kGboardD;
  }
  if (finch_trial == "gboard_e") {
    return MultiWordExperimentGroup::kGboardE;
  }
  if (finch_trial == "gboard_f") {
    return MultiWordExperimentGroup::kGboardF;
  }
  return MultiWordExperimentGroup::kGboardE;
}

chromeos::machine_learning::mojom::TextSuggestionMode ToTextSuggestionModeMojom(
    AssistiveSuggestionMode suggestion_mode) {
  switch (suggestion_mode) {
    case AssistiveSuggestionMode::kCompletion:
      return chromeos::machine_learning::mojom::TextSuggestionMode::kCompletion;
    case AssistiveSuggestionMode::kPrediction:
      return chromeos::machine_learning::mojom::TextSuggestionMode::kPrediction;
  }
}

std::optional<AssistiveSuggestion> ToAssistiveSuggestion(
    const TextSuggestionCandidatePtr& candidate,
    const AssistiveSuggestionMode& suggestion_mode) {
  if (!candidate->is_multi_word()) {
    // TODO(crbug/1146266): Handle emoji suggestions
    return std::nullopt;
  }

  return AssistiveSuggestion{.mode = suggestion_mode,
                             .type = AssistiveSuggestionType::kMultiWord,
                             .text = candidate->get_multi_word()->text};
}

std::string TrimText(const std::string& text) {
  size_t text_length = text.length();
  return text_length > kMaxNumberCharsSent
             ? text.substr(text_length - kMaxNumberCharsSent)
             : text;
}

MultiWordSuggestionType ToSuggestionType(
    const ime::AssistiveSuggestionMode& suggestion_mode) {
  switch (suggestion_mode) {
    case ime::AssistiveSuggestionMode::kCompletion:
      return MultiWordSuggestionType::kCompletion;
    case ime::AssistiveSuggestionMode::kPrediction:
      return MultiWordSuggestionType::kPrediction;
    default:
      return MultiWordSuggestionType::kUnknown;
  }
}

void RecordRequestLatency(base::TimeDelta delta) {
  base::UmaHistogramTimes(
      "InputMethod.Assistive.CandidateGenerationTime.MultiWord", delta);
}

void RecordPrecedingTextLength(size_t text_length) {
  base::UmaHistogramCounts1000(
      "InputMethod.Assistive.MultiWord.PrecedingTextLength", text_length);
}

void RecordRequestCandidates(
    const ime::AssistiveSuggestionMode& suggestion_mode) {
  base::UmaHistogramEnumeration(
      "InputMethod.Assistive.MultiWord.RequestCandidates",
      ToSuggestionType(suggestion_mode));
}

void RecordEmptyCandidate(const ime::AssistiveSuggestionMode& suggestion_mode) {
  UMA_HISTOGRAM_ENUMERATION("InputMethod.Assistive.MultiWord.EmptyCandidate",
                            ToSuggestionType(suggestion_mode));
}

void RecordCandidatesGenerated(AssistiveSuggestionMode suggestion_mode) {
  base::UmaHistogramEnumeration(
      "InputMethod.Assistive.MultiWord.CandidatesGenerated",
      ToSuggestionType(suggestion_mode));
}

}  // namespace

SuggestionsServiceClient::SuggestionsServiceClient() {
  std::string field_trial = base::GetFieldTrialParamValueByFeature(
      features::kAssistMultiWord, "group");
  auto spec = TextSuggesterSpec::New(GetExperimentGroup(field_trial));

  chromeos::machine_learning::ServiceConnection::GetInstance()
      ->GetMachineLearningService()
      .LoadTextSuggester(
          text_suggester_.BindNewPipeAndPassReceiver(), std::move(spec),
          base::BindOnce(&SuggestionsServiceClient::OnTextSuggesterLoaded,
                         weak_ptr_factory_.GetWeakPtr()));
}

SuggestionsServiceClient::~SuggestionsServiceClient() = default;

void SuggestionsServiceClient::OnTextSuggesterLoaded(
    chromeos::machine_learning::mojom::LoadModelResult result) {
  text_suggester_loaded_ =
      result == chromeos::machine_learning::mojom::LoadModelResult::OK;
}

void SuggestionsServiceClient::RequestSuggestions(
    const std::string& preceding_text,
    const ime::AssistiveSuggestionMode& suggestion_mode,
    const std::vector<ime::DecoderCompletionCandidate>& completion_candidates,
    RequestSuggestionsCallback callback) {
  if (!IsAvailable()) {
    std::move(callback).Run({});
    return;
  }

  RecordPrecedingTextLength(preceding_text.size());
  RecordRequestCandidates(suggestion_mode);

  auto query = TextSuggesterQuery::New();
  query->text = TrimText(preceding_text);
  query->suggestion_mode = ToTextSuggestionModeMojom(suggestion_mode);

  for (const auto& candidate : completion_candidates) {
    auto next_word_candidate = NextWordCompletionCandidate::New();
    next_word_candidate->text = candidate.text;
    next_word_candidate->normalized_score = candidate.score;
    if (next_word_candidate->text.empty()) {
      RecordEmptyCandidate(suggestion_mode);
    }
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
    AssistiveSuggestionMode suggestion_mode_requested,
    chromeos::machine_learning::mojom::TextSuggesterResultPtr result) {
  std::vector<AssistiveSuggestion> suggestions;

  if (result->candidates.size() > 0) {
    RecordCandidatesGenerated(suggestion_mode_requested);
  }

  for (const auto& candidate : result->candidates) {
    auto suggestion =
        ToAssistiveSuggestion(std::move(candidate), suggestion_mode_requested);
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

}  // namespace input_method
}  // namespace ash
