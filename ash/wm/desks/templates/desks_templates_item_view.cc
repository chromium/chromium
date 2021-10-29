// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_item_view.h"

#include <string>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/templates/desks_templates_delete_button.h"
#include "ash/wm/desks/templates/desks_templates_icon_view.h"
#include "ash/wm/desks/templates/desks_templates_presenter.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

// The padding values of the DesksTemplatesItemView.
constexpr int kHorizontalPaddingDp = 24;
constexpr int kVerticalPaddingDp = 16;

// The space between icon views.
constexpr int kIconSpacingDp = 8;

// The preferred size of the whole DesksTemplatesItemView.
constexpr gfx::Size kPreferredSize(220, 120);

// The corner radius for the DesksTemplatesItemView.
constexpr int kCornerRadius = 16;

// TODO(richui): Replace these temporary values once specs come out.
constexpr gfx::Size kViewSize(250, 20);
constexpr int kDeleteButtonMargin = 8;
constexpr int kDeleteButtonSize = 24;

constexpr char kAmPmTimeDateFmtStr[] = "%d:%02d%s, %d-%02d-%02d";

// A struct for storing the various information used to determine which app
// icons/favicons to display.
struct IconInfo {
  int activation_index;
  int count;
};

// TODO(richui): This is a placeholder text format. Update this once specs are
// done.
std::u16string GetTimeStr(base::Time timestamp) {
  base::Time::Exploded exploded_time;
  timestamp.LocalExplode(&exploded_time);

  const int noon = 12;
  int hour = exploded_time.hour % noon;
  if (hour == 0)
    hour += noon;

  std::string time = base::StringPrintf(
      kAmPmTimeDateFmtStr, hour, exploded_time.minute,
      (exploded_time.hour >= noon ? "pm" : "am"), exploded_time.year,
      exploded_time.month, exploded_time.day_of_month);
  return base::UTF8ToUTF16(time);
}

// TODO(chinsenj): Revisit how we determine the sorting order.
// Given `desk_template`, returns an ordered vector of the unique icon
// identifiers (app ids/urls) and their number of occurrences.
std::vector<std::pair<std::string, int>> CountAndSortIconIdentifiers(
    DeskTemplate* desk_template) {
  const app_restore::RestoreData* restore_data =
      desk_template->desk_restore_data();
  if (!restore_data)
    return std::vector<std::pair<std::string, int>>();

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

  // Create a vector using `identifier_info` that contains pairs of identifiers
  // and counts. This will be unsorted.
  std::vector<std::pair<std::string, int>> identifiers_with_count;
  for (auto entry : identifier_info) {
    identifiers_with_count.emplace_back(
        std::pair<std::string, int>{entry.first, entry.second.count});
  }

  // Sort `identifiers_with_count` using the activation indices stored in
  // `identifier_info`.
  std::sort(identifiers_with_count.begin(), identifiers_with_count.end(),
            [identifier_info](std::pair<std::string, int> data_1,
                              std::pair<std::string, int> data_2) {
              return identifier_info.at(data_1.first).activation_index <
                     identifier_info.at(data_2.first).activation_index;
            });

  return identifiers_with_count;
}

}  // namespace

DesksTemplatesItemView::DesksTemplatesItemView(DeskTemplate* desk_template)
    : uuid_(desk_template->uuid()) {
  // TODO(richui): Remove all the borders. It is only used for visualizing
  // bounds while it is a placeholder.

  auto delete_button_callback = base::BindRepeating(
      &DesksTemplatesItemView::OnDeleteButtonPressed, base::Unretained(this));
  auto launch_template_callback = base::BindRepeating(
      &DesksTemplatesItemView::OnGridItemPressed, base::Unretained(this));

  views::View* spacer;
  views::BoxLayoutView* container;
  views::Builder<DesksTemplatesItemView>(this)
      .SetPreferredSize(kPreferredSize)
      .SetUseDefaultFillLayout(true)
      .SetAccessibleName(desk_template->template_name())
      .SetCallback(std::move(launch_template_callback))
      .SetBackground(views::CreateRoundedRectBackground(
          AshColorProvider::Get()->GetControlsLayerColor(
              AshColorProvider::ControlsLayerType::
                  kControlBackgroundColorInactive),
          kCornerRadius))
      .AddChildren(
          views::Builder<views::BoxLayoutView>()
              .CopyAddressTo(&container)
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kStart)
              .SetInsideBorderInsets(
                  gfx::Insets(kVerticalPaddingDp, kHorizontalPaddingDp))
              .AddChildren(
                  views::Builder<views::Textfield>()
                      .CopyAddressTo(&name_view_)
                      .SetText(desk_template->template_name())
                      .SetAccessibleName(desk_template->template_name())
                      .SetPreferredSize(kViewSize),
                  views::Builder<views::Label>()
                      .CopyAddressTo(&time_view_)
                      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                      .SetText(GetTimeStr(desk_template->created_time()))
                      .SetPreferredSize(kViewSize),
                  views::Builder<views::View>().CopyAddressTo(&spacer),
                  views::Builder<views::BoxLayoutView>()
                      .CopyAddressTo(&preview_view_)
                      .SetOrientation(
                          views::BoxLayout::Orientation::kHorizontal)
                      .SetBetweenChildSpacing(kIconSpacingDp)),
          views::Builder<DesksTemplatesDeleteButton>()
              .CopyAddressTo(&delete_button_)
              .SetCallback(std::move(delete_button_callback)))
      .BuildChildren();

  container->SetFlexForView(spacer, 1);
  UpdateDeleteButtonVisibility();
  SetIcons(CountAndSortIconIdentifiers(desk_template));
}

DesksTemplatesItemView::~DesksTemplatesItemView() = default;

void DesksTemplatesItemView::UpdateDeleteButtonVisibility() {
  // For switch access, setting the delete button to visible allows users to
  // navigate to it.
  // TODO(richui): update `force_show_delete_button_` based on touch events.
  delete_button_->SetVisible(
      (IsMouseHovered() || force_show_delete_button_ ||
       Shell::Get()->accessibility_controller()->IsSwitchAccessRunning()));
}

void DesksTemplatesItemView::Layout() {
  views::View::Layout();

  delete_button_->SetBoundsRect(
      gfx::Rect(width() - kDeleteButtonSize - kDeleteButtonMargin,
                kDeleteButtonMargin, kDeleteButtonSize, kDeleteButtonSize));

  if (icon_views_.empty())
    return;

  const int available_horizontal_space = preview_view_->bounds().width();
  // Use the preferred size of the `preview_view_` since this will provide the
  // width as if every view in `icon_views_` is shown.
  int used_horizontal_space = preview_view_->GetPreferredSize().width();
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

void DesksTemplatesItemView::SetIcons(
    const std::vector<std::pair<std::string, int>>& identifiers_and_counts) {
  DCHECK(icon_views_.empty());

  if (identifiers_and_counts.empty())
    return;

  for (size_t i = 0; i < kMaxIcons && i < identifiers_and_counts.size(); ++i) {
    DesksTemplatesIconView* icon_view = preview_view_->AddChildView(
        views::Builder<DesksTemplatesIconView>()
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
  const int num_added_icons = preview_view_->children().size();
  DesksTemplatesIconView* overflow_icon_view = preview_view_->AddChildView(
      views::Builder<DesksTemplatesIconView>()
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

void DesksTemplatesItemView::OnDeleteButtonPressed() {
  DesksTemplatesPresenter::Get()->DeleteEntry(uuid_.AsLowercaseString());
}

void DesksTemplatesItemView::OnGridItemPressed() {
  DesksTemplatesPresenter::Get()->LaunchDeskTemplate(uuid_.AsLowercaseString());
}

BEGIN_METADATA(DesksTemplatesItemView, views::Button)
END_METADATA

}  // namespace ash
