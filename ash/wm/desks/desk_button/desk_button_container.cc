// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_button/desk_button_container.h"

#include <vector>

#include "ash/public/cpp/desk_profiles_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/desk_button_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_button/desk_button.h"
#include "ash/wm/desks/desk_button/desk_switch_button.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/i18n/rtl.h"
#include "base/notreached.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

namespace ash {

DeskButtonContainer::DeskButtonContainer() = default;
DeskButtonContainer::~DeskButtonContainer() = default;

// static
bool DeskButtonContainer::ShouldShowDeskProfilesUi() {
  if (!chromeos::features::IsDeskProfilesEnabled()) {
    return false;
  }
  auto* desk_profiles_delegate = Shell::Get()->GetDeskProfilesDelegate();
  if (!desk_profiles_delegate ||
      desk_profiles_delegate->GetProfilesSnapshot().size() < 2u) {
    return false;
  }
  return true;
}

// static
int DeskButtonContainer::GetMaxLength(bool zero_state) {
  if (zero_state) {
    return kDeskButtonContainerHeightVertical;
  }
  if (ShouldShowDeskProfilesUi()) {
    return kDeskButtonContainerWidthHorizontalExpandedWithAvatar;
  }
  return kDeskButtonContainerWidthHorizontalExpandedNoAvatar;
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

gfx::Size DeskButtonContainer::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (zero_state_) {
    return {kDeskButtonContainerWidthVertical, GetPreferredLength()};
  }
  return {GetPreferredLength(), kDeskButtonContainerHeightHorizontal};
}

void DeskButtonContainer::Layout(PassKey) {
  if (!desk_button_widget_) {
    return;
  }

  if (zero_state_) {
    desk_button_->SetBoundsRect(
        gfx::Rect({kDeskButtonContainerInsetsVertical.left(),
                   kDeskButtonContainerInsetsVertical.top()},
                  desk_button_->GetPreferredSize()));
  } else {
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

  if (zero_state_) {
    len += kDeskButtonContainerInsetsVertical.height() +
           desk_button_->GetPreferredSize().height();
  } else {
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
  }

  return len;
}

bool DeskButtonContainer::IntersectsWithDeskButtonUi(
    const gfx::Point& screen_location) const {
  if (auto* widget = GetWidget(); widget && widget->IsVisible()) {
    for (const auto view : children()) {
      if (view->GetVisible() &&
          view->GetBoundsInScreen().Contains(screen_location)) {
        return true;
      }
    }
  }
  return false;
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
  NOTREACHED();
}

void DeskButtonContainer::Init(DeskButtonWidget* desk_button_widget) {
  CHECK(desk_button_widget);
  desk_button_widget_ = desk_button_widget;

  shelf_ = desk_button_widget_->shelf();
  CHECK(shelf_);

  zero_state_ = !shelf_->IsHorizontalAlignment();

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetFlipCanvasOnPaintForRTLUI(false);

  AddChildView(views::Builder<DeskButton>()
                   .CopyAddressTo(&desk_button_)
                   .Init(/*desk_button_container=*/this)
                   .Build());
  AddChildView(
      views::Builder<DeskSwitchButton>()
          .CopyAddressTo(&prev_desk_button_)
          .Init(/*desk_button_container=*/this, DeskSwitchButton::Type::kPrev)
          .SetVisible(!zero_state_)
          .Build());
  AddChildView(
      views::Builder<DeskSwitchButton>()
          .CopyAddressTo(&next_desk_button_)
          .Init(/*desk_button_container=*/this, DeskSwitchButton::Type::kNext)
          .SetVisible(!zero_state_)
          .Build());

  desks_observation_.Observe(DesksController::Get());
  session_observer_.Observe(SessionController::Get());
}

void DeskButtonContainer::UpdateUi(const Desk* active_desk) {
  SetBackground(zero_state_ ? nullptr
                            : views::CreateThemedRoundedRectBackground(
                                  cros_tokens::kCrosSysSystemOnBase,
                                  kDeskButtonContainerCornerRadius));
  desk_button_->SetZeroState(zero_state_);
  desk_button_->UpdateUi(active_desk);
  prev_desk_button_->UpdateUi(active_desk);
  next_desk_button_->UpdateUi(active_desk);
}

void DeskButtonContainer::UpdateUiAndLayoutIfNeeded(const Desk* active_desk) {
  gfx::Size old_preferred_size = GetPreferredSize();

  UpdateUi(active_desk);

  if (GetPreferredSize() != old_preferred_size) {
    desk_button_widget_->delegate_view()->InvalidateLayout();
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
    if (event->type() == ui::EventType::kGestureLongPress) {
      source_type = ui::MenuSourceType::MENU_SOURCE_LONG_PRESS;
    } else if (event->type() == ui::EventType::kGestureLongTap) {
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
