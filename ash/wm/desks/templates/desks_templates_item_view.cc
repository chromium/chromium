// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_item_view.h"

#include <string>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/close_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/wm/desks/label_textfield.h"
#include "ash/wm/desks/templates/desks_templates_dialog_controller.h"
#include "ash/wm/desks/templates/desks_templates_icon_container.h"
#include "ash/wm/desks/templates/desks_templates_name_view.h"
#include "ash/wm/desks/templates/desks_templates_presenter.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_targeter_delegate.h"

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
constexpr int kDeleteButtonMargin = 8;

// The margin between the grid item contents and the card container.
constexpr int kGridItemMargin = 24;
constexpr int kTimeViewHeight = 20;

// Pixel offset for the focus ring around the whole time. Positive values means
// the focus ring sits outside of the item.
constexpr int kFocusRingOffset = 2;

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
  auto launch_template_callback = base::BindRepeating(
      &DesksTemplatesItemView::OnGridItemPressed, base::Unretained(this));

  views::View* spacer;
  views::BoxLayoutView* card_container;
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
              .CopyAddressTo(&card_container)
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .SetCrossAxisAlignment(
                  views::BoxLayout::CrossAxisAlignment::kStart)
              .SetInsideBorderInsets(
                  gfx::Insets(kVerticalPaddingDp, kHorizontalPaddingDp))
              .AddChildren(
                  views::Builder<DesksTemplatesNameView>()
                      .CopyAddressTo(&name_view_)
                      .SetText(desk_template->template_name())
                      .SetAccessibleName(desk_template->template_name()),
                  views::Builder<views::Label>()
                      .CopyAddressTo(&time_view_)
                      .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                      .SetText(GetTimeStr(desk_template->created_time()))
                      .SetPreferredSize(gfx::Size(
                          kPreferredSize.width() - kGridItemMargin * 2,
                          kTimeViewHeight)),
                  views::Builder<views::View>().CopyAddressTo(&spacer),
                  views::Builder<DesksTemplatesIconContainer>().CopyAddressTo(
                      &icon_container_view_)),
          views::Builder<views::View>().CopyAddressTo(&hover_container_))
      .BuildChildren();

  // TODO(crbug.com/1267470): Make `PillButton` work with views::Builder.
  launch_button_ = hover_container_->AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&DesksTemplatesItemView::OnGridItemPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_ASH_DESKS_TEMPLATES_USE_TEMPLATE_BUTTON),
      PillButton::Type::kIconless, /*icon=*/nullptr));

  delete_button_ = hover_container_->AddChildView(std::make_unique<CloseButton>(
      base::BindRepeating(&DesksTemplatesItemView::OnDeleteButtonPressed,
                          base::Unretained(this)),
      CloseButton::Type::kMedium));

  hover_container_->SetUseDefaultFillLayout(true);
  hover_container_->SetVisible(false);

  icon_container_view_->PopulateIconContainerFromTemplate(desk_template);
  icon_container_view_->SetVisible(true);
  card_container->SetFlexForView(spacer, 1);

  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);
  views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                kCornerRadius);
  views::FocusRing::Install(this);
  views::FocusRing* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetHasFocusPredicate([](views::View* view) {
    return static_cast<DesksTemplatesItemView*>(view)->IsViewHighlighted();
  });
  focus_ring->SetPathGenerator(
      std::make_unique<views::RoundRectHighlightPathGenerator>(
          gfx::Insets(-kFocusRingOffset), kCornerRadius + kFocusRingOffset));

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

DesksTemplatesItemView::~DesksTemplatesItemView() = default;

void DesksTemplatesItemView::UpdateHoverButtonsVisibility(
    const gfx::Point& screen_location,
    bool is_touch) {
  gfx::Point location_in_view = screen_location;
  ConvertPointFromScreen(this, &location_in_view);

  // For switch access, setting the hover buttons to visible allows users to
  // navigate to it.
  const bool visible =
      (is_touch && HitTestPoint(location_in_view)) ||
      (!is_touch && IsMouseHovered()) ||
      Shell::Get()->accessibility_controller()->IsSwitchAccessRunning();
  hover_container_->SetVisible(visible);
  icon_container_view_->SetVisible(!visible);
}

void DesksTemplatesItemView::Layout() {
  views::View::Layout();

  const gfx::Size delete_button_size = delete_button_->GetPreferredSize();
  DCHECK_EQ(delete_button_size.width(), delete_button_size.height());
  delete_button_->SetBoundsRect(
      gfx::Rect(width() - delete_button_size.width() - kDeleteButtonMargin,
                kDeleteButtonMargin, delete_button_size.width(),
                delete_button_size.height()));

  const gfx::Size launch_button_preferred_size =
      launch_button_->CalculatePreferredSize();
  launch_button_->SetBoundsRect(gfx::Rect(
      {(width() - launch_button_preferred_size.width()) / 2,
       height() - launch_button_preferred_size.height() - kVerticalPaddingDp},
      launch_button_preferred_size));
}

void DesksTemplatesItemView::OnThemeChanged() {
  views::View::OnThemeChanged();
  auto* color_provider = AshColorProvider::Get();
  const SkColor control_background_color_inactive =
      color_provider->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);

  GetBackground()->SetNativeControlColor(control_background_color_inactive);

  time_view_->SetBackgroundColor(control_background_color_inactive);
  time_view_->SetEnabledColor(color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));

  views::FocusRing::Get(this)->SetColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));
}

views::View* DesksTemplatesItemView::TargetForRect(views::View* root,
                                                   const gfx::Rect& rect) {
  // With the design of the template card having the textfield within a
  // clickable button, as well as having the grid view be a `PreTargetHandler`,
  // we needed to make `this` a `ViewTargeterDelegate` for the view event
  // targeter in order to allow the `name_view_` to be specifically targeted and
  // focused.
  if (root == this && name_view_->bounds().Contains(rect))
    return name_view_;
  return views::ViewTargeterDelegate::TargetForRect(root, rect);
}

void DesksTemplatesItemView::OnDeleteTemplate() {
  // Notify the highlight controller that we're going away.
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  DCHECK(overview_session);
  overview_session->highlight_controller()->OnViewDestroyingOrDisabling(this);

  DesksTemplatesPresenter::Get()->DeleteEntry(uuid_.AsLowercaseString());
}

void DesksTemplatesItemView::OnDeleteButtonPressed() {
  // Show the dialog to confirm the deletion.
  auto* dialog_controller = DesksTemplatesDialogController::Get();
  dialog_controller->ShowDeleteDialog(
      Shell::GetPrimaryRootWindow(), name_view_->GetAccessibleName(),
      base::BindOnce(&DesksTemplatesItemView::OnDeleteTemplate,
                     base::Unretained(this)));
}

void DesksTemplatesItemView::OnGridItemPressed() {
  DesksTemplatesPresenter::Get()->LaunchDeskTemplate(uuid_.AsLowercaseString());
}

views::View* DesksTemplatesItemView::GetView() {
  return this;
}

void DesksTemplatesItemView::MaybeActivateHighlightedView() {
  OnGridItemPressed();
}

void DesksTemplatesItemView::MaybeCloseHighlightedView() {
  OnDeleteButtonPressed();
}

void DesksTemplatesItemView::MaybeSwapHighlightedView(bool right) {}

void DesksTemplatesItemView::OnViewHighlighted() {
  views::FocusRing::Get(this)->SchedulePaint();
}

void DesksTemplatesItemView::OnViewUnhighlighted() {
  views::FocusRing::Get(this)->SchedulePaint();
}

BEGIN_METADATA(DesksTemplatesItemView, views::Button)
END_METADATA

}  // namespace ash
