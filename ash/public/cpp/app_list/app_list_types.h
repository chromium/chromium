// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_TYPES_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_TYPES_H_

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/protocol/app_list_specifics.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace ash {

// The initial value of |profile_id_| in AppListControllerImpl.
constexpr int kAppListInvalidProfileID = -1;

// The value from which the unique profile id starts. Notice that this profile
// id is only used for mojo callings between AppListController and AppListClient
constexpr int kAppListProfileIdStartFrom = 0;

// The threshold of mouse drag event. The mouse event is treated as tapping if
// drag offset is smaller than the threshold.
constexpr int kMouseDragThreshold = 2;

// Id of OEM folder in app list.
ASH_PUBLIC_EXPORT extern const char kOemFolderId[];

// The AppListItem ID of the "Linux apps" folder.
ASH_PUBLIC_EXPORT extern const char kCrostiniFolderId[];

// App list config types supported by AppListConfig.
enum class AppListConfigType {
  // Legacy configs, chosen based on the size of the screen.
  // Used when ProductivityLauncher is disabled.
  kLarge,
  kMedium,
  kSmall,

  // Config for tablet mode on typical size screens.
  // Used when ProductivityLauncher is enabled.
  kRegular,

  // Config for clamshell mode. Also used for tablet mode on small screens.
  // Used when ProductivityLauncher is enabled.
  kDense,
};

// A structure holding an item icon' color information.
class ASH_PUBLIC_EXPORT IconColor {
 public:
  // The minimum value of a valid hue. It is also the hue for the color that is
  // close or equal to pure white.
  static constexpr int kHueMin = -1;

  // The maximum value of a valid hue. It is also the hue for the color that is
  // close or equal to pure black.
  static constexpr int kHueMax = 360;

  static constexpr int kHueInvalid = kHueMin - 1;

  IconColor();
  IconColor(sync_pb::AppListSpecifics::ColorGroup background_color, int hue);
  IconColor(const IconColor&);
  IconColor& operator=(const IconColor&);
  ~IconColor();

  // The overloaded rational operators. NOTE: these operators assume that
  // operands are valid.
  bool operator<(const IconColor&) const;
  bool operator>(const IconColor&) const;
  bool operator>=(const IconColor&) const;
  bool operator<=(const IconColor&) const;
  bool operator==(const IconColor&) const;
  bool operator!=(const IconColor&) const;

  // Returns true only when all data members are valid: `background_color_` is
  // non-empty and `hue_` is in the valid range.
  bool IsValid() const;

  sync_pb::AppListSpecifics::ColorGroup background_color() const {
    return background_color_;
  }
  int hue() const { return hue_; }

 private:
  // Indicates an icon's background color.
  sync_pb::AppListSpecifics::ColorGroup background_color_ =
      sync_pb::AppListSpecifics::COLOR_EMPTY;

  // Indicates an icon's hue. NOTE: `hue_` falls in the range of [-1,360] that
  // is different from the normal range which is [0, 360). See the comments for
  // `kHueMin` and `kHueMax`.
  int hue_ = kHueInvalid;
};

// A structure holding the common information which is sent between ash and,
// chrome representing an app list item.
struct ASH_PUBLIC_EXPORT AppListItemMetadata {
  AppListItemMetadata();
  AppListItemMetadata(const AppListItemMetadata& rhs);
  ~AppListItemMetadata();

  std::string id;    // Id of the app list item.
  std::string name;  // Corresponding app/folder's name of the item.

  AppStatus app_status = AppStatus::kReady;  // App status.

  std::string folder_id;           // Id of folder where the item resides.
  syncer::StringOrdinal position;  // Position of the item.
  bool is_folder = false;          // Whether this item is a folder.
  bool is_persistent = false;  // Whether this folder is allowed to contain only
                               // 1 item.
  gfx::ImageSkia icon;         // The icon of this item.
  bool is_page_break = false;  // Whether this item is a "page break" item.
  SkColor badge_color = SK_ColorWHITE;  // Notification badge color.

  // Whether the app was installed this session and has not yet been launched.
  bool is_new_install = false;

  int icon_version = 0;  // An int represent icon version. If changed, `icon`
                         // should be reloaded.

  // The item's icon color.
  IconColor icon_color;
};

// All possible orders to sort app list items.
enum class AppListSortOrder {
  // The sort order is not set.
  kCustom = 0,

  // Items are sorted by the name alphabetical order. Note that folders are
  // always placed in front of other types of items.
  kNameAlphabetical,

  // Items are sorted by the name reverse alphabetical order. Note that folders
  // are always placed in front of other types of items.
  kNameReverseAlphabetical,

  // Items are sorted in order of color in rainbow order from red to purple.
  // Items are first sorted by the color of the icon background, then sorted
  // by the light vibrant color extracted from the icon.
  kColor
};

// Lists the reasons that ash requests for item position update.
enum class RequestPositionUpdateReason {
  // Fix the position when multiple items share the same position.
  kFixItem,

  // Move an item.
  kMoveItem
};

// Lists the reasons that ash requests to move an item into a folder.
enum class RequestMoveToFolderReason {
  // Merge two items and move the first item to the created folder.
  kMergeFirstItem,

  // Merge two items and move the second item to the created folder.
  kMergeSecondItem,

  // Move an item to an existed folder.
  kMoveItem
};

// All possible states of the app list.
// Note: Do not change the order of these as they are used for metrics.
enum class AppListState {
  kStateApps = 0,
  kStateSearchResults,
  kStateStart_DEPRECATED,
  kStateEmbeddedAssistant,
  // Add new values here.

  kInvalidState,               // Don't use over IPC
  kStateLast = kInvalidState,  // Don't use over IPC
};

// Sub-pages of the app list bubble (with ProductivityLauncher).
enum class AppListBubblePage { kApps, kSearch, kAssistant };

// All possible states of the app list view.
enum class AppListViewState {
  // Closes |app_list_main_view_| and dismisses the delegate.
  kClosed,
  // The initial state for the app list when neither maximize or side shelf
  // modes are active. If set, the widget will peek over the shelf by
  // kPeekingAppListHeight DIPs.
  kPeeking,
  // Entered when text is entered into the search box from peeking mode.
  kHalf,
  // Default app list state in maximize and side shelf modes. Entered from an
  // upward swipe from |PEEKING| or from clicking the chevron.
  kFullscreenAllApps,
  // Entered from an upward swipe from |HALF| or by entering text in the
  // search box from |FULLSCREEN_ALL_APPS|.
  kFullscreenSearch
};

// The status of the app list model.
enum class AppListModelStatus {
  kStatusNormal,
  kStatusSyncing,  // Syncing apps or installing synced apps.
};

// The UI component the user launched the search result from. Must match
// chrome/browser/ui/app_list/app_launch_event_logger.proto.
// This enum is used in a histogram, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// AppListLaunchedFrom enum listing in tools/metrics/histograms/enums.xml.
enum class AppListLaunchedFrom {
  kLaunchedFromGrid = 1,
  kLaunchedFromSuggestionChip = 2,
  kLaunchedFromShelf = 3,
  kLaunchedFromSearchBox = 4,
  kLaunchedFromRecentApps = 5,
  kLaunchedFromContinueTask = 6,
  kMaxValue = kLaunchedFromContinueTask,
};

// The UI representation of the app that's being launched. Currently all search
// results that are not apps (OminboxResult, LauncherSearcResult, etc.) are
// grouped into kSearchResult. Meanwhile app search results, apps that appear in
// the recent apps section, and suggested chips (if productivity launcher is
// disabled) are considered kAppSearchResult. kApp is used for apps launched
// from the apps grid.
enum class AppListLaunchType { kSearchResult, kAppSearchResult, kApp };

// Type of the search result, which is set in Chrome. These values are persisted
// to logs. Entries should not be renumbered and numeric values should never be
// reused.
//
// TODO(crbug.com/1258415): kFileChip and kDriveChip can be deprecated once the
// new launcher is launched.
enum class AppListSearchResultType {
  kUnknown,       // Unknown type. Don't use over IPC
  kInstalledApp,  // Installed apps.
  kPlayStoreApp,  // Installable apps from PlayStore.
  kInstantApp,    // Instant apps.
  kInternalApp,   // Chrome OS apps.
  kOmnibox,       // Results from Omnibox.
  kLauncher,      // Results from launcher search (currently only from Files).
  kAnswerCard,    // WebContents based answer card.
  kPlayStoreReinstallApp,  // Reinstall recommendations from PlayStore.
  kArcAppShortcut,         // ARC++ app shortcuts.
  kZeroStateFile,          // Zero state local file results.
  kZeroStateDrive,         // Drive QuickAccess results.
  kFileChip,               // Local file results in suggestion chips.
  kDriveChip,              // Drive file results in suggestion chips.
  kAssistantChip,          // Assistant results in suggestion chips.
  kOsSettings,             // OS settings results.
  kInternalPrivacyInfo,    // Result used internally by privacy notices.
  kAssistantText,          // Assistant text results.
  kHelpApp,                // Help App (aka Explore) results.
  kFileSearch,             // Local file search results.
  kDriveSearch,            // Drive file search results.
  // Add new values here.
  kMaxValue = kDriveSearch,
};

ASH_PUBLIC_EXPORT bool IsAppListSearchResultAnApp(
    AppListSearchResultType result_type);

// The different categories a search result can be part of. Every search result
// to be displayed in the search box should be associated with one category. It
// is an error for results displayed in the search box to have a kUnknown
// category, but results displayed in other views - eg. the Continue section -
// may use kUnknown.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AppListSearchResultCategory {
  kUnknown = 0,
  kApps = 1,
  kAppShortcuts = 2,
  kWeb = 3,
  kFiles = 4,
  kSettings = 5,
  kHelp = 6,
  kPlayStore = 7,
  kSearchAndAssistant = 8,
  kMaxValue = kSearchAndAssistant,
};

// Which UI container(s) the result should be displayed in.
// Do not change the order of these as they are used for metrics.
//
// TODO(https://crbug.com/1258415): kChip can be deprecated once
// ProductivityLauncher is launched.
enum SearchResultDisplayType {
  kNone = 0,
  kList = 1,  // Displays in search list
  kTile = 2,  // Displays in search tiles
  // kRecommendation = 3  // No longer used, split between kTile and kChip
  kAnswerCard = 4,  // Displays in answer cards
  kChip = 5,        // Displays in suggestion chips
  kContinue = 6,    // Displays in the Continue section
  kRecentApps = 7,  // Displays in recent apps row
  // Add new values here
  kLast,  // Don't use over IPC
};

// Which index in the UI container should the result be placed in.
enum SearchResultDisplayIndex {
  kFirstIndex,
  kSecondIndex,
  kThirdIndex,
  kFourthIndex,
  kFifthIndex,
  kSixthIndex,
  kUndefined,
};

// Actions for search results. These map to the buttons beside some search
// results, and do not include the launching of the result itself.
// TODO(crbug.com/1263751): Currently these are only relevant to omnibox
// results, but these are being generalized to other result types.
enum SearchResultActionType {
  // Removes the search result.
  kRemove = 0,
  // Appends the result to search box query.
  kAppend,
  // kSearchResultActionMax is always last.
  kSearchResultActionTypeMax
};

// The shape to mask a search result icon with.
enum class SearchResultIconShape {
  kDefault,
  kRectangle,
  kRoundedRectangle,
  kCircle,
};

struct ASH_PUBLIC_EXPORT SearchResultIconInfo {
  SearchResultIconInfo();
  // TODO(crbug.com/1232897): Make the search backend explicitly set dimension
  // and shape for all icons by removing the one- and two-argument versions of
  // the constructor.
  explicit SearchResultIconInfo(gfx::ImageSkia icon);
  SearchResultIconInfo(gfx::ImageSkia icon, int dimension);
  SearchResultIconInfo(gfx::ImageSkia icon,
                       int dimension,
                       SearchResultIconShape shape);

  SearchResultIconInfo(const SearchResultIconInfo&);

  ~SearchResultIconInfo();

  // The icon itself.
  gfx::ImageSkia icon;

  // The size to display the icon at, while preserving aspect ratio. Only
  // used for the results list view.
  absl::optional<int> dimension;

  // The shape to mask the icon with. Only used by the results list view.
  SearchResultIconShape shape = SearchResultIconShape::kDefault;
};

// A tagged range in search result text.
struct ASH_PUBLIC_EXPORT SearchResultTag {
  // Similar to ACMatchClassification::Style, the style values are not
  // mutually exclusive.
  enum Style {
    NONE = 0,
    URL = 1 << 0,
    MATCH = 1 << 1,
    DIM = 1 << 2,
  };

  SearchResultTag();
  SearchResultTag(int styles, uint32_t start, uint32_t end);

  int styles;
  gfx::Range range;
};
using SearchResultTags = std::vector<SearchResultTag>;

// Data representing an action that can be performed on this search result.
// An action could be represented as an icon set or as a blue button with
// a label. Icon set is chosen if label text is empty. Otherwise, a blue
// button with the label text will be used.
struct ASH_PUBLIC_EXPORT SearchResultAction {
  SearchResultAction();
  SearchResultAction(SearchResultActionType type,
                     const gfx::ImageSkia& image,
                     const std::u16string& tooltip_text,
                     bool visible_on_hover);
  SearchResultAction(const SearchResultAction& other);
  ~SearchResultAction();

  SearchResultActionType type;
  gfx::ImageSkia image;
  std::u16string tooltip_text;
  // Visible when button or its parent row in hover state.
  bool visible_on_hover;
};
using SearchResultActions = std::vector<SearchResultAction>;

// A structure holding the common information which is sent from chrome to ash,
// representing a search result.
struct ASH_PUBLIC_EXPORT SearchResultMetadata {
  SearchResultMetadata();
  SearchResultMetadata(const SearchResultMetadata& rhs);
  ~SearchResultMetadata();

  // The id of the result.
  std::string id;

  // The title of the result, e.g. an app's name, an autocomplete query, etc.
  std::u16string title;

  // A detail string of this result.
  std::u16string details;

  // An text to be announced by a screen reader app.
  std::u16string accessible_name;

  // How the title matches the query. See the SearchResultTag section for more
  // details.
  std::vector<SearchResultTag> title_tags;

  // How the details match the query. See the SearchResultTag section for more
  // details.
  std::vector<SearchResultTag> details_tags;

  // Actions that can be performed on this result. See the SearchResultAction
  // section for more details.
  std::vector<SearchResultAction> actions;

  // The average rating score of the app corresponding to this result, ranged
  // from 0 to 5. It's negative if there's no rating for the result.
  float rating = -1.0;

  // A formatted price string, e.g. "$7.09", "HK$3.94", etc.
  std::u16string formatted_price;

  // Which category a search result is contained in within the search box. May
  // be kUnknown for results displayed in non-search-box views, eg. the Continue
  // section.
  AppListSearchResultCategory category = AppListSearchResultCategory::kUnknown;

  // Whether this result is a top match and should be shown in the Top Matches
  // section instead of its category.
  bool best_match = false;

  // The type of this result.
  AppListSearchResultType result_type = AppListSearchResultType::kUnknown;

  // A search result type used for metrics.
  ash::SearchResultType metrics_type = ash::SEARCH_RESULT_TYPE_BOUNDARY;

  // Which UI container(s) the result should be displayed in.
  SearchResultDisplayType display_type = SearchResultDisplayType::kList;

  // Which index in the UI container should the result be placed in.
  SearchResultDisplayIndex display_index = SearchResultDisplayIndex::kUndefined;

  // A score to settle conflicts between two apps with the same requested
  // |display_index|.
  float position_priority = 0.0f;

  // A score to determine the result display order.
  double display_score = 0;

  // Whether this is searched from Omnibox.
  bool is_omnibox_search = false;

  // Whether this result is installing.
  bool is_installing = false;

  // Whether this result is a recommendation.
  bool is_recommendation = false;

  // A query URL associated with this result. The meaning and treatment of the
  // URL (e.g. displaying inline web contents) is dependent on the result type.
  absl::optional<GURL> query_url;

  // An optional id that identifies an equivalent result to this result. Answer
  // card result has this set to remove the equivalent omnibox
  // search-what-you-typed result when there is an answer card for the query.
  absl::optional<std::string> equivalent_result_id;

  // The icon of this result.
  SearchResultIconInfo icon;

  // The icon of this result in a smaller dimension to be rendered in suggestion
  // chip view.
  // TODO(crbug.com/1225161): Remove this and replace it with |icon| and an
  // appropriately set |icon_dimension|.
  gfx::ImageSkia chip_icon;

  // The badge icon of this result that indicates its type, e.g. installable
  // from PlayStore, installable from WebStore, etc.
  ui::ImageModel badge_icon;

  // Flag indicating whether the `badge_icon` should be painted atop a circle
  // background image.
  bool use_badge_icon_background = false;

  // If set to true, whether or not to send visibility updates through to to
  // the chrome side when this result is set visible/invisible.
  bool notify_visibility_change = false;
};

// A struct holding a search result id and its corresponding position index that
// was being shown to the user.
struct SearchResultIdWithPositionIndex {
  SearchResultIdWithPositionIndex(std::string result_id, int index)
      : id(result_id), position_index(index) {}

  // The id of the result.
  std::string id;

  // The position index of the result.
  int position_index;
};

using SearchResultIdWithPositionIndices =
    std::vector<SearchResultIdWithPositionIndex>;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_TYPES_H_
