// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_search_controller.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/picker/model/picker_category.h"
#include "ash/picker/model/picker_search_results.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace ash {

enum class AppListSearchResultType;

namespace {

// TODO: b/316936687 - Use the icons from real search results.
const gfx::VectorIcon& kPlaceholderIcon = kCheckIcon;

PickerSearchResults::Section GetFakeExpressionsSection(
    base::span<const PickerSearchResult> gif_results) {
  std::vector<PickerSearchResult> results = {
      PickerSearchResult::Emoji(u"👍"),
      PickerSearchResult::Emoji(u"😊"),
      PickerSearchResult::Symbol(u"⊃"),
      PickerSearchResult::Symbol(u"⊇"),
      PickerSearchResult::Symbol(u"♬"),
      PickerSearchResult::Emoticon(u"¯\\_(ツ)_/¯")};
  results.insert(results.end(), gif_results.begin(), gif_results.end());
  return PickerSearchResults::Section(u"Matching expressions",
                                      std::move(results));
}

}  // namespace

PickerSearchController::PickerSearchController(PickerClient* client)
    : client_(CHECK_DEREF(client)) {}

PickerSearchController::~PickerSearchController() = default;

void PickerSearchController::StartSearch(
    const std::u16string& query,
    std::optional<PickerCategory> category,
    PickerViewDelegate::SearchResultsCallback callback) {
  client_->StopCrosQuery();
  ResetResults();
  current_callback_ = std::move(callback);
  current_query_ = query;
  client_->StartCrosSearch(
      query, base::BindRepeating(&PickerSearchController::HandleSearchResults,
                                 weak_ptr_factory_.GetWeakPtr()));
  client_->FetchGifSearch(
      base::UTF16ToUTF8(query),
      base::BindOnce(&PickerSearchController::HandleGifSearchResults,
                     weak_ptr_factory_.GetWeakPtr(), query));

  // Show fake results while we wait for responses.
  // TODO: b/324154537 - Show a loading animation instead.
  RunCallback();
}

void PickerSearchController::ResetResults() {
  omnibox_results_ = std::vector({
      PickerSearchResult::BrowsingHistory(
          GURL("http://www.foo.com"), u"Foo",
          ui::ImageModel::FromVectorIcon(kPlaceholderIcon)),
      PickerSearchResult::BrowsingHistory(
          GURL("http://crbug.com"), u"Crbug",
          ui::ImageModel::FromVectorIcon(kPlaceholderIcon)),
  });
  gif_results_ = std::vector({PickerSearchResult::Gif(
      GURL("https://media.tenor.com/BzfS_9uPq_AAAAAd/cat-bonfire.gif"),
      gfx::Size(140, 140), u"gif")});
}

void PickerSearchController::RunCallback() {
  CHECK(current_callback_);
  current_callback_.Run(PickerSearchResults({{
      GetFakeExpressionsSection(gif_results_),
      PickerSearchResults::Section(u"Matching links", omnibox_results_),
  }}));
}

void PickerSearchController::HandleSearchResults(
    ash::AppListSearchResultType type,
    std::vector<PickerSearchResult> results) {
  omnibox_results_ = std::move(results);
  RunCallback();
}

void PickerSearchController::HandleGifSearchResults(
    std::u16string query,
    std::vector<PickerSearchResult> results) {
  // As we cannot stop GIF search result callbacks, check whether the query for
  // this request is the current query to prevent showing results from an
  // outdated query.
  // TODO: b/324992789 - Allow stopping GIF search results.
  if (query == current_query_) {
    gif_results_ = std::move(results);
    RunCallback();
  }
}
}  // namespace ash
