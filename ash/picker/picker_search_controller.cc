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
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace ash {

enum class AppListSearchResultType;

namespace {

// TODO: b/316936687 - Use the icons from real search results.
const gfx::VectorIcon& kPlaceholderIcon = kCheckIcon;

PickerSearchResults::Section GetFakeExpressionsSection() {
  return PickerSearchResults::Section(
      u"Matching expressions",
      {{PickerSearchResult::Emoji(u"👍"), PickerSearchResult::Emoji(u"😊"),
        PickerSearchResult::Symbol(u"⊃"), PickerSearchResult::Symbol(u"⊇"),
        PickerSearchResult::Symbol(u"♬"),
        PickerSearchResult::Emoticon(u"¯\\_(ツ)_/¯"),
        PickerSearchResult::Gif(
            GURL("https://media.tenor.com/BzfS_9uPq_AAAAAd/cat-bonfire.gif"),
            gfx::Size(140, 140), u"gif")}});
}

PickerSearchResults::Section GetFakeLinksSection() {
  return PickerSearchResults::Section(
      u"Matching links",
      {{
          PickerSearchResult::BrowsingHistory(
              GURL("http://www.foo.com"), u"Foo",
              ui::ImageModel::FromVectorIcon(kPlaceholderIcon)),
          PickerSearchResult::BrowsingHistory(
              GURL("http://crbug.com"), u"Crbug",
              ui::ImageModel::FromVectorIcon(kPlaceholderIcon)),
      }});
}

PickerSearchResults::Section GetFakeFilesSection() {
  return PickerSearchResults::Section(
      u"Matching files", {{PickerSearchResult::Text(u"my file"),
                           PickerSearchResult::Text(u"my other file")}});
}

void HandleCrosSearchResults(PickerViewDelegate::SearchResultsCallback callback,
                             AppListSearchResultType type,
                             std::vector<PickerSearchResult> results) {
  callback.Run(PickerSearchResults({{
      GetFakeExpressionsSection(),
      PickerSearchResults::Section(u"Matching links", results),
      GetFakeFilesSection(),
  }}));
}

}  // namespace

PickerSearchController::PickerSearchController(PickerClient* client)
    : client_(CHECK_DEREF(client)) {}

void PickerSearchController::StartSearch(
    const std::u16string& query,
    std::optional<PickerCategory> category,
    PickerViewDelegate::SearchResultsCallback callback) {
  client_->StopCrosQuery();
  // Show fake results while we wait for a response from CrOS Search.
  // TODO: b/324154537 - Show a loading animation instead.
  callback.Run(PickerSearchResults({{
      GetFakeExpressionsSection(),
      GetFakeLinksSection(),
      GetFakeFilesSection(),
  }}));
  client_->StartCrosSearch(query, base::BindRepeating(&HandleCrosSearchResults,
                                                      std::move(callback)));
}

}  // namespace ash
