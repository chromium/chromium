// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_types.h"

namespace ash {

const char kOemFolderId[] = "ddb1da55-d478-4243-8642-56d3041f0263";

// In order to be compatible with sync folder id must match standard.
// Generated using crx_file::id_util::GenerateId("LinuxAppsFolder")
const char kCrostiniFolderId[] = "ddolnhmblagmcagkedkbfejapapdimlk";

bool IsAppListSearchResultAnApp(AppListSearchResultType result_type) {
  switch (result_type) {
    case AppListSearchResultType::kInstalledApp:
    case AppListSearchResultType::kInternalApp:
    case AppListSearchResultType::kPlayStoreApp:
    case AppListSearchResultType::kPlayStoreReinstallApp:
    case AppListSearchResultType::kArcAppShortcut:
    case AppListSearchResultType::kInstantApp:
      return true;
    case AppListSearchResultType::kUnknown:
    case AppListSearchResultType::kOmnibox:
    case AppListSearchResultType::kLauncher:
    case AppListSearchResultType::kAnswerCard:
    case AppListSearchResultType::kZeroStateFile:
    case AppListSearchResultType::kZeroStateDrive:
    case AppListSearchResultType::kFileChip:
    case AppListSearchResultType::kDriveChip:
    case AppListSearchResultType::kAssistantChip:
    case AppListSearchResultType::kOsSettings:
    case AppListSearchResultType::kInternalPrivacyInfo:
    case AppListSearchResultType::kAssistantText:
    case AppListSearchResultType::kHelpApp:
    case AppListSearchResultType::kFileSearch:
    case AppListSearchResultType::kDriveSearch:
      return false;
  }
}

// IconColor -------------------------------------------------------------------

// static
constexpr int IconColor::kHueInvalid;
constexpr int IconColor::kHueMin;
constexpr int IconColor::kHueMax;

IconColor::IconColor() = default;

IconColor::IconColor(sync_pb::AppListSpecifics::ColorGroup background_color,
                     int hue)
    : background_color_(background_color), hue_(hue) {}

IconColor::IconColor(const IconColor& rhs)
    : background_color_(rhs.background_color()), hue_(rhs.hue()) {}

IconColor& IconColor::operator=(const IconColor& rhs) = default;

IconColor::~IconColor() = default;

bool IconColor::operator<(const IconColor& rhs) const {
  // TODO(crbug.com/1270898): Add DCHECKs for checking IsValid() and
  // rhs.IsValid(). Investigate and fix the case where IconColors are invalid.
  // In the meantime invalid IconColors can still be sorted against other
  // IconColors and are ordered to come before other icons in this case.

  // Compare background colors first.
  if (background_color_ != rhs.background_color())
    return background_color_ < rhs.background_color();

  return hue_ < rhs.hue();
}

bool IconColor::operator>(const IconColor& rhs) const {
  // TODO(crbug.com/1270898): Investigate and add back DCHECKS for IsValid() and
  // rhs.IsValid().

  // Compare background colors first.
  if (background_color_ != rhs.background_color())
    return background_color_ > rhs.background_color();

  return hue_ > rhs.hue();
}

bool IconColor::operator>=(const IconColor& rhs) const {
  return !(*this < rhs);
}

bool IconColor::operator<=(const IconColor& rhs) const {
  return !(*this > rhs);
}

bool IconColor::operator==(const IconColor& rhs) const {
  return !(*this != rhs);
}

bool IconColor::operator!=(const IconColor& rhs) const {
  // TODO(crbug.com/1270898): Investigate and add back DCHECKS for IsValid() and
  // rhs.IsValid().

  return *this < rhs || *this > rhs;
}

bool IconColor::IsValid() const {
  const bool is_hue_valid = (hue_ >= kHueMin && hue_ <= kHueMax);
  return background_color_ != sync_pb::AppListSpecifics::COLOR_EMPTY &&
         is_hue_valid;
}

////////////////////////////////////////////////////////////////////////////////
// AppListItemMetadata:

AppListItemMetadata::AppListItemMetadata() = default;
AppListItemMetadata::AppListItemMetadata(const AppListItemMetadata& rhs) =
    default;
AppListItemMetadata::~AppListItemMetadata() = default;

////////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, AppListBubblePage page) {
  switch (page) {
    case AppListBubblePage::kNone:
      return os << "None";
    case AppListBubblePage::kApps:
      return os << "Apps";
    case AppListBubblePage::kSearch:
      return os << "Search";
    case AppListBubblePage::kAssistant:
      return os << "Assistant";
  }
}

std::ostream& operator<<(std::ostream& os, AppListViewState state) {
  switch (state) {
    case AppListViewState::kClosed:
      return os << "Closed";
    case AppListViewState::kPeeking:
      return os << "Peeking";
    case AppListViewState::kHalf:
      return os << "Half";
    case AppListViewState::kFullscreenAllApps:
      return os << "FullscreenAllApps";
    case AppListViewState::kFullscreenSearch:
      return os << "FullscreenSearch";
  }
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

SearchResultAction::SearchResultAction() = default;

SearchResultAction::SearchResultAction(SearchResultActionType type,
                                       const gfx::ImageSkia& image,
                                       const std::u16string& tooltip_text,
                                       bool visible_on_hover)
    : type(type),
      image(image),
      tooltip_text(tooltip_text),
      visible_on_hover(visible_on_hover) {}

SearchResultAction::SearchResultAction(const SearchResultAction& other) =
    default;

SearchResultAction::~SearchResultAction() = default;

////////////////////////////////////////////////////////////////////////////////
// SearchResultTextItem:

SearchResultTextItem::SearchResultTextItem(SearchResultTextItemType type) {
  item_type = type;
}

SearchResultTextItem::SearchResultTextItem(const SearchResultTextItem& other) {
  item_type = other.item_type;
  raw_text = other.raw_text;
  text_tags = other.text_tags;
  icon_code = other.icon_code;
  raw_icon = other.raw_icon;
}

SearchResultTextItem& SearchResultTextItem::operator=(
    const SearchResultTextItem& other) {
  item_type = other.item_type;
  raw_text = other.raw_text;
  text_tags = other.text_tags;
  icon_code = other.icon_code;
  raw_icon = other.raw_icon;
  return *this;
}

SearchResultTextItem::~SearchResultTextItem() = default;

SearchResultTextItemType SearchResultTextItem::GetType() const {
  return item_type;
}

const std::u16string& SearchResultTextItem::GetText() const {
  DCHECK_EQ(item_type, SearchResultTextItemType::kString);
  return raw_text.value();
}

SearchResultTextItem& SearchResultTextItem::SetText(std::u16string text) {
  DCHECK_EQ(item_type, SearchResultTextItemType::kString);
  raw_text = text;
  return *this;
}

const SearchResultTags& SearchResultTextItem::GetTextTags() const {
  DCHECK_EQ(item_type, SearchResultTextItemType::kString);
  return text_tags.value();
}

SearchResultTextItem& SearchResultTextItem::SetTextTags(SearchResultTags tags) {
  DCHECK_EQ(item_type, SearchResultTextItemType::kString);
  text_tags = tags;
  return *this;
}

gfx::ImageSkia SearchResultTextItem::GetIconFromCode() const {
  DCHECK_EQ(item_type, SearchResultTextItemType::kIconCode);
  return gfx::ImageSkia();
}

SearchResultTextItem& SearchResultTextItem::SetIconCode(int code) {
  DCHECK_EQ(item_type, SearchResultTextItemType::kIconCode);
  icon_code = code;
  return *this;
}

gfx::ImageSkia SearchResultTextItem::GetIcon() const {
  DCHECK_EQ(item_type, SearchResultTextItemType::kCustomIcon);
  return raw_icon.value();
}

SearchResultTextItem& SearchResultTextItem::SetIcon(gfx::ImageSkia icon) {
  DCHECK_EQ(item_type, SearchResultTextItemType::kCustomIcon);
  raw_icon = icon;
  return *this;
}
////////////////////////////////////////////////////////////////////////////////
// SearchResultMetadata:

SearchResultMetadata::SearchResultMetadata() = default;
SearchResultMetadata::SearchResultMetadata(const SearchResultMetadata& rhs) =
    default;
SearchResultMetadata::~SearchResultMetadata() = default;

}  // namespace ash
