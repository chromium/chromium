// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_TYPES_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_TYPES_H_

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/sync/model/string_ordinal.h"
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

// App list config types supported by AppListConfig.
enum class AppListConfigType {
  // Config type used for all screens when app_list_features::ScalableAppList
  // feature is disabled. (Note that two configs having this type can differ, in
  // case one of them is scaled down).
  kShared,

  // Config used on large screens when app_list_features::ScalableAppList
  // feature is enabled.
  kLarge,

  // Config used on medium sized screens when app_list_features::ScalableAppList
  // feature is enabled.
  kMedium,

  // Config used on small screens when app_list_features::ScalableAppList
  // feature is enabled.
  kSmall
};

// A structure holding the common information which is sent between ash and,
// chrome representing an app list item.
struct ASH_PUBLIC_EXPORT AppListItemMetadata {
  AppListItemMetadata();
  AppListItemMetadata(const AppListItemMetadata& rhs);
  ~AppListItemMetadata();

  std::string id;          // Id of the app list item.
  std::string name;        // Corresponding app/folder's name of the item.
  std::string short_name;  // Corresponding app's short name of the item. Empty
                           // if the app doesn't have one or it's a folder.
  std::string folder_id;   // Id of folder where the item resides.
  syncer::StringOrdinal position;  // Position of the item.
  bool is_folder = false;          // Whether this item is a folder.
  bool is_persistent = false;  // Whether this folder is allowed to contain only
                               // 1 item.
  gfx::ImageSkia icon;         // The icon of this item.
  bool is_page_break = false;  // Whether this item is a "page break" item.
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
  kMaxValue = kLaunchedFromSearchBox,
};

// The UI representation of the search result. Currently all search results
// that are not apps (OminboxResult, LauncherSearcResult, etc.) are grouped
// into kSearchResult. Meanwhile SearchResultTileItemView (shown in zero state)
// and suggested chips are considered kAppSearchResult.
enum class AppListLaunchType {
  kSearchResult = 0,
  kAppSearchResult,
};

// Type of the search result, which is set in Chrome.
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
  kDriveQuickAccess,       // Drive QuickAccess results.
  // Add new values here.
};

// How the result should be displayed. Do not change the order of these as
// they are used for metrics.
enum SearchResultDisplayType {
  kNone = 0,
  kList,
  kTile,
  kRecommendation,
  kCard,
  // Add new values here.

  kLast,  // Don't use over IPC
};

// Which UI container should the result be displayed in.
enum SearchResultDisplayLocation {
  kSuggestionChipContainer,
  kTileListContainer,
  kPlacementUndefined,
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

// Actions for OmniBox zero state suggestion.
enum OmniBoxZeroStateAction {
  // Removes the zero state suggestion.
  kRemoveSuggestion = 0,
  // Appends the suggestion to search box query.
  kAppendSuggestion,
  // kZeroStateActionMax is always last.
  kZeroStateActionMax
};

// Returns OmniBoxZeroStateAction mapped for |button_index|.
ASH_PUBLIC_EXPORT OmniBoxZeroStateAction
GetOmniBoxZeroStateAction(int button_index);

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
  SearchResultAction(const gfx::ImageSkia& image,
                     const base::string16& tooltip_text,
                     bool visible_on_hover);
  SearchResultAction(const SearchResultAction& other);
  ~SearchResultAction();

  gfx::ImageSkia image;
  base::string16 tooltip_text;
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
  base::string16 title;

  // A detail string of this result.
  base::string16 details;

  // An text to be announced by a screen reader app.
  base::string16 accessible_name;

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
  base::string16 formatted_price;

  // The type of this result.
  AppListSearchResultType result_type = AppListSearchResultType::kUnknown;

  // The subtype of this result. Derived search result classes can use this to
  // represent their own subtypes. Currently, OmniboxResult sets this to
  // indicate this is a history result, previous query, etc. A value of -1
  // indicates no subtype has been set.
  int result_subtype = -1;

  // How this result is displayed.
  SearchResultDisplayType display_type = SearchResultDisplayType::kList;

  // Which UI container should the result be displayed in.
  SearchResultDisplayLocation display_location =
      SearchResultDisplayLocation::kPlacementUndefined;

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

  // A query URL associated with this result. The meaning and treatment of the
  // URL (e.g. displaying inline web contents) is dependent on the result type.
  base::Optional<GURL> query_url;

  // An optional id that identifies an equivalent result to this result. Answer
  // card result has this set to remove the equivalent omnibox
  // search-what-you-typed result when there is an answer card for the query.
  base::Optional<std::string> equivalent_result_id;

  // The icon of this result.
  gfx::ImageSkia icon;

  // The icon of this result in a smaller dimension to be rendered in suggestion
  // chip view.
  gfx::ImageSkia chip_icon;

  // The badge icon of this result that indicates its type, e.g. installable
  // from PlayStore, installable from WebStore, etc.
  gfx::ImageSkia badge_icon;

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
