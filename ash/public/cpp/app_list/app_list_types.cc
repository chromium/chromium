// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_types.h"

#include <string>
#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "base/check.h"
#include "base/files/file.h"
#include "base/functional/callback.h"
#include "build/branding_buildflags.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/icons/vector_icons.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {

namespace {

// Search control dictionary pref keys.
const char kLauncherAppSearchEnabled[] = "app_search_enabled";
const char kLauncherAppShortcutSearchEnabled[] = "app_shortcut_search_enabled";
const char kLauncherWebSearchEnabled[] = "web_search_enabled";
const char kLauncherFileSearchEnabled[] = "file_search_enabled";
const char kLauncherHelpSearchEnabled[] = "help_search_enabled";
const char kLauncherPlayStoreSearchEnabled[] = "play_store_search_enabled";
const char kLauncherGameSearchEnabled[] = "game_search_enabled";
const char kLauncherImageSearchEnabled[] = "image_search_enabled";

}  // namespace

const char kOemFolderId[] = "ddb1da55-d478-4243-8642-56d3041f0263";

// In order to be compatible with sync folder id must match standard.
// Generated using crx_file::id_util::GenerateId("LinuxAppsFolder")
const char kCrostiniFolderId[] = "ddolnhmblagmcagkedkbfejapapdimlk";

// Generated using crx_file::id_util::GenerateId("BruschettaAppsFolder")
const char kBruschettaFolderId[] = "olojmkekngdacpmgcffeipkflkgohcja";

bool IsAppListSearchResultAnApp(AppListSearchResultType result_type) {
  switch (result_type) {
    case AppListSearchResultType::kInstalledApp:
    case AppListSearchResultType::kInternalApp:
    case AppListSearchResultType::kPlayStoreApp:
    case AppListSearchResultType::kPlayStoreReinstallApp:
    case AppListSearchResultType::kArcAppShortcut:
    case AppListSearchResultType::kInstantApp:
    case AppListSearchResultType::kGames:
    case AppListSearchResultType::kZeroStateApp:
    case AppListSearchResultType::kAppShortcutV2:
      return true;
    case AppListSearchResultType::kUnknown:
    case AppListSearchResultType::kOmnibox:
    case AppListSearchResultType::kLauncher:
    case AppListSearchResultType::kAnswerCard:
    case AppListSearchResultType::kZeroStateFile:
    case AppListSearchResultType::kZeroStateDrive:
    case AppListSearchResultType::kOsSettings:
    case AppListSearchResultType::kInternalPrivacyInfo:
    case AppListSearchResultType::kAssistantText:
    case AppListSearchResultType::kHelpApp:
    case AppListSearchResultType::kZeroStateHelpApp:
    case AppListSearchResultType::kFileSearch:
    case AppListSearchResultType::kDriveSearch:
    case AppListSearchResultType::kKeyboardShortcut:
    case AppListSearchResultType::kOpenTab:
    case AppListSearchResultType::kPersonalization:
    case AppListSearchResultType::kImageSearch:
    case AppListSearchResultType::kSystemInfo:
    case AppListSearchResultType::kDesksAdminTemplate:
      return false;
  }
}

bool IsZeroStateResultType(AppListSearchResultType result_type) {
  switch (result_type) {
    case AppListSearchResultType::kZeroStateFile:
    case AppListSearchResultType::kZeroStateDrive:
    case AppListSearchResultType::kZeroStateHelpApp:
    case AppListSearchResultType::kZeroStateApp:
    case AppListSearchResultType::kDesksAdminTemplate:
      return true;
    case AppListSearchResultType::kUnknown:
    case AppListSearchResultType::kInstalledApp:
    case AppListSearchResultType::kPlayStoreApp:
    case AppListSearchResultType::kInstantApp:
    case AppListSearchResultType::kInternalApp:
    case AppListSearchResultType::kOmnibox:
    case AppListSearchResultType::kLauncher:
    case AppListSearchResultType::kAnswerCard:
    case AppListSearchResultType::kPlayStoreReinstallApp:
    case AppListSearchResultType::kArcAppShortcut:
    case AppListSearchResultType::kOsSettings:
    case AppListSearchResultType::kInternalPrivacyInfo:
    case AppListSearchResultType::kAssistantText:
    case AppListSearchResultType::kHelpApp:
    case AppListSearchResultType::kFileSearch:
    case AppListSearchResultType::kDriveSearch:
    case AppListSearchResultType::kKeyboardShortcut:
    case AppListSearchResultType::kOpenTab:
    case AppListSearchResultType::kGames:
    case AppListSearchResultType::kPersonalization:
    case AppListSearchResultType::kImageSearch:
    case AppListSearchResultType::kSystemInfo:
    case AppListSearchResultType::kAppShortcutV2:
      return false;
  }
}

std::string GetAppListControlCategoryName(
    AppListSearchControlCategory control_category) {
  switch (control_category) {
    // Non-toggleable category does not have a pref name is always enabled.
    case AppListSearchControlCategory::kCannotToggle:
      return std::string();
    case AppListSearchControlCategory::kApps:
      return kLauncherAppSearchEnabled;
    case AppListSearchControlCategory::kAppShortcuts:
      return kLauncherAppShortcutSearchEnabled;
    case AppListSearchControlCategory::kWeb:
      return kLauncherWebSearchEnabled;
    case AppListSearchControlCategory::kFiles:
      return kLauncherFileSearchEnabled;
    case ash::AppListSearchControlCategory::kHelp:
      return kLauncherHelpSearchEnabled;
    case ash::AppListSearchControlCategory::kPlayStore:
      return kLauncherPlayStoreSearchEnabled;
    case ash::AppListSearchControlCategory::kGames:
      return kLauncherGameSearchEnabled;
    case ash::AppListSearchControlCategory::kImages:
      return kLauncherImageSearchEnabled;
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

std::ostream& operator<<(std::ostream& os, AppListState state) {
  switch (state) {
    case AppListState::kStateApps:
      return os << "StateApps";
    case AppListState::kStateSearchResults:
      return os << "SearchResults";
    case AppListState::kStateStart_DEPRECATED:
      return os << "Start_DEPRECATED";
    case AppListState::kStateEmbeddedAssistant:
      return os << "EmbeddedAssistant";
    case AppListState::kInvalidState:
      return os << "InvalidState";
  }
}

std::ostream& operator<<(std::ostream& os, AppListBubblePage page) {
  switch (page) {
    case AppListBubblePage::kNone:
      return os << "None";
    case AppListBubblePage::kApps:
      return os << "Apps";
    case AppListBubblePage::kAppsCollections:
      return os << "AppsCollections";
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
    case AppListViewState::kFullscreenAllApps:
      return os << "FullscreenAllApps";
    case AppListViewState::kFullscreenSearch:
      return os << "FullscreenSearch";
  }
}

////////////////////////////////////////////////////////////////////////////////
// SearchResultIconInfo:

SearchResultIconInfo::SearchResultIconInfo() = default;

SearchResultIconInfo::SearchResultIconInfo(ui::ImageModel icon, int dimension)
    : icon(icon), dimension(dimension) {}

SearchResultIconInfo::SearchResultIconInfo(ui::ImageModel icon,
                                           int dimension,
                                           SearchResultIconShape shape,
                                           bool is_placeholder)
    : icon(icon),
      dimension(dimension),
      shape(shape),
      is_placeholder(is_placeholder) {}

SearchResultIconInfo::SearchResultIconInfo(const SearchResultIconInfo& other)
    : icon(other.icon),
      dimension(other.dimension),
      shape(other.shape),
      is_placeholder(other.is_placeholder) {}

SearchResultIconInfo::~SearchResultIconInfo() = default;

////////////////////////////////////////////////////////////////////////////////
// SearchResultSystemInfoAnswerCardInfo:

SystemInfoAnswerCardData::SystemInfoAnswerCardData() = default;

SystemInfoAnswerCardData::SystemInfoAnswerCardData(
    SystemInfoAnswerCardDisplayType display_type)
    : display_type(display_type) {}

SystemInfoAnswerCardData::SystemInfoAnswerCardData(double bar_chart_percentage)
    : display_type(SystemInfoAnswerCardDisplayType::kBarChart),
      bar_chart_percentage(bar_chart_percentage) {}

SystemInfoAnswerCardData::~SystemInfoAnswerCardData() = default;

SystemInfoAnswerCardData::SystemInfoAnswerCardData(
    const SystemInfoAnswerCardData& other) = default;

void SystemInfoAnswerCardData::SetExtraDetails(
    const std::u16string& description_on_right) {
  extra_details = description_on_right;
}

void SystemInfoAnswerCardData::SetUpperLimitForBarChart(double upper_limit) {
  DCHECK(upper_limit <= 100 && upper_limit >= 0);
  upper_warning_limit_bar_chart = upper_limit;
}
void SystemInfoAnswerCardData::SetLowerLimitForBarChart(double lower_limit) {
  DCHECK(lower_limit <= 100 && lower_limit >= 0);
  lower_warning_limit_bar_chart = lower_limit;
}

void SystemInfoAnswerCardData::UpdateBarChartPercentage(
    double new_bar_chart_percentage) {
  DCHECK(new_bar_chart_percentage <= 100 && new_bar_chart_percentage >= 0);
  bar_chart_percentage = new_bar_chart_percentage;
}

////////////////////////////////////////////////////////////////////////////////
// FileMetadataLoader:

FileMetadataLoader::FileMetadataLoader() = default;
FileMetadataLoader::FileMetadataLoader(const FileMetadataLoader&) = default;
FileMetadataLoader& FileMetadataLoader::operator=(const FileMetadataLoader&) =
    default;
FileMetadataLoader::~FileMetadataLoader() = default;

void FileMetadataLoader::RequestFileInfo(
    OnMetadataLoadedCallback on_loaded_callback) {
  // Return an empty base::File::Info if the loader callback is not set.
  if (loader_callback_.is_null()) {
    std::move(on_loaded_callback).Run(base::File::Info());
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::OnceCallback<base::File::Info()>(loader_callback_),
      std::move(on_loaded_callback));
}

void FileMetadataLoader::SetLoaderCallback(MetadataLoaderCallback callback) {
  loader_callback_ = std::move(callback);
}

////////////////////////////////////////////////////////////////////////////////
// SearchResultTag:

SearchResultTag::SearchResultTag() = default;

SearchResultTag::SearchResultTag(int styles, uint32_t start, uint32_t end)
    : styles(styles), range(start, end) {}

////////////////////////////////////////////////////////////////////////////////
// SearchResultAction:

SearchResultAction::SearchResultAction() = default;

SearchResultAction::SearchResultAction(SearchResultActionType type,
                                       const std::u16string& tooltip_text)
    : type(type), tooltip_text(tooltip_text) {}

SearchResultAction::SearchResultAction(const SearchResultAction& other) =
    default;

SearchResultAction::~SearchResultAction() = default;

////////////////////////////////////////////////////////////////////////////////
// SearchResultTextItem:

SearchResultTextItem::SearchResultTextItem(SearchResultTextItemType type) {
  item_type_ = type;
}

SearchResultTextItem::SearchResultTextItem(const SearchResultTextItem& other) =
    default;

SearchResultTextItem& SearchResultTextItem::operator=(
    const SearchResultTextItem& other) = default;

SearchResultTextItem::~SearchResultTextItem() = default;

SearchResultTextItemType SearchResultTextItem::GetType() const {
  return item_type_;
}

const std::u16string& SearchResultTextItem::GetText() const {
  DCHECK(item_type_ == SearchResultTextItemType::kString ||
         item_type_ == SearchResultTextItemType::kIconifiedText);
  return raw_text_.value();
}

SearchResultTextItem& SearchResultTextItem::SetText(std::u16string text) {
  DCHECK(item_type_ == SearchResultTextItemType::kString ||
         item_type_ == SearchResultTextItemType::kIconifiedText);
  raw_text_ = text;
  return *this;
}

const SearchResultTags& SearchResultTextItem::GetTextTags() const {
  DCHECK(item_type_ == SearchResultTextItemType::kString ||
         item_type_ == SearchResultTextItemType::kIconifiedText);
  return text_tags_.value();
}

SearchResultTags& SearchResultTextItem::GetTextTags() {
  DCHECK(item_type_ == SearchResultTextItemType::kString ||
         item_type_ == SearchResultTextItemType::kIconifiedText);
  return text_tags_.value();
}

SearchResultTextItem& SearchResultTextItem::SetTextTags(SearchResultTags tags) {
  DCHECK(item_type_ == SearchResultTextItemType::kString ||
         item_type_ == SearchResultTextItemType::kIconifiedText);
  text_tags_ = tags;
  return *this;
}

const gfx::VectorIcon* SearchResultTextItem::GetIconFromCode() const {
  DCHECK_EQ(item_type_, SearchResultTextItemType::kIconCode);
  DCHECK(icon_code_.has_value());
  switch (icon_code_.value()) {
    // Browser.
    case kKeyboardShortcutBrowserBack:
      return &kKsvBrowserBackIcon;
    case kKeyboardShortcutBrowserForward:
      return &kKsvBrowserForwardIcon;
    case kKeyboardShortcutBrowserHome:
      return &kKsvBrowserHomeIcon;
    case kKeyboardShortcutBrowserRefresh:
      return &kKsvReloadIcon;
    case kKeyboardShortcutBrowserSearch:
      return &kKsBrowserSearchIcon;
    // Emoji picker.
    case kKeyboardShortcutEmojiPicker:
      return &kKsEmojiPickerIcon;
    // Dictation.
    case kKeyboardShortcutDictationToggle:
      return &kKsDictationIcon;
    // Zoom.
    case kKeyboardShortcutZoom:
      return &kKsvFullscreenIcon;
    // Media.
    case kKeyboardShortcutMediaLaunchApp1:
      return &kKsvOverviewIcon;
    case kKeyboardShortcutMediaLaunchApp1Refresh:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return &kOverviewRefreshIcon;
#else
      return &kKsvOverviewIcon;
#endif
    case kKeyboardShortcutMediaFastForward:
      return &kKsMediaFastForwardIcon;
    case kKeyboardShortcutMediaLaunchMail:
      return &kKsMediaLaunchMailIcon;
    case kKeyboardShortcutMediaPause:
      return &kKsMediaPauseIcon;
    case kKeyboardShortcutMediaPlay:
      return &kKsMediaPlayIcon;
    case kKeyboardShortcutMediaPlayPause:
      return &kKsMediaPlayPauseIcon;
    case kKeyboardShortcutMediaTrackNext:
      return &kKsMediaTrackNextIcon;
    case kKeyboardShortcutMediaTrackPrevious:
      return &kKsMediaTrackPreviousIcon;
    // Brightness.
    case kKeyboardShortcutBrightnessDown:
      return &kKsvBrightnessDownIcon;
    case kKeyboardShortcutBrightnessUp:
      return &kKsvBrightnessUpIcon;
    case kKeyboardShortcutBrightnessUpRefresh:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return &kBrightnessUpRefreshIcon;
#else
      return &kKsvBrightnessUpIcon;
#endif
    // Volume.
    case kKeyboardShortcutVolumeMute:
      return &kKsvMuteIcon;
    case kKeyboardShortcutVolumeDown:
      return &kKsvVolumeDownIcon;
    case kKeyboardShortcutVolumeUp:
      return &kKsvVolumeUpIcon;
    // Arrows.
    case kKeyboardShortcutUp:
      return &kKsvArrowUpIcon;
    case kKeyboardShortcutDown:
      return &kKsvArrowDownIcon;
    case kKeyboardShortcutLeft:
      return &kKsvArrowLeftIcon;
    case kKeyboardShortcutRight:
      return &kKsvArrowRightIcon;
    // Privacy.
    case kKeyboardShortcutPrivacyScreenToggle:
      return &kKsvPrivacyScreenToggleIcon;
    // Settings.
    case kKeyboardShortcutSettings:
      return &kKsSettingsIcon;
    // Snapshot.
    case kKeyboardShortcutSnapshot:
      return &kKsvSnapshotIcon;
    // Launcher.
    case kKeyboardShortcutLauncher:
      return &kKsLauncherIcon;
    case kKeyboardShortcutLauncherRefresh:
      return &kCampbellHeroIcon;
    // Search.
    case kKeyboardShortcutSearch:
      return &kKsSearchIcon;
    // Apps.
    case kKeyboardShortcutAssistant:
      return &kKsAssistantIcon;
    case kKeyboardShortcutAllApps:
      return &kKsAllAppsIcon;
    case kKeyboardShortcutCalculator:
      return &kKsCalculatorIcon;
    case kKeyboardShortcutInputModeChange:
      return &kKsInputModeChangeIcon;
    case kKeyboardShortcutMicrophone:
      return &kKsMicrophoneIcon;
    // Power.
    case kKeyboardShortcutPower:
      return &kKsPowerIcon;
    // Keyboard brightness.
    case kKeyboardShortcutKeyboardBrightnessDown:
      return &kKsKeyboardBrightnessDownIcon;
    case kKeyboardShortcutKeyboardBrightnessUp:
      return &kKsKeyboardBrightnessUpIcon;
    case kKeyboardShortcutKeyboardBacklightToggle:
      return &kKsKeyboardBrightnessToggleIcon;
    // Accessibility.
    case kKeyboardShortcutAccessibility:
      return &kKsAccessibilityIcon;
    // Context menu.
    case kKeyboardShortcutContextMenu:
      return &kKsContextMenuIcon;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case kKeyboardShortcutKeyboardRightAlt:
      return &kRightAltInternalIcon;
#endif
    default:
      return nullptr;
  }
}

SearchResultTextItem& SearchResultTextItem::SetIconCode(IconCode code) {
  DCHECK_EQ(item_type_, SearchResultTextItemType::kIconCode);
  icon_code_ = code;
  return *this;
}

gfx::ImageSkia SearchResultTextItem::GetImage() const {
  DCHECK_EQ(item_type_, SearchResultTextItemType::kCustomImage);
  return raw_image_.value();
}

SearchResultTextItem& SearchResultTextItem::SetImage(gfx::ImageSkia icon) {
  DCHECK_EQ(item_type_, SearchResultTextItemType::kCustomImage);
  raw_image_ = icon;
  return *this;
}

SearchResultTextItem::OverflowBehavior
SearchResultTextItem::GetOverflowBehavior() const {
  DCHECK_EQ(item_type_, SearchResultTextItemType::kString);
  return overflow_behavior_;
}

SearchResultTextItem& SearchResultTextItem::SetOverflowBehavior(
    SearchResultTextItem::OverflowBehavior overflow_behavior) {
  DCHECK_EQ(item_type_, SearchResultTextItemType::kString);
  overflow_behavior_ = overflow_behavior;
  return *this;
}

bool SearchResultTextItem::GetAlternateIconAndTextStyling() const {
  CHECK(item_type_ == SearchResultTextItemType::kIconifiedText ||
        item_type_ == SearchResultTextItemType::kIconCode);
  return alternate_icon_text_code_styling_;
}

SearchResultTextItem& SearchResultTextItem::SetAlternateIconAndTextStyling(
    bool alternate_icon_text_code_styling) {
  CHECK(item_type_ == SearchResultTextItemType::kIconifiedText ||
        item_type_ == SearchResultTextItemType::kIconCode);
  alternate_icon_text_code_styling_ = alternate_icon_text_code_styling;
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
// SearchResultMetadata:

SearchResultMetadata::SearchResultMetadata() = default;
SearchResultMetadata::SearchResultMetadata(const SearchResultMetadata& rhs) =
    default;
SearchResultMetadata::~SearchResultMetadata() = default;

}  // namespace ash
