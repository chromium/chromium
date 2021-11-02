// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_icon_container.h"

#include "ash/public/cpp/desk_template.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/templates/desks_templates_icon_view.h"
#include "base/containers/contains.h"
#include "components/app_restore/app_launch_info.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"

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

// TODO(chinsenj): Revisit how we determine the sorting order.
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
    for (auto& restore_data : app_id_to_launch_list_entry.second) {
      const std::string& identifier =
          restore_data.second->urls.has_value()
              ? restore_data.second->urls
                    .value()[restore_data.second->active_tab_index.value()]
                    .spec()
              : app_id_to_launch_list_entry.first;
      const int activation_index =
          restore_data.second->activation_index.value();

      // A single app can have multiple windows so count their occurrences and
      // use the smallest activation index for sorting purposes.
      if (!base::Contains(identifier_info, identifier)) {
        identifier_info[identifier] = {activation_index,
                                       /*count=*/1};
      } else {
        ++identifier_info[identifier].count;
        identifier_info[identifier].activation_index = std::min(
            identifier_info[identifier].activation_index, activation_index);
      }
    }
  }

  SetIcons(SortIconIdentifiers(identifier_info));
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

void DesksTemplatesIconContainer::SetIcons(
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
    icon_view->SetIconAndCount(identifiers_and_counts[i].first,
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
  overflow_icon_view->SetIconAndCount(
      std::string(), identifiers_and_counts.size() - num_added_icons);
  icon_views_.push_back(overflow_icon_view);
}

BEGIN_METADATA(DesksTemplatesIconContainer, views::BoxLayoutView)
END_METADATA

}  // namespace ash
