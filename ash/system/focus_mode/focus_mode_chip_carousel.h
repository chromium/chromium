// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CHIP_CAROUSEL_H_
#define ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CHIP_CAROUSEL_H_

#include "ash/ash_export.h"
#include "ui/views/controls/scroll_view.h"

namespace views {
class BoxLayoutView;
}

namespace ash {

// A horizontal scroll bar of chips for tasks. Selecting a task chip will save
// it as the currently selected task for the focus session.
class ASH_EXPORT FocusModeChipCarousel : public views::ScrollView {
 public:
  // Called when a task chip is pressed, contains a task name string.
  using ChipPressedCallback =
      base::RepeatingCallback<void(const std::u16string&)>;

  FocusModeChipCarousel(ChipPressedCallback on_chip_pressed);
  FocusModeChipCarousel(const FocusModeChipCarousel&) = delete;
  FocusModeChipCarousel& operator=(const FocusModeChipCarousel&) = delete;
  ~FocusModeChipCarousel() override;

  // Updates the carousel of task chips from the first 5 tasks in `tasks`.
  // TODO(b/305085993): Update task data representation once API is integrated.
  void SetTasks(const std::vector<std::u16string>& tasks);

  // Returns whether the carousel is currently displaying any tasks.
  bool HasTasks() const;

 private:
  // This view contains the chips for the chip carousel.
  raw_ptr<views::BoxLayoutView> scroll_contents_ = nullptr;
  const ChipPressedCallback on_chip_pressed_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_FOCUS_MODE_FOCUS_MODE_CHIP_CAROUSEL_H_
