// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_icon_container.h"

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/desks_templates_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/templates/saved_desk_icon_view.h"
#include "base/containers/contains.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The space between icon views.
constexpr int kIconSpacingDp = 8;

bool IsBrowserAppId(const std::string& app_id) {
  return app_id == app_constants::kChromeAppId ||
         app_id == app_constants::kLacrosAppId;
}

// Given a map of unique icon identifiers to icon info, returns a vector of the
// same key, value pair ordered by icons' activation index.
std::vector<SavedDeskIconContainer::IconIdentifierAndIconInfo>
SortIconIdentifierToIconInfo(
    std::map<std::string, SavedDeskIconContainer::IconInfo>&
        icon_identifier_to_icon_info) {
  // Create a vector using `sorted_icon_identifier_to_icon_info` that contains
  // pairs of identifiers and counts. This will initially be unsorted.
  std::vector<SavedDeskIconContainer::IconIdentifierAndIconInfo>
      sorted_icon_identifier_to_icon_info;

  for (const auto& entry : icon_identifier_to_icon_info) {
    sorted_icon_identifier_to_icon_info.emplace_back(entry.first,
                                                     std::move(entry.second));
  }

  // Sort `sorted_icon_identifier_to_icon_info` using the activation indices
  // stored in `icon_identifier_to_icon_info`. `data_n.first` points to the icon
  // identifier.
  std::sort(
      sorted_icon_identifier_to_icon_info.begin(),
      sorted_icon_identifier_to_icon_info.end(),
      [&icon_identifier_to_icon_info](
          const SavedDeskIconContainer::IconIdentifierAndIconInfo& data_1,
          const SavedDeskIconContainer::IconIdentifierAndIconInfo& data_2) {
        return icon_identifier_to_icon_info.at(data_1.first).activation_index <
               icon_identifier_to_icon_info.at(data_2.first).activation_index;
      });

  return sorted_icon_identifier_to_icon_info;
}

// Inserts an `IconInfo` struct into `out_icon_identifier_to_icon_info` if no
// entry exists for `identifier`. If an entry exists for `identifier`, updates
// its values.
void InsertIconIdentifierToIconInfo(
    const std::string& app_id,
    const std::u16string& app_title,
    const std::string& identifier,
    int activation_index,
    std::map<std::string, SavedDeskIconContainer::IconInfo>*
        out_icon_identifier_to_icon_info) {
  // A single app/site can have multiple windows so count their occurrences and
  // use the smallest activation index for sorting purposes.
  if (!base::Contains(*out_icon_identifier_to_icon_info, identifier)) {
    (*out_icon_identifier_to_icon_info)[identifier] = {
        app_id, base::UTF16ToUTF8(app_title), activation_index,
        /*count=*/1};
  } else {
    ++(*out_icon_identifier_to_icon_info)[identifier].count;
    (*out_icon_identifier_to_icon_info)[identifier].activation_index = std::min(
        (*out_icon_identifier_to_icon_info)[identifier].activation_index,
        activation_index);
  }
}

// Iterates through `launch_list`, inserting `IconInfo` structs into
// `out_icon_identifier_to_icon_info` for each tab and app.
void InsertIconIdentifierToIconInfoFromLaunchList(
    const std::string& app_id,
    const app_restore::RestoreData::LaunchList& launch_list,
    std::map<std::string, SavedDeskIconContainer::IconInfo>*
        out_icon_identifier_to_icon_info) {
  // We want to group active tabs and apps ahead of inactive tabs so offsets
  // inactive tabs activation index by `kInactiveTabOffset`. In almost every use
  // case, there should be no more than `kInactiveTabOffset` number of tabs +
  // apps on a desk.
  constexpr int kInactiveTabOffset = 10000;

  for (auto& restore_data : launch_list) {
    // If `restore_data` is a SWA then it will have a valid url for its active
    // tab. However, in this case we want to display the SWA's icon via its app
    // id so to determine whether `restore_data` is an SWA we need to check
    // whether it's a browser.
    const bool is_browser =
        IsBrowserAppId(app_id) &&
        (!restore_data.second->app_type_browser.has_value() ||
         !restore_data.second->app_type_browser.value());
    const int activation_index = restore_data.second->activation_index.value();
    const int active_tab_index =
        restore_data.second->active_tab_index.value_or(-1);
    const std::u16string app_title = restore_data.second->title.value_or(u"");
    if (restore_data.second->urls.has_value() && is_browser) {
      const auto& urls = restore_data.second->urls.value();
      // Make all urls that have the same domain identical.
      std::map<GURL, GURL> seen_urls_by_domain;
      for (int i = 0; i < static_cast<int>(urls.size()); ++i) {
        // For each domain, if we have seen the domain before in another url,
        // use that url instead. If we haven't, register this url as the url to
        // use for this domain.
        GURL url = seen_urls_by_domain[urls[i].GetWithEmptyPath()];
        if (!url.is_valid()) {
          url = urls[i];
          seen_urls_by_domain[url.GetWithEmptyPath()] = url;
        }
        InsertIconIdentifierToIconInfo(
            app_id, app_title, url.spec(),
            active_tab_index == i ? activation_index
                                  : kInactiveTabOffset + activation_index,
            out_icon_identifier_to_icon_info);
      }
    } else {
      // PWAs will have the same app id as chrome. For these apps, retrieve
      // their app id from their app name if possible.
      std::string new_app_id = app_id;
      absl::optional<std::string> app_name = restore_data.second->app_name;
      if (IsBrowserAppId(app_id) && app_name.has_value())
        new_app_id = app_restore::GetAppIdFromAppName(app_name.value());

      InsertIconIdentifierToIconInfo(app_id, app_title, new_app_id,
                                     activation_index,
                                     out_icon_identifier_to_icon_info);
    }
  }
}

}  // namespace

SavedDeskIconContainer::SavedDeskIconContainer() {
  views::Builder<SavedDeskIconContainer>(this)
      .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
      .SetBetweenChildSpacing(kIconSpacingDp)
      .BuildChildren();
}

SavedDeskIconContainer::~SavedDeskIconContainer() = default;

void SavedDeskIconContainer::PopulateIconContainerFromTemplate(
    const DeskTemplate* desk_template) {
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  if (!restore_data)
    return;

  // Iterate through the template's WindowInfo, counting the occurrences of each
  // unique icon identifier and storing their lowest activation index.
  std::map<std::string, IconInfo> icon_identifier_to_icon_info;
  const auto& launch_list = restore_data->app_id_to_launch_list();
  for (auto& app_id_to_launch_list_entry : launch_list) {
    InsertIconIdentifierToIconInfoFromLaunchList(
        app_id_to_launch_list_entry.first, app_id_to_launch_list_entry.second,
        &icon_identifier_to_icon_info);
  }

  CreateIconViewsFromIconIdentifiers(
      SortIconIdentifierToIconInfo(icon_identifier_to_icon_info));
}

void SavedDeskIconContainer::PopulateIconContainerFromWindows(
    const std::vector<aura::Window*>& windows) {
  DCHECK(!windows.empty());

  // Iterate through `windows`, counting the occurrences of each unique icon and
  // storing their lowest activation index.
  std::map<std::string, IconInfo> icon_identifier_to_icon_info;
  auto* delegate = Shell::Get()->desks_templates_delegate();
  for (size_t i = 0; i < windows.size(); ++i) {
    auto* window = windows[i];

    // If `window` is an incognito window, we want to display the incognito icon
    // instead of its favicons so denote it using
    // `DeskTemplate::kIncognitoWindowIdentifier`.
    const bool is_incognito_window = delegate->IsIncognitoWindow(window);
    const std::string app_id =
        is_incognito_window
            ? DeskTemplate::kIncognitoWindowIdentifier
            : ShelfID::Deserialize(window->GetProperty(kShelfIDKey)).app_id;
    if (is_incognito_window && !incognito_window_color_provider_) {
      incognito_window_color_provider_ =
          views::Widget::GetWidgetForNativeWindow(window)->GetColorProvider();
    }

    // Since there were no modifications to `app_id`, app id and icon identifier
    // are both `app_id`.
    InsertIconIdentifierToIconInfo(
        /*app_id=*/app_id, /*app_title=*/window->GetTitle(),
        /*identifier=*/app_id, i, &icon_identifier_to_icon_info);
  }

  CreateIconViewsFromIconIdentifiers(
      SortIconIdentifierToIconInfo(icon_identifier_to_icon_info));
}

void SavedDeskIconContainer::Layout() {
  views::BoxLayoutView::Layout();

  auto icon_views = children();
  if (icon_views.empty())
    return;

  const int available_horizontal_space = bounds().width();
  // Use the preferred size of this since this will provide the width as if
  // every view in `icon_views` is shown.
  int used_horizontal_space = GetPreferredSize().width();
  auto* overflow_icon_view = static_cast<SavedDeskIconView*>(icon_views.back());
  if (used_horizontal_space > available_horizontal_space) {
    // Reverse iterate through `icon_views` starting with the first
    // non-overflow icon view (i.e. the second-last element). Hide as many icons
    // we need to fit `available_horizontal_space` and then update the overflow
    // icon view.
    int num_hidden_icons = 0;
    for (auto it = ++icon_views.rbegin(); it != icon_views.rend(); ++it) {
      if ((*it)->GetVisible()) {
        used_horizontal_space -=
            ((*it)->GetPreferredSize().width() + kIconSpacingDp);
        (*it)->SetVisible(false);
        num_hidden_icons += static_cast<SavedDeskIconView*>((*it))->count();
      }

      if (used_horizontal_space <= available_horizontal_space)
        break;
    }
    // Overflow icon count = the number of hidden icons + the number of
    // unavailable windows.
    overflow_icon_view->UpdateCount(overflow_icon_view->count() +
                                    num_hidden_icons);
  } else if (overflow_icon_view->count() == 0) {
    // There is no overflow so hide the overflow icon view.
    overflow_icon_view->SetVisible(false);
  }
}

void SavedDeskIconContainer::CreateIconViewsFromIconIdentifiers(
    const std::vector<IconIdentifierAndIconInfo>&
        icon_identifier_to_icon_info) {
  DCHECK(children().empty());

  if (icon_identifier_to_icon_info.empty())
    return;

  auto* delegate = Shell::Get()->desks_templates_delegate();
  int num_hidden_icons = 0;
  for (size_t i = 0; i < icon_identifier_to_icon_info.size(); ++i) {
    auto icon_identifier = icon_identifier_to_icon_info[i].first;
    auto icon_info = icon_identifier_to_icon_info[i].second;
    // Don't create new icons once we have reached the max, or if the app is
    // unavailable (uninstalled or unsupported). Count the amount of skipped
    // apps so we know what to display on the overflow. In addition, dialog
    // popups may show incognito window icons. Saved desks will not have
    // incognito window icon identifiers and will not count them here.
    if (children().size() < kMaxIcons &&
        (icon_identifier == DeskTemplate::kIncognitoWindowIdentifier ||
         delegate->IsAppAvailable(icon_info.app_id))) {
      SavedDeskIconView* icon_view =
          AddChildView(views::Builder<SavedDeskIconView>()
                           .SetBackground(views::CreateRoundedRectBackground(
                               AshColorProvider::Get()->GetControlsLayerColor(
                                   AshColorProvider::ControlsLayerType::
                                       kControlBackgroundColorInactive),
                               SavedDeskIconView::kIconViewSize / 2))
                           .Build());
      icon_view->SetIconIdentifierAndCount(icon_identifier, icon_info.app_id,
                                           icon_info.app_title, icon_info.count,
                                           /*show_plus=*/true);
    } else {
      num_hidden_icons += icon_info.count;
    }
  }

  // If no child views were added, the icon container contains only unavailable
  // apps so we should *not* show plus.
  const bool show_plus = !children().empty();

  // Always add a `SavedDeskIconView` overflow counter in case the width
  // of the view changes. It will be hidden if not needed.
  SavedDeskIconView* overflow_icon_view =
      AddChildView(views::Builder<SavedDeskIconView>()
                       .SetBackground(views::CreateRoundedRectBackground(
                           AshColorProvider::Get()->GetControlsLayerColor(
                               AshColorProvider::ControlsLayerType::
                                   kControlBackgroundColorInactive),
                           SavedDeskIconView::kIconViewSize / 2))
                       .Build());

  // Set `icon_identifier`, `app_id` and `app_title` to be empty strings for
  // overflow icon views, since only the count should matter.
  overflow_icon_view->SetIconIdentifierAndCount(
      /*icon_identifier=*/std::string(), /*app_id=*/std::string(),
      /*app_title=*/std::string(),
      /*count=*/num_hidden_icons, show_plus);
}

BEGIN_METADATA(SavedDeskIconContainer, views::BoxLayoutView)
END_METADATA

}  // namespace ash
