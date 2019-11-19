// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_types.h"

namespace ash {

const char kOemFolderId[] = "ddb1da55-d478-4243-8642-56d3041f0263";

////////////////////////////////////////////////////////////////////////////////
// AppListItemMetadata:

AppListItemMetadata::AppListItemMetadata() = default;
AppListItemMetadata::AppListItemMetadata(const AppListItemMetadata& rhs) =
    default;
AppListItemMetadata::~AppListItemMetadata() = default;

OmniBoxZeroStateAction GetOmniBoxZeroStateAction(int button_index) {
  if (button_index < 0 ||
      button_index >=
          static_cast<int>(ash::OmniBoxZeroStateAction::kZeroStateActionMax)) {
    return ash::OmniBoxZeroStateAction::kZeroStateActionMax;
  }

  return static_cast<ash::OmniBoxZeroStateAction>(button_index);
}

////////////////////////////////////////////////////////////////////////////////
// SearchResultTag:

SearchResultTag::SearchResultTag() = default;

SearchResultTag::SearchResultTag(int styles, uint32_t start, uint32_t end)
    : styles(styles), range(start, end) {}

////////////////////////////////////////////////////////////////////////////////
// SearchResultAction:

SearchResultAction::SearchResultAction() {}

SearchResultAction::SearchResultAction(const gfx::ImageSkia& image,
                                       const base::string16& tooltip_text,
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
