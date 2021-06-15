// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/persistent_desks_bar_view.h"

#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/persistent_desks_bar_circular_button.h"
#include "ash/wm/desks/zero_state_button.h"
#include "base/containers/flat_set.h"
#include "base/stl_util.h"
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

// -----------------------------------------------------------------------------
// PersistentDesksBarDeskButton:

// The button with the desk's name inside the PersistentDesksBarView.
class PersistentDesksBarDeskButton : public DeskButtonBase {
 public:
  explicit PersistentDesksBarDeskButton(const Desk* desk)
      : DeskButtonBase(desk->name()), desk_(desk) {
    // Only paint the background of the active desk's button.
    SetShouldPaintBackground(desk_ == DesksController::Get()->active_desk());
  }
  PersistentDesksBarDeskButton(const PersistentDesksBarDeskButton&) = delete;
  PersistentDesksBarDeskButton& operator=(const PersistentDesksBarDeskButton) =
      delete;
  ~PersistentDesksBarDeskButton() override = default;

  const Desk* desk() const { return desk_; }

 private:
  // DeskButtonBase:
  void OnButtonPressed() override {
    DesksController::Get()->ActivateDesk(
        desk_, DesksSwitchSource::kPersistentDesksBar);
  }

  void OnThemeChanged() override {
    DeskButtonBase::OnThemeChanged();
    SetEnabledTextColors(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  }

  void OnMouseEntered(const ui::MouseEvent& event) override {
    SetShouldPaintBackground(true);
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    SetShouldPaintBackground(desk_ == DesksController::Get()->active_desk());
  }

  const Desk* desk_;
};

// -----------------------------------------------------------------------------
// PersistentDesksBarView:

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
    auto iter =
        std::find_if(to_be_removed.begin(), to_be_removed.end(),
                     [desk_ptr](PersistentDesksBarDeskButton* desk_button) {
                       return desk_ptr == desk_button->desk();
                     });
    if (iter != to_be_removed.end()) {
      (*iter)->SetShouldPaintBackground(desk->is_active());
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

const std::vector<std::u16string>
PersistentDesksBarView::GetDeskButtonsTextForTesting() const {
  std::vector<std::u16string> desk_buttons_text;
  for (auto* desk_button : desk_buttons_)
    desk_buttons_text.push_back(desk_button->desk()->name());
  return desk_buttons_text;
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
