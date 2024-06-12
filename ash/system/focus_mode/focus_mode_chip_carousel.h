// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CHIP_CAROUSEL_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CHIP_CAROUSEL_H_

#include "ash/ash_export.h"
#include "ash/system/focus_mode/focus_mode_tasks_provider.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class FlexLayoutView;
class ImageButton;
}  // namespace views

namespace ash {

namespace {
class ChipCarouselScrollView;
}

// A horizontal scroll bar of chips for tasks. Selecting a task chip will save
// it as the currently selected task for the focus session.
class ASH_EXPORT FocusModeChipCarousel : public views::BoxLayoutView {
  METADATA_HEADER(FocusModeChipCarousel, views::BoxLayoutView)
 public:
  // Called when a task chip is pressed, contains a task pointer that is alive
  // for the lifetime of the task chip.
  using ChipPressedCallback =
      base::RepeatingCallback<void(const FocusModeTask& task)>;

  explicit FocusModeChipCarousel(ChipPressedCallback on_chip_pressed);
  FocusModeChipCarousel(const FocusModeChipCarousel&) = delete;
  FocusModeChipCarousel& operator=(const FocusModeChipCarousel&) = delete;
  ~FocusModeChipCarousel() override;

  // TODO(b/305085993): Update setting logic once API is integrated.
  // Updates the carousel of task chips from the first 5 tasks in `tasks` and
  // scrolls the carousel back to the beginning.
  void SetTasks(const std::vector<FocusModeTask>& tasks);

  // Returns whether the carousel is currently displaying any tasks.
  bool HasTasks() const;
  // Returns the number of tasks the carousel is currently showing.
  int GetTaskCountForTesting() const;

  views::ScrollView* GetScrollViewForTesting() const;

  // views::View:
  void Layout(PassKey) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

 private:
  friend class FocusModeChipCarouselTest;

  // Adds or updates the linear gradient on the overflown sides of the carousel.
  void UpdateGradient();

  // Removes the gradient from the `scroll_view_`.
  void RemoveGradient();

  // Scrolls the task chips left if `left` and right otherwise.
  void OnChevronPressed(bool left);

  // Scrolls `scroll_view_` until `chip` is fully in view and aligned with the
  // left side of the non-masked region of the `scroll_view_`. This does not
  // scroll past the end of the carousel.
  void ScrollToChip(views::View* chip);

  raw_ptr<ChipCarouselScrollView> scroll_view_ = nullptr;
  // This view contains the chips for the chip carousel.
  raw_ptr<views::FlexLayoutView> scroll_contents_ = nullptr;
  // The callback to run when a task chip is pressed.
  const ChipPressedCallback on_chip_pressed_;
  // The overflow chevrons.
  raw_ptr<views::ImageButton> left_overflow_icon_ = nullptr;
  raw_ptr<views::ImageButton> right_overflow_icon_ = nullptr;
  // Scroll callbacks are used so we can update chevron and gradient visibility.
  base::CallbackListSubscription on_contents_scrolled_subscription_;
  base::CallbackListSubscription on_contents_scroll_ended_subscription_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CHIP_CAROUSEL_H_
