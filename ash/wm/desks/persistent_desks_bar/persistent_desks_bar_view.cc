// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/persistent_desks_bar/persistent_desks_bar_view.h"

#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/persistent_desks_bar/persistent_desks_bar_button.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/flat_set.h"
#include "base/ranges/algorithm.h"
#include "ui/views/background.h"

namespace ash {

namespace {

constexpr int kDeskButtonWidth = 60;
constexpr int kDeskButtonHeight = 28;
constexpr int kDeskButtonSpacing = 8;
constexpr int kDeskButtonsY = 6;

constexpr int kOverviewButtonRightPadding = 6;
constexpr int kVerticalDotsButtonAndOverviewButtonSpacing = 8;

}  // namespace

PersistentDesksBarView::PersistentDesksBarView()
    : vertical_dots_button_(AddChildView(
          std::make_unique<PersistentDesksBarVerticalDotsButton>())),
      overview_button_(
          AddChildView(std::make_unique<PersistentDesksBarOverviewButton>())) {}

PersistentDesksBarView::~PersistentDesksBarView() = default;

void PersistentDesksBarView::RefreshDeskButtons() {
  base::flat_set<PersistentDesksBarDeskButton*> to_be_removed(
      desk_buttons_.begin(), desk_buttons_.end());
  auto* desk_controller = DesksController::Get();
  const auto& desks = desk_controller->desks();
  const size_t previous_desk_buttons_size = desk_buttons_.size();
  for (const auto& desk : desks) {
    const Desk* desk_ptr = desk.get();
    auto iter = base::ranges::find(to_be_removed, desk_ptr,
                                   &PersistentDesksBarDeskButton::desk);
    if (iter != to_be_removed.end()) {
      (*iter)->SetShouldPaintBackground(desk->is_active());
      (*iter)->UpdateText(desk->name());
      to_be_removed.erase(iter);
      continue;
    }

    desk_buttons_.push_back(
        AddChildView(std::make_unique<PersistentDesksBarDeskButton>(desk_ptr)));
  }

  if (!to_be_removed.empty()) {
    DCHECK_EQ(1u, to_be_removed.size());
    auto* to_be_removed_desk_button = *(to_be_removed.begin());
    base::Erase(desk_buttons_, to_be_removed_desk_button);
    RemoveChildViewT(to_be_removed_desk_button);
  }
  if (desks.size() != previous_desk_buttons_size)
    Layout();
}

void PersistentDesksBarView::Layout() {
  const int width = bounds().width();
  const int content_width =
      (desk_buttons_.size() + 1) * (kDeskButtonWidth + kDeskButtonSpacing) -
      kDeskButtonSpacing;
  int x = (width - content_width) / 2;
  for (auto* desk_button : desk_buttons_) {
    desk_button->SetBoundsRect(
        gfx::Rect(gfx::Point(x, kDeskButtonsY),
                  gfx::Size(kDeskButtonWidth, kDeskButtonHeight)));
    x += (kDeskButtonWidth + kDeskButtonSpacing);
  }

  const gfx::Size overview_button_size = overview_button_->GetPreferredSize();
  const int overview_button_x = bounds().right() -
                                overview_button_size.width() -
                                kOverviewButtonRightPadding;
  const int overview_button_y =
      (bounds().height() - overview_button_size.height()) / 2;
  overview_button_->SetBoundsRect(gfx::Rect(
      gfx::Point(overview_button_x, overview_button_y), overview_button_size));

  // `vertical_dots_button_` has the same size and y-axis position as
  // `overview_button_`.
  vertical_dots_button_->SetBoundsRect(
      gfx::Rect(gfx::Point(overview_button_x - overview_button_size.width() -
                               kVerticalDotsButtonAndOverviewButtonSpacing,
                           overview_button_y),
                overview_button_size));
}

void PersistentDesksBarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetBackground(
      views::CreateSolidBackground(AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kOpaque)));
}

}  // namespace ash
