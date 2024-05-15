// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_action_button.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr int kDeskCloseButtonSize = 24;

}  // namespace

DeskActionButton::DeskActionButton(const std::u16string& tooltip,
                                   Type type,
                                   base::RepeatingClosure pressed_callback,
                                   DeskActionView* desk_action_view)
    : CloseButton(pressed_callback,
                  CloseButton::Type::kMediumFloating,
                  type == Type::kCombineDesk ? &kCombineDesksIcon : nullptr),
      type_(type),
      pressed_callback_(std::move(pressed_callback)),
      desk_action_view_(desk_action_view) {
  SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  SetPreferredSize(gfx::Size(kDeskCloseButtonSize, kDeskCloseButtonSize));
  UpdateTooltip(tooltip);

  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  focus_ring->SetPathGenerator(
      std::make_unique<views::CircleHighlightPathGenerator>(
          -gfx::Insets(focus_ring->GetHaloThickness() / 2)));
  if (desk_action_view_->mini_view()->owner_bar()->type() ==
          DeskBarViewBase::Type::kOverview &&
      !features::IsOverviewNewFocusEnabled()) {
    focus_ring->SetHasFocusPredicate(
        base::BindRepeating([](const views::View* view) {
          const auto* v = views::AsViewClass<DeskActionButton>(view);
          CHECK(v);
          return v->is_focused();
        }));
  }
  views::InstallCircleHighlightPathGenerator(this);
}

DeskActionButton::~DeskActionButton() {}

views::View* DeskActionButton::GetView() {
  return this;
}

void DeskActionButton::MaybeActivateFocusedView() {
  pressed_callback_.Run();
}

void DeskActionButton::MaybeCloseFocusedView(bool primary_action) {}

void DeskActionButton::MaybeSwapFocusedView(bool right) {}

void DeskActionButton::OnFocusableViewFocused() {
  desk_action_view_->OnFocusChange();
  views::FocusRing::Get(this)->SchedulePaint();
}

void DeskActionButton::OnFocusableViewBlurred() {
  desk_action_view_->OnFocusChange();
  views::FocusRing::Get(this)->SchedulePaint();
}

bool DeskActionButton::CanShow() const {
  // The button should not show if there is only one desk.
  auto* desk_controller = DesksController::Get();
  if (!desk_controller || !desk_controller->CanRemoveDesks()) {
    return false;
  }

  // The close desk button can always show, while the combine desk button shows
  // when its desk contains app windows or there are all desk windows.
  return type_ == Type::kCloseDesk ||
         desk_action_view_->mini_view()->desk()->ContainsAppWindows() ||
         !desk_controller->visible_on_all_desks_windows().empty();
}

void DeskActionButton::UpdateTooltip(const std::u16string& tooltip) {
  SetTooltipText(l10n_util::GetStringFUTF16(
      type_ == Type::kCombineDesk ? IDS_ASH_DESKS_COMBINE_DESKS_DESCRIPTION
                                  : IDS_ASH_DESKS_CLOSE_ALL_DESCRIPTION,
      tooltip));
}

BEGIN_METADATA(DeskActionButton)
END_METADATA

}  // namespace ash
