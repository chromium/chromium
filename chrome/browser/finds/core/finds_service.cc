// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_service.h"

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/finds.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"

namespace finds {

namespace {

// The duration of history to look back when gathering URLs for theme
// suggestions.
// TODO(crbug.com/493283477): Align with notif cooldown.
constexpr base::TimeDelta kHistoryLookbackInterval = base::Days(7);

std::string FindsSuggestionResponseToHumanReadableString(
    const optimization_guide::proto::FindsSuggestionResponse& response) {
  std::vector<std::string> lines;
  for (int i = 0; i < response.suggestions_size(); ++i) {
    const auto& theme = response.suggestions(i);
    lines.push_back(base::StringPrintf("Theme: %s (Type: %d, Score: %lld)",
                                       theme.theme_title().c_str(),
                                       static_cast<int>(theme.theme_type()),
                                       static_cast<int64_t>(theme.score())));
    for (int j = 0; j < theme.suggestions_size(); ++j) {
      const auto& suggestion = theme.suggestions(j);
      lines.push_back(base::StringPrintf("  - %s: %s",
                                         suggestion.title().c_str(),
                                         suggestion.target_url().c_str()));
    }
  }
  return base::JoinString(lines, "\n");
}

}  // namespace

FindsService::FindsService(OptimizationGuideKeyedService* opt_guide_service,
                           history::HistoryService* history_service)
    : opt_guide_service_(opt_guide_service),
      history_service_(history_service) {}

FindsService::~FindsService() {
  history_task_tracker_.TryCancelAll();
}

void FindsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FindsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FindsService::GetModelResponse(base::OnceCallback<void(Result)> callback) {
  if (!history_service_) {
    std::move(callback).Run({Result::Status::kHistoryServiceUnavailable,
                             "Error: HistoryService not available."});
    return;
  }

  if (!opt_guide_service_) {
    std::move(callback).Run(
        {Result::Status::kOptimizationGuideUnavailable,
         "Error: OptimizationGuideKeyedService not available."});
    return;
  }

  history::QueryOptions options;
  options.begin_time = base::Time::Now() - kHistoryLookbackInterval;

  history_service_->QueryHistory(
      std::u16string(), options,
      base::BindOnce(&FindsService::OnHistoryQueryComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      &history_task_tracker_);
}

void FindsService::OnHistoryQueryComplete(
    base::OnceCallback<void(Result)> callback,
    history::QueryResults results) {
  if (!opt_guide_service_) {
    std::move(callback).Run(
        {Result::Status::kOptimizationGuideUnavailable,
         "Error: OptimizationGuideKeyedService not available."});
    return;
  }

  if (results.empty()) {
    std::move(callback).Run({Result::Status::kEmptyHistory,
                             "Error: No history available to suggest themes."});
    return;
  }

  optimization_guide::proto::FindsSuggestionRequest request;
  for (const auto& result : results) {
    auto* entry = request.add_entries();
    entry->set_visit_time_usec(
        (result.visit_time() - base::Time::UnixEpoch()).InMicroseconds());
    entry->set_title(base::UTF16ToUTF8(result.title()));
    entry->set_url(result.url().spec());
  }

  opt_guide_service_->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kFinds, request, {},
      base::BindOnce(&FindsService::OnModelExecutionComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FindsService::OnModelExecutionComplete(
    base::OnceCallback<void(Result)> callback,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  if (!result.response.has_value()) {
    std::string error_message =
        base::StringPrintf("Model execution failed. Error code: %d",
                           static_cast<int>(result.response.error().error()));
    std::move(callback).Run(
        {Result::Status::kModelExecutionFailed, error_message});
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::FindsSuggestionResponse>(
      result.response.value());
  if (response) {
    std::move(callback).Run(
        {Result::Status::kSuccess,
         FindsSuggestionResponseToHumanReadableString(*response)});
  } else {
    std::move(callback).Run(
        {Result::Status::kResponseParsingFailed,
         "Model execution successful, but failed to parse response."});
  }
}

}  // namespace finds
