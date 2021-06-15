// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_PERSISTENT_DESKS_BAR_CIRCULAR_BUTTON_H_
#define ASH_WM_DESKS_PERSISTENT_DESKS_BAR_CIRCULAR_BUTTON_H_

#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

class PersistentDesksBarContextMenu;

// The base class of the circular buttons inside the persistent desks bar.
// Including PersistentDesksBarVerticalDotsButton and
// PersistentDesksBarOverviewButton.
class PersistentDesksBarCircularButton : public views::ImageButton {
 public:
  explicit PersistentDesksBarCircularButton(const gfx::VectorIcon& icon);
  PersistentDesksBarCircularButton(const PersistentDesksBarCircularButton&) =
      delete;
  PersistentDesksBarCircularButton& operator=(
      const PersistentDesksBarCircularButton&) = delete;
  ~PersistentDesksBarCircularButton() override = default;

  // views::ImageButton:
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

 private:
  virtual void OnButtonPressed() = 0;

  const gfx::VectorIcon& icon_;
};

// A circular button inside the persistent desks bar or the desks bar in
// overview mode with kPersistentDesksBarVerticalDotsIcon. Clicking the button
// will open a context menu that can be used to send feedback or show/hide the
// persistent desks bar.
class PersistentDesksBarVerticalDotsButton
    : public PersistentDesksBarCircularButton {
 public:
  PersistentDesksBarVerticalDotsButton();
  PersistentDesksBarVerticalDotsButton(
      const PersistentDesksBarVerticalDotsButton&) = delete;
  PersistentDesksBarVerticalDotsButton& operator=(
      const PersistentDesksBarVerticalDotsButton&);
  ~PersistentDesksBarVerticalDotsButton() override;

  PersistentDesksBarContextMenu* GetContextMenuForTesting() const {
    return context_menu_.get();
  }

 private:
  // PersistentDesksBarCircularButton:
  void OnButtonPressed() override;

  void OnMenuClosed();

  std::unique_ptr<PersistentDesksBarContextMenu> context_menu_;
};

// A circular button inside the persistent desks bar with
// kPersistentDesksBarChevronDownIcon. Clicking the button will destroy the bar
// and enter overview mode.
class PersistentDesksBarOverviewButton
    : public PersistentDesksBarCircularButton {
 public:
  PersistentDesksBarOverviewButton();
  PersistentDesksBarOverviewButton(const PersistentDesksBarOverviewButton&) =
      delete;
  const PersistentDesksBarOverviewButton& operator=(
      const PersistentDesksBarOverviewButton&) = delete;
  ~PersistentDesksBarOverviewButton() override;

 private:
  // PersistentDesksBarCircularButton:
  void OnButtonPressed() override;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_PERSISTENT_DESKS_BAR_CIRCULAR_BUTTON_H_
