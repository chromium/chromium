// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_icon_container.h"

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/desks_templates_delegate.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/templates/desks_templates_icon_view.h"
#include "base/containers/contains.h"
#include "components/app_restore/app_launch_info.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The space between icon views.
constexpr int kIconSpacingDp = 8;

// A struct for storing the various information used to determine which app
// icons/favicons to display.
struct IconInfo {
  int activation_index;
  int count;
};

// Given a map from unique icon identifiers to their count, returns an ordered
// vector of the unique icon identifiers (app ids/urls) and their number of
// occurrences.
std::vector<std::pair<std::string, int>> SortIconIdentifiers(
    const std::map<std::string, IconInfo>& identifier_info) {
  // Create a vector using `identifier_info` that contains pairs of identifiers
  // and counts. This will be unsorted.
  std::vector<std::pair<std::string, int>> identifiers_with_count;
  for (const auto& entry : identifier_info)
    identifiers_with_count.emplace_back(entry.first, entry.second.count);

  // Sort `identifiers_with_count` using the activation indices stored in
  // `identifier_info`.
  std::sort(identifiers_with_count.begin(), identifiers_with_count.end(),
            [&identifier_info](const std::pair<std::string, int>& data_1,
                               const std::pair<std::string, int>& data_2) {
              return identifier_info.at(data_1.first).activation_index <
                     identifier_info.at(data_2.first).activation_index;
            });

  return identifiers_with_count;
}

// Inserts an `IconInfo` struct into `out_identifier_info` if no entry exists
// for `identifier`. If an entry exists for `identifier`, updates its values.
void InsertIdentifierInfo(
    const std::string& identifier,
    int activation_index,
    std::map<std::string, IconInfo>* out_identifier_info) {
  // A single app/site can have multiple windows so count their occurrences and
  // use the smallest activation index for sorting purposes.
  if (!base::Contains(*out_identifier_info, identifier)) {
    (*out_identifier_info)[identifier] = {activation_index, /*count=*/1};
  } else {
    ++(*out_identifier_info)[identifier].count;
    (*out_identifier_info)[identifier].activation_index = std::min(
        (*out_identifier_info)[identifier].activation_index, activation_index);
  }
}

// Iterates through `launch_list`, inserting `IconInfo` structs into
// `out_identifier_info` for each tab and app.
void InsertIdentifierInfoFromLaunchList(
    const std::string& app_id,
    const app_restore::RestoreData::LaunchList& launch_list,
    std::map<std::string, IconInfo>* out_identifier_info) {
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
        app_id == extension_misc::kChromeAppId &&
        (!restore_data.second->app_type_browser.has_value() ||
         !restore_data.second->app_type_browser.value());
    const int activation_index = restore_data.second->activation_index.value();
    const int active_tab_index =
        restore_data.second->active_tab_index.value_or(-1);
    if (restore_data.second->urls.has_value() && is_browser) {
      const auto& urls = restore_data.second->urls.value();
      for (int i = 0; i < static_cast<int>(urls.size()); ++i) {
        InsertIdentifierInfo(urls[i].spec(),
                             active_tab_index == i
                                 ? activation_index
                                 : kInactiveTabOffset + activation_index,
                             out_identifier_info);
      }
    } else {
      InsertIdentifierInfo(app_id, activation_index, out_identifier_info);
    }
  }
}

}  // namespace

DesksTemplatesIconContainer::DesksTemplatesIconContainer() {
  views::Builder<DesksTemplatesIconContainer>(this)
      .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
      .SetBetweenChildSpacing(kIconSpacingDp)
      .BuildChildren();
}

DesksTemplatesIconContainer::~DesksTemplatesIconContainer() = default;

void DesksTemplatesIconContainer::PopulateIconContainerFromTemplate(
    DeskTemplate* desk_template) {
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  if (!restore_data)
    return;

  // Iterate through the template's WindowInfo, counting the occurrences of each
  // unique icon identifier and storing their lowest activation index.
  std::map<std::string, IconInfo> identifier_info;
  for (auto& app_id_to_launch_list_entry :
       restore_data->app_id_to_launch_list()) {
    InsertIdentifierInfoFromLaunchList(app_id_to_launch_list_entry.first,
                                       app_id_to_launch_list_entry.second,
                                       &identifier_info);
  }

  CreateIconViewsFromIconIdentifiers(SortIconIdentifiers(identifier_info));
}

void DesksTemplatesIconContainer::PopulateIconContainerFromWindows(
    const std::vector<aura::Window*>& windows) {
  DCHECK(!windows.empty());

  // Iterate through `windows`, counting the occurrences of each unique icon and
  // storing their lowest activation index.
  std::map<std::string, IconInfo> identifier_info;
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

    InsertIdentifierInfo(app_id, i, &identifier_info);
  }

  CreateIconViewsFromIconIdentifiers(SortIconIdentifiers(identifier_info));
}

void DesksTemplatesIconContainer::Layout() {
  views::BoxLayoutView::Layout();

  if (icon_views_.empty())
    return;

  const int available_horizontal_space = bounds().width();
  // Use the preferred size of this since this will provide the width as if
  // every view in `icon_views_` is shown.
  int used_horizontal_space = GetPreferredSize().width();
  DesksTemplatesIconView* overflow_icon_view = icon_views_.back();
  if (used_horizontal_space > available_horizontal_space) {
    // Reverse iterate through `icon_views_` starting with the first
    // non-overflow icon view (i.e. the second-last element). Hide as many icons
    // we need to fit `available_horizontal_space` and then update the overflow
    // icon view.
    int num_hidden_icons = 0;
    for (auto it = ++icon_views_.rbegin(); it != icon_views_.rend(); ++it) {
      if ((*it)->GetVisible()) {
        used_horizontal_space -= (*it)->GetPreferredSize().width();
        (*it)->SetVisible(false);
        ++num_hidden_icons;
      }

      if (used_horizontal_space <= available_horizontal_space)
        break;
    }
    overflow_icon_view->UpdateCount(overflow_icon_view->count() +
                                    num_hidden_icons);
  } else if (overflow_icon_view->count() == 0) {
    // There is no overflow so hide the overflow icon view.
    overflow_icon_view->SetVisible(false);
  }
}

void DesksTemplatesIconContainer::CreateIconViewsFromIconIdentifiers(
    const std::vector<std::pair<std::string, int>>& identifiers_and_counts) {
  DCHECK(icon_views_.empty());

  if (identifiers_and_counts.empty())
    return;

  for (size_t i = 0; i < kMaxIcons && i < identifiers_and_counts.size(); ++i) {
    DesksTemplatesIconView* icon_view =
        AddChildView(views::Builder<DesksTemplatesIconView>()
                         .SetBackground(views::CreateRoundedRectBackground(
                             AshColorProvider::Get()->GetControlsLayerColor(
                                 AshColorProvider::ControlsLayerType::
                                     kControlBackgroundColorInactive),
                             DesksTemplatesIconView::kIconSize / 2))
                         .Build());
    icon_view->SetIconIdentifierAndCount(identifiers_and_counts[i].first,
                                         identifiers_and_counts[i].second);
    icon_views_.push_back(icon_view);
  }

  // Always add a `DesksTemplatesIconView` overflow counter in case the width
  // of the view changes. It will be hidden if not needed.
  const int num_added_icons = children().size();
  DesksTemplatesIconView* overflow_icon_view =
      AddChildView(views::Builder<DesksTemplatesIconView>()
                       .SetBackground(views::CreateRoundedRectBackground(
                           AshColorProvider::Get()->GetControlsLayerColor(
                               AshColorProvider::ControlsLayerType::
                                   kControlBackgroundColorInactive),
                           DesksTemplatesIconView::kIconSize / 2))
                       .Build());
  overflow_icon_view->SetIconIdentifierAndCount(
      std::string(), identifiers_and_counts.size() - num_added_icons);
  icon_views_.push_back(overflow_icon_view);
}

BEGIN_METADATA(DesksTemplatesIconContainer, views::BoxLayoutView)
END_METADATA

}  // namespace ash
