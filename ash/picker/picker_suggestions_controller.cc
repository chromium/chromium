// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_suggestions_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/picker/model/picker_mode_type.h"
#include "ash/picker/model/picker_model.h"
#include "ash/picker/picker_category.h"
#include "ash/picker/picker_client.h"
#include "ash/picker/picker_clipboard_history_provider.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/picker_shortcuts.h"
#include "ash/picker/search/picker_date_search.h"
#include "ash/picker/search/picker_math_search.h"
#include "base/feature_list.h"

namespace ash {
namespace {

constexpr int kMaxRecentFiles = 10;
constexpr int kMaxRecentLinks = 10;
constexpr base::TimeDelta kMaxLocalFileSuggestionRecencyDelta = base::Days(30);
constexpr base::TimeDelta kMaxLocalFileCategoryRecencyDelta = base::Days(3652);
}

PickerSuggestionsController::PickerSuggestionsController() = default;
PickerSuggestionsController::~PickerSuggestionsController() = default;

std::vector<PickerSearchResult> GetMostRecentResults(
    size_t n,
    std::vector<PickerSearchResult> results) {
  if (results.size() > n) {
    results.erase(results.begin() + n, results.end());
  }
  return results;
}

void PickerSuggestionsController::GetSuggestions(PickerClient& client,
                                                 const PickerModel& model,
                                                 SuggestionsCallback callback) {
  if (model.GetMode() == PickerModeType::kUnfocused) {
    std::vector<PickerSearchResult> new_window_results;
    for (PickerNewWindowResult::Type type : {
             PickerNewWindowResult::Type::kDoc,
             PickerNewWindowResult::Type::kSheet,
             PickerNewWindowResult::Type::kSlide,
             PickerNewWindowResult::Type::kChrome,
         }) {
      new_window_results.push_back(PickerNewWindowResult(type));
    }
    callback.Run(std::move(new_window_results));
  }

  if (model.GetMode() == PickerModeType::kUnfocused ||
      model.GetMode() == PickerModeType::kNoSelection) {
    callback.Run({PickerCapsLockResult(!model.is_caps_lock_enabled(),
                                       GetPickerShortcutForCapsLock())});
  }

  if (base::Contains(model.GetAvailableCategories(),
                     PickerCategory::kEditorRewrite)) {
    client.GetSuggestedEditorResults(callback);
  }

  if (base::Contains(model.GetAvailableCategories(),
                     PickerCategory::kLobster) &&
      model.GetMode() == PickerModeType::kHasSelection) {
    callback.Run({PickerLobsterResult(/*display_name=*/u"")});
  }

  if (model.GetMode() == PickerModeType::kHasSelection) {
    std::vector<PickerSearchResult> case_transform_results;
    for (PickerCaseTransformResult::Type type : {
             PickerCaseTransformResult::Type::kUpperCase,
             PickerCaseTransformResult::Type::kLowerCase,
             PickerCaseTransformResult::Type::kTitleCase,
         }) {
      case_transform_results.push_back(PickerCaseTransformResult(type));
    }
    callback.Run(std::move(case_transform_results));
  }

  // TODO: b/344685737 - Rank and collect suggestions in a more intelligent way.
  for (PickerCategory category : model.GetRecentResultsCategories()) {
    // Special case certain categories where we can save computation by only
    // asking for 1 result.
    // TODO: b/357740941: Request only one Drive file once directory filtering
    // is implemented inside DriveFS.
    // TODO: b/366237507 - Request only one Link result once HistoryService
    // supports filtering.
    switch (category) {
      case PickerCategory::kLinks:
        client.GetSuggestedLinkResults(
            /*max_results=*/base::FeatureList::IsEnabled(
                ash::features::kPickerFilterLinks)
                ? 10
                : 1,
            base::BindRepeating(&GetMostRecentResults, 1).Then(callback));
        break;
      case PickerCategory::kLocalFiles: {
        const size_t max_results =
            base::FeatureList::IsEnabled(ash::features::kPickerGrid) ? 3 : 1;
        client.GetRecentLocalFileResults(
            max_results, kMaxLocalFileSuggestionRecencyDelta,
            base::BindRepeating(&GetMostRecentResults, max_results)
                .Then(callback));
        break;
      }
      case PickerCategory::kDriveFiles:
        client.GetRecentDriveFileResults(
            /*max_results=*/5,
            base::BindRepeating(&GetMostRecentResults, 1).Then(callback));
        break;
      default:
        GetSuggestionsForCategory(
            client, category,
            base::BindRepeating(&GetMostRecentResults, 1).Then(callback));
        break;
    }
  }
}

void PickerSuggestionsController::GetSuggestionsForCategory(
    PickerClient& client,
    PickerCategory category,
    SuggestionsCallback callback) {
  switch (category) {
    case PickerCategory::kEditorWrite:
    case PickerCategory::kEditorRewrite:
    case PickerCategory::kLobster:
      NOTREACHED_NORETURN();
    case PickerCategory::kLinks:
      // TODO: b/366237507 - Request only kMaxRecentLinks results once
      // HistoryService supports filtering.
      client.GetSuggestedLinkResults(
          base::FeatureList::IsEnabled(ash::features::kPickerFilterLinks)
              ? kMaxRecentLinks * 3
              : kMaxRecentLinks,
          std::move(callback));
      return;
    case PickerCategory::kEmojisGifs:
    case PickerCategory::kEmojis:
      NOTREACHED_NORETURN();
    case PickerCategory::kDriveFiles:
      client.GetRecentDriveFileResults(kMaxRecentFiles, std::move(callback));
      return;
    case PickerCategory::kLocalFiles:
      client.GetRecentLocalFileResults(
          kMaxRecentFiles,
          base::FeatureList::IsEnabled(ash::features::kPickerRecentFiles)
              ? kMaxLocalFileCategoryRecencyDelta
              : kMaxLocalFileSuggestionRecencyDelta,
          std::move(callback));
      return;
    case PickerCategory::kDatesTimes:
      std::move(callback).Run(PickerSuggestedDateResults());
      break;
    case PickerCategory::kUnitsMaths:
      std::move(callback).Run(PickerMathExamples());
      break;
    case PickerCategory::kClipboard:
      clipboard_provider_.FetchResults(std::move(callback));
      return;
  }
}

}  // namespace ash
