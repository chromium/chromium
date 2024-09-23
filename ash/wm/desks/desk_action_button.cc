// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_action_button.h"

#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/overview/overview_grid.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

const gfx::VectorIcon* GetVectorIconForType(DeskActionButton::Type type) {
  switch (type) {
    case DeskActionButton::Type::kContextMenu:
      return &kThreeDotMoreIcon;
    case DeskActionButton::Type::kCloseDesk:
      return nullptr;
    case DeskActionButton::Type::kCombineDesk:
      return &kCombineDesksIcon;
  }
}

// Returns true if the desk controller exists and there is more than one desk.
bool CanRemoveDesks() {
  auto* desk_controller = DesksController::Get();
  return desk_controller && desk_controller->CanRemoveDesks();
}

// Returns true if the desk contains app windows or there are all desk windows.
bool CanCombineDesks(DeskActionView* action_view) {
  return action_view->mini_view()->desk()->ContainsAppWindows() ||
         !DesksController::Get()->visible_on_all_desks_windows().empty();
}

constexpr int kDeskCloseButtonSize = 24;

}  // namespace

DeskActionButton::DeskActionButton(const std::u16string& tooltip,
                                   Type type,
                                   base::RepeatingClosure pressed_callback,
                                   DeskActionView* desk_action_view)
    : CloseButton(pressed_callback,
                  CloseButton::Type::kMediumFloating,
                  GetVectorIconForType(type)),
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
  views::InstallCircleHighlightPathGenerator(this);
}

DeskActionButton::~DeskActionButton() {}

bool DeskActionButton::CanShow() const {
  DeskMiniView* mini_view = desk_action_view_->mini_view();
  switch (type_) {
    case DeskActionButton::Type::kContextMenu:
      // The context menu button will only show if the given desk should show
      // the save desk options, or the combine desk option should show.
      return saved_desk_util::ShouldShowSavedDesksOptionsForDesk(
                 mini_view->desk(), mini_view->owner_bar()) ||
             (CanRemoveDesks() && CanCombineDesks(desk_action_view_));
    case DeskActionButton::Type::kCloseDesk:
      return CanRemoveDesks();
    case DeskActionButton::Type::kCombineDesk:
      return CanRemoveDesks() && CanCombineDesks(desk_action_view_);
  }
}

void DeskActionButton::UpdateTooltip(const std::u16string& tooltip) {
  int message_id;
  switch (type_) {
    case DeskActionButton::Type::kContextMenu:
      message_id = IDS_ASH_DESKS_CONTEXT_MENU_DESCRIPTION;
      break;
    case DeskActionButton::Type::kCloseDesk:
      message_id = IDS_ASH_DESKS_CLOSE_ALL_DESCRIPTION;
      break;
    case DeskActionButton::Type::kCombineDesk:
      message_id = IDS_ASH_DESKS_COMBINE_DESKS_DESCRIPTION;
      break;
  }
  SetTooltipText(type_ == Type::kContextMenu
                     ? l10n_util::GetStringUTF16(message_id)
                     : l10n_util::GetStringFUTF16(message_id, tooltip));
}

BEGIN_METADATA(DeskActionButton)
END_METADATA

}  // namespace ash
