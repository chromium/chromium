// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/quick_insert_suggestions_controller.h"

#include "ash/quick_insert/model/quick_insert_mode_type.h"
#include "ash/quick_insert/model/quick_insert_model.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_client.h"
#include "ash/quick_insert/quick_insert_clipboard_history_provider.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/quick_insert_shortcuts.h"
#include "ash/quick_insert/search/quick_insert_date_search.h"
#include "ash/quick_insert/search/quick_insert_math_search.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/emoji/gif_tenor_api_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {
namespace {

constexpr int kMaxRecentFiles = 10;
constexpr int kMaxRecentLinks = 10;
constexpr base::TimeDelta kMaxLocalFileSuggestionRecencyDelta = base::Days(30);
constexpr base::TimeDelta kMaxLocalFileCategoryRecencyDelta = base::Days(3652);

std::vector<QuickInsertSearchResult> ConvertToSearchResults(
    base::expected<tenor::mojom::PaginatedGifResponsesPtr,
                   GifTenorApiFetcher::Error> response) {
  if (!response.has_value()) {
    // TODO: b/325368650 - Add better handling of errors.
    return {};
  }
  size_t rank = 0;
  return base::ToVector(
      (*response)->results, [&rank](tenor::mojom::GifResponsePtr& result) {
        CHECK(result);
        tenor::mojom::GifUrlsPtr& urls = result->url;
        CHECK(urls);
        return QuickInsertSearchResult(QuickInsertGifResult(
            std::move(urls->preview), std::move(urls->preview_image),
            result->preview_size, std::move(urls->full), result->full_size,
            base::UTF8ToUTF16(result->content_description), rank++));
      });
}

}  // namespace

QuickInsertSuggestionsController::QuickInsertSuggestionsController() = default;
QuickInsertSuggestionsController::~QuickInsertSuggestionsController() = default;

std::vector<QuickInsertSearchResult> GetMostRecentResults(
    size_t n,
    std::vector<QuickInsertSearchResult> results) {
  if (results.size() > n) {
    results.erase(results.begin() + n, results.end());
  }
  return results;
}

void QuickInsertSuggestionsController::GetSuggestions(
    QuickInsertClient& client,
    const QuickInsertModel& model,
    SuggestionsCallback callback) {
  if (model.GetMode() == QuickInsertModeType::kUnfocused) {
    std::vector<QuickInsertSearchResult> new_window_results;
    for (QuickInsertNewWindowResult::Type type : {
             QuickInsertNewWindowResult::Type::kDoc,
             QuickInsertNewWindowResult::Type::kSheet,
             QuickInsertNewWindowResult::Type::kSlide,
             QuickInsertNewWindowResult::Type::kChrome,
         }) {
      new_window_results.push_back(QuickInsertNewWindowResult(type));
    }
    callback.Run(std::move(new_window_results));
  }

  if (model.GetMode() == QuickInsertModeType::kUnfocused ||
      model.GetMode() == QuickInsertModeType::kNoSelection) {
    callback.Run({QuickInsertCapsLockResult(
        !model.is_caps_lock_enabled(), GetQuickInsertShortcutForCapsLock())});
  }

  if (base::Contains(model.GetAvailableCategories(),
                     QuickInsertCategory::kEditorRewrite)) {
    client.GetSuggestedEditorResults(callback);
  }

  if (base::Contains(model.GetAvailableCategories(),
                     QuickInsertCategory::kLobsterWithSelectedText)) {
    callback.Run({QuickInsertLobsterResult(
        QuickInsertLobsterResult::Mode::kWithSelection, /*display_name=*/u"")});
  }

  if (model.GetMode() == QuickInsertModeType::kHasSelection) {
    std::vector<QuickInsertSearchResult> case_transform_results;
    for (QuickInsertCaseTransformResult::Type type : {
             QuickInsertCaseTransformResult::Type::kUpperCase,
             QuickInsertCaseTransformResult::Type::kLowerCase,
             QuickInsertCaseTransformResult::Type::kTitleCase,
         }) {
      case_transform_results.push_back(QuickInsertCaseTransformResult(type));
    }
    callback.Run(std::move(case_transform_results));
  }

  // TODO: b/344685737 - Rank and collect suggestions in a more intelligent way.
  for (QuickInsertCategory category : model.GetRecentResultsCategories()) {
    // Special case certain categories where we can save computation by only
    // asking for 1 result.
    // TODO: b/357740941: Request only one Drive file once directory filtering
    // is implemented inside DriveFS.
    // TODO: b/366237507 - Request only one Link result once HistoryService
    // supports filtering.
    switch (category) {
      case QuickInsertCategory::kLinks:
        link_suggester_.GetSuggestedLinks(
            client.GetHistoryService(), client.GetFaviconService(),
            /*max_results=*/10,
            base::BindRepeating(&GetMostRecentResults, 1).Then(callback));
        break;
      case QuickInsertCategory::kLocalFiles: {
        const size_t max_results = 3;
        client.GetRecentLocalFileResults(
            max_results, kMaxLocalFileSuggestionRecencyDelta,
            base::BindRepeating(&GetMostRecentResults, max_results)
                .Then(callback));
        break;
      }
      case QuickInsertCategory::kDriveFiles:
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

void QuickInsertSuggestionsController::GetSuggestionsForCategory(
    QuickInsertClient& client,
    QuickInsertCategory category,
    SuggestionsCallback callback) {
  switch (category) {
    case QuickInsertCategory::kEditorWrite:
    case QuickInsertCategory::kEditorRewrite:
    case QuickInsertCategory::kLobsterWithNoSelectedText:
    case QuickInsertCategory::kLobsterWithSelectedText:
      NOTREACHED();
    case QuickInsertCategory::kLinks:
      // TODO: b/366237507 - Request only kMaxRecentLinks results once
      // HistoryService supports filtering.
      link_suggester_.GetSuggestedLinks(
          client.GetHistoryService(), client.GetFaviconService(),
          kMaxRecentLinks * 3, std::move(callback));
      return;
    case QuickInsertCategory::kEmojisGifs:
    case QuickInsertCategory::kEmojis:
      NOTREACHED();
    case QuickInsertCategory::kGifs:
      GifTenorApiFetcher::FetchFeaturedGifs(
          client.GetSharedURLLoaderFactory(),
          /*=*/std::nullopt,
          base::BindOnce(ConvertToSearchResults).Then(std::move(callback)));
      return;
    case QuickInsertCategory::kDriveFiles:
      client.GetRecentDriveFileResults(kMaxRecentFiles, std::move(callback));
      return;
    case QuickInsertCategory::kLocalFiles:
      client.GetRecentLocalFileResults(kMaxRecentFiles,
                                       kMaxLocalFileCategoryRecencyDelta,
                                       std::move(callback));
      return;
    case QuickInsertCategory::kDatesTimes:
      std::move(callback).Run(QuickInsertSuggestedDateResults());
      break;
    case QuickInsertCategory::kUnitsMaths:
      std::move(callback).Run(QuickInsertMathExamples());
      break;
    case QuickInsertCategory::kClipboard:
      clipboard_provider_.FetchResults(std::move(callback));
      return;
  }
}

}  // namespace ash
