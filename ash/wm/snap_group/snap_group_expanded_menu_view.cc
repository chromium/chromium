// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group_expanded_menu_view.h"

#include <memory>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr auto kExpandedMenuPadding = gfx::Insets::VH(8, 3);
constexpr int kSpaceBetweenButton = 3;

SplitViewController* split_view_controller() {
  return SplitViewController::Get(Shell::GetPrimaryRootWindow());
}

}  // namespace

SnapGroupExpandedMenuView::SnapGroupExpandedMenuView(SnapGroup* snap_group)
    : snap_group_(snap_group),
      swap_windows_button_(AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(
              &SnapGroupExpandedMenuView::OnSwapWindowsButtonPressed,
              base::Unretained(this)),
          IconButton::Type::kMediumFloating,
          &kSnapGroupSwapWindowsIcon,
          IDS_ASH_SNAP_GROUP_SWAP_WINDOWS,
          /*is_togglable=*/false,
          /*has_border=*/true))),
      update_primary_window_button_(AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(
              &SnapGroupExpandedMenuView::OnUpdatePrimaryWindowButtonPressed,
              base::Unretained(this)),
          IconButton::Type::kMediumFloating,
          &kSnapGroupUpdatePrimaryWindowIcon,
          IDS_ASH_SNAP_GROUP_UPDATE_LEFT_WINDOW,
          /*is_togglable=*/false,
          /*has_border=*/true))),
      update_secondary_window_button_(AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(
              &SnapGroupExpandedMenuView::OnUpdateSecondaryWindowButtonPressed,
              base::Unretained(this)),
          IconButton::Type::kMediumFloating,
          &kSnapGroupUpdateSecondaryWindowIcon,
          IDS_ASH_SNAP_GROUP_UPDATE_RIGHT_WINDOW,
          /*is_togglable=*/false,
          /*has_border=*/true))),
      unlock_button_(AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(&SnapGroupExpandedMenuView::OnUnLockButtonPressed,
                              base::Unretained(this)),
          IconButton::Type::kMediumFloating,
          &kLockScreenEasyUnlockOpenIcon,
          IDS_ASH_SNAP_GROUP_CLICK_TO_UNLOCK_WINDOWS,
          /*is_togglable=*/false,
          /*has_border=*/true))) {
  SetPaintToLayer();
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kExpandedMenuRoundedCornerRadius));

  auto* box_layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kExpandedMenuPadding,
      kSpaceBetweenButton));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
}

SnapGroupExpandedMenuView::~SnapGroupExpandedMenuView() = default;

void SnapGroupExpandedMenuView::OnUpdatePrimaryWindowButtonPressed() {
  split_view_controller()->OpenOverviewOnTheOtherSideOfTheScreen(
      SplitViewController::SnapPosition::kSecondary);
}

void SnapGroupExpandedMenuView::OnUpdateSecondaryWindowButtonPressed() {
  split_view_controller()->OpenOverviewOnTheOtherSideOfTheScreen(
      SplitViewController::SnapPosition::kPrimary);
}

void SnapGroupExpandedMenuView::OnSwapWindowsButtonPressed() {
  split_view_controller()->SwapWindows(
      SplitViewController::SwapWindowsSource::kSnapGroupSwapWindowsButton);
}

void SnapGroupExpandedMenuView::OnUnLockButtonPressed() {
  Shell::Get()->snap_group_controller()->RemoveSnapGroup(snap_group_);
  // `this` will be deleted after this line.
}

BEGIN_METADATA(SnapGroupExpandedMenuView, views::View)
END_METADATA

}  // namespace ash
