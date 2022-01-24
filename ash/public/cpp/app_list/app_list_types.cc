// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_types.h"

namespace ash {

const char kOemFolderId[] = "ddb1da55-d478-4243-8642-56d3041f0263";

// In order to be compatible with sync folder id must match standard.
// Generated using crx_file::id_util::GenerateId("LinuxAppsFolder")
const char kCrostiniFolderId[] = "ddolnhmblagmcagkedkbfejapapdimlk";

////////////////////////////////////////////////////////////////////////////////
// AppListItemMetadata:

AppListItemMetadata::AppListItemMetadata() = default;
AppListItemMetadata::AppListItemMetadata(const AppListItemMetadata& rhs) =
    default;
AppListItemMetadata::~AppListItemMetadata() = default;

// TODO: This method could be eliminated, by passing the action with result
// action metadata instead of implicitly relying on order in which actions are
// listed in SearchResult::actions().
SearchResultActionType GetSearchResultActionType(int button_index) {
  if (button_index < 0 ||
      button_index >= static_cast<int>(
                          SearchResultActionType::kSearchResultActionTypeMax)) {
    return SearchResultActionType::kSearchResultActionTypeMax;
  }

  return static_cast<SearchResultActionType>(button_index);
}

////////////////////////////////////////////////////////////////////////////////
// SearchResultIconInfo:

SearchResultIconInfo::SearchResultIconInfo() = default;

SearchResultIconInfo::SearchResultIconInfo(gfx::ImageSkia icon) : icon(icon) {}

SearchResultIconInfo::SearchResultIconInfo(gfx::ImageSkia icon, int dimension)
    : icon(icon), dimension(dimension) {}

SearchResultIconInfo::SearchResultIconInfo(gfx::ImageSkia icon,
                                           int dimension,
                                           SearchResultIconShape shape)
    : icon(icon), dimension(dimension), shape(shape) {}

SearchResultIconInfo::SearchResultIconInfo(const SearchResultIconInfo& other)
    : icon(other.icon), dimension(other.dimension), shape(other.shape) {}

SearchResultIconInfo::~SearchResultIconInfo() = default;

////////////////////////////////////////////////////////////////////////////////
// SearchResultTag:

SearchResultTag::SearchResultTag() = default;

SearchResultTag::SearchResultTag(int styles, uint32_t start, uint32_t end)
    : styles(styles), range(start, end) {}

////////////////////////////////////////////////////////////////////////////////
// SearchResultAction:

SearchResultAction::SearchResultAction() {}

SearchResultAction::SearchResultAction(const gfx::ImageSkia& image,
                                       const std::u16string& tooltip_text,
                                       bool visible_on_hover)
    : image(image),
      tooltip_text(tooltip_text),
      visible_on_hover(visible_on_hover) {}

SearchResultAction::SearchResultAction(const SearchResultAction& other) =
    default;

SearchResultAction::~SearchResultAction() = default;

////////////////////////////////////////////////////////////////////////////////
// SearchResultMetadata:

SearchResultMetadata::SearchResultMetadata() = default;
SearchResultMetadata::SearchResultMetadata(const SearchResultMetadata& rhs) =
    default;
SearchResultMetadata::~SearchResultMetadata() = default;

}  // namespace ash
