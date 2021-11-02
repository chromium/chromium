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
#include "ash/wm/desks/templates/desks_templates_icon_container.h"
#include "ash/wm/desks/templates/desks_templates_presenter.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {

// The padding values of the DesksTemplatesItemView.
constexpr int kHorizontalPaddingDp = 24;
constexpr int kVerticalPaddingDp = 16;

// The preferred size of the whole DesksTemplatesItemView.
constexpr gfx::Size kPreferredSize(220, 120);

// The corner radius for the DesksTemplatesItemView.
constexpr int kCornerRadius = 16;

// TODO(richui): Replace these temporary values once specs come out.
constexpr gfx::Size kViewSize(250, 20);
constexpr int kDeleteButtonMargin = 8;
constexpr int kDeleteButtonSize = 24;

constexpr char kAmPmTimeDateFmtStr[] = "%d:%02d%s, %d-%02d-%02d";

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

}  // namespace

DesksTemplatesItemView::DesksTemplatesItemView(DeskTemplate* desk_template)
    : uuid_(desk_template->uuid()) {
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
                  views::Builder<DesksTemplatesIconContainer>().CopyAddressTo(
                      &icon_container_view_)),
          views::Builder<DesksTemplatesDeleteButton>()
              .CopyAddressTo(&delete_button_)
              .SetCallback(std::move(delete_button_callback)))
      .BuildChildren();

  icon_container_view_->PopulateIconContainerFromTemplate(desk_template);
  container->SetFlexForView(spacer, 1);
  UpdateDeleteButtonVisibility();
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
