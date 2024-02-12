// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_button/desk_button_container.h"

#include "ash/public/cpp/desk_profiles_delegate.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/screen_util.h"
#include "ash/shelf/desk_button_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_button/desk_button.h"
#include "ash/wm/desks/desk_button/desk_switch_button.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

DeskButtonContainer::DeskButtonContainer() = default;
DeskButtonContainer::~DeskButtonContainer() = default;

// static
bool DeskButtonContainer::ShouldShowDeskProfilesUi() {
  if (auto* desk_profiles_delegate = Shell::Get()->GetDeskProfilesDelegate()) {
    return desk_profiles_delegate->GetProfilesSnapshot().size() > 1u;
  }
  return false;
}

// static
int DeskButtonContainer::GetMaxLength(bool horizontal_shelf, bool zero_state) {
  if (horizontal_shelf) {
    if (ShouldShowDeskProfilesUi()) {
      return zero_state ? kDeskButtonContainerWidthHorizontalZeroWithAvatar
                        : kDeskButtonContainerWidthHorizontalExpandedWithAvatar;
    }
    return zero_state ? kDeskButtonContainerWidthHorizontalZeroNoAvatar
                      : kDeskButtonContainerWidthHorizontalExpandedNoAvatar;
  }
  return kDeskButtonContainerHeightVertical;
}

void DeskButtonContainer::OnProfileUpsert(const LacrosProfileSummary& summary) {
  UpdateUiAndLayoutIfNeeded(DesksController::Get()->active_desk());
}

void DeskButtonContainer::OnProfileRemoved(uint64_t profile_id) {
  UpdateUiAndLayoutIfNeeded(DesksController::Get()->active_desk());
}

void DeskButtonContainer::OnFirstSessionStarted() {
  // The desk profiles delegate will be available if lacros and desk profiles
  // are both enabled.
  desk_profiles_observer_.Reset();
  if (auto* delegate = Shell::Get()->GetDeskProfilesDelegate()) {
    desk_profiles_observer_.Observe(delegate);
  }
}

gfx::Size DeskButtonContainer::CalculatePreferredSize() const {
  if (IsHorizontalShelf()) {
    return {GetPreferredLength(), kDeskButtonContainerHeightHorizontal};
  }
  return {kDeskButtonContainerWidthVertical, GetPreferredLength()};
}

void DeskButtonContainer::Layout(PassKey) {
  if (!desk_button_widget_) {
    return;
  }

  if (IsHorizontalShelf()) {
    auto get_spacing = [&](views::View* view1, views::View* view2) {
      if ((view1 == prev_desk_button_ && view2 == next_desk_button_) ||
          (view1 == next_desk_button_ && view2 == prev_desk_button_)) {
        return kDeskButtonSwitchButtonSpacing;
      }
      return kDeskButtonContainerChildSpacingHorizontal;
    };

    std::vector<views::View*> views_to_layout;
    for (auto child : children()) {
      if (child->GetVisible()) {
        views_to_layout.emplace_back(child);
      }
    }

    if (base::i18n::IsRTL()) {
      std::reverse(views_to_layout.begin(), views_to_layout.end());
    }

    int x = kDeskButtonContainerInsetsHorizontal.left();
    const int y = kDeskButtonContainerInsetsHorizontal.top();
    for (size_t i = 0; i < views_to_layout.size(); i++) {
      if (i) {
        x += get_spacing(views_to_layout[i - 1], views_to_layout[i]);
      }
      views_to_layout[i]->SetBoundsRect(
          gfx::Rect({x, y}, views_to_layout[i]->GetPreferredSize()));
      x += views_to_layout[i]->GetPreferredSize().width();
    }
  } else {
    desk_button_->SetBoundsRect(
        gfx::Rect({kDeskButtonContainerInsetsVertical.left(),
                   kDeskButtonContainerInsetsVertical.top()},
                  desk_button_->GetPreferredSize()));
  }
}

void DeskButtonContainer::OnDeskAdded(const Desk* desk, bool from_undo) {
  UpdateUiAndLayoutIfNeeded(DesksController::Get()->active_desk());
}

void DeskButtonContainer::OnDeskRemoved(const Desk* desk) {
  UpdateUiAndLayoutIfNeeded(DesksController::Get()->active_desk());
}

void DeskButtonContainer::OnDeskReordered(int old_index, int new_index) {
  UpdateUiAndLayoutIfNeeded(DesksController::Get()->active_desk());
}

void DeskButtonContainer::OnDeskActivationChanged(const Desk* activated,
                                                  const Desk* deactivated) {
  UpdateUiAndLayoutIfNeeded(activated);
}

void DeskButtonContainer::OnDeskNameChanged(const Desk* desk,
                                            const std::u16string& new_name) {
  if (!desk->is_active()) {
    return;
  }

  UpdateUi(desk);
}

void DeskButtonContainer::PrepareForAlignmentChange() {
  UpdateUiAndLayoutIfNeeded(DesksController::Get()->active_desk());
}

int DeskButtonContainer::GetPreferredLength() const {
  int len = 0;

  if (IsHorizontalShelf()) {
    len += kDeskButtonContainerInsetsHorizontal.left() +
           desk_button_->GetPreferredSize().width();
    if (prev_desk_button_->GetVisible() && next_desk_button_->GetVisible()) {
      len += kDeskButtonContainerChildSpacingHorizontal +
             prev_desk_button_->GetPreferredSize().width() +
             kDeskButtonSwitchButtonSpacing +
             prev_desk_button_->GetPreferredSize().width();
    } else if (prev_desk_button_->GetVisible()) {
      len += kDeskButtonContainerChildSpacingHorizontal +
             prev_desk_button_->GetPreferredSize().width();
    } else if (next_desk_button_->GetVisible()) {
      len += kDeskButtonContainerChildSpacingHorizontal +
             next_desk_button_->GetPreferredSize().width();
    }
    len += kDeskButtonContainerInsetsHorizontal.right();
  } else {
    len += kDeskButtonContainerInsetsVertical.height() +
           desk_button_->GetPreferredSize().height();
  }

  return len;
}

std::u16string DeskButtonContainer::GetTitleForView(
    const views::View* view) const {
  if (view == desk_button_) {
    return desk_button_->GetTitle();
  } else if (view == prev_desk_button_) {
    return prev_desk_button_->GetTitle();
  } else if (view == next_desk_button_) {
    return next_desk_button_->GetTitle();
  }
  NOTREACHED_NORETURN();
}

bool DeskButtonContainer::IsHorizontalShelf() const {
  return shelf_->IsHorizontalAlignment();
}

bool DeskButtonContainer::IsForcedZeroState() const {
  if (gfx::NativeWindow native_window = GetWidget()->GetNativeWindow()) {
    return screen_util::GetDisplayBoundsWithShelf(native_window).width() <
           kDeskButtonLargeDisplayThreshold;
  }
  return false;
}

void DeskButtonContainer::Init(DeskButtonWidget* desk_button_widget) {
  CHECK(desk_button_widget);
  desk_button_widget_ = desk_button_widget;

  shelf_ = desk_button_widget_->shelf();
  CHECK(shelf_);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetFlipCanvasOnPaintForRTLUI(false);

  SetBackground(IsHorizontalShelf() ? views::CreateThemedRoundedRectBackground(
                                          cros_tokens::kCrosSysSystemOnBase,
                                          kDeskButtonContainerCornerRadius)
                                    : nullptr);

  AddChildView(views::Builder<DeskButton>()
                   .CopyAddressTo(&desk_button_)
                   .Init(/*desk_button_container=*/this)
                   .Build());
  AddChildView(
      views::Builder<DeskSwitchButton>()
          .CopyAddressTo(&prev_desk_button_)
          .Init(/*desk_button_container=*/this, DeskSwitchButton::Type::kPrev)
          .SetVisible(IsHorizontalShelf())
          .Build());
  AddChildView(
      views::Builder<DeskSwitchButton>()
          .CopyAddressTo(&next_desk_button_)
          .Init(/*desk_button_container=*/this, DeskSwitchButton::Type::kNext)
          .SetVisible(IsHorizontalShelf())
          .Build());

  desks_observation_.Observe(DesksController::Get());
  session_observer_.Observe(SessionController::Get());
}

void DeskButtonContainer::UpdateUi(const Desk* active_desk) {
  desk_button_->set_zero_state(zero_state_);
  desk_button_->UpdateUi(active_desk);
  prev_desk_button_->UpdateUi(active_desk);
  next_desk_button_->UpdateUi(active_desk);
}

void DeskButtonContainer::UpdateUiAndLayoutIfNeeded(const Desk* active_desk) {
  gfx::Size old_preferred_size = GetPreferredSize();

  UpdateUi(active_desk);

  if (GetPreferredSize() != old_preferred_size) {
    desk_button_widget_->delegate_view()->DeprecatedLayoutImmediately();
  }
}
void DeskButtonContainer::HandleLocaleChange() {
  desk_button_->UpdateLocaleSpecificSettings();
  prev_desk_button_->UpdateLocaleSpecificSettings();
  next_desk_button_->UpdateLocaleSpecificSettings();
}

void DeskButtonContainer::MaybeShowContextMenu(views::View* source,
                                               ui::LocatedEvent* event) {
  if (!desk_button_->is_activated()) {
    ui::MenuSourceType source_type = ui::MenuSourceType::MENU_SOURCE_MOUSE;
    if (event->type() == ui::ET_GESTURE_LONG_PRESS) {
      source_type = ui::MenuSourceType::MENU_SOURCE_LONG_PRESS;
    } else if (event->type() == ui::ET_GESTURE_LONG_TAP) {
      source_type = ui::MenuSourceType::MENU_SOURCE_LONG_TAP;
    }
    gfx::Point location_in_screen(event->location());
    View::ConvertPointToScreen(source, &location_in_screen);
    source->ShowContextMenu(location_in_screen, source_type);
  }

  event->SetHandled();
  event->StopPropagation();
}

BEGIN_METADATA(DeskButtonContainer)
END_METADATA

}  // namespace ash
