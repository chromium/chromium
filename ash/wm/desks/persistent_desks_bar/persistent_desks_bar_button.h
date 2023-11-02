// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_PERSISTENT_DESKS_BAR_BUTTON_H_
#define ASH_WM_DESKS_PERSISTENT_DESKS_BAR_BUTTON_H_

#include <string>

#include "ash/wm/desks/zero_state_button.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

class Desk;
class PersistentDesksBarContextMenu;

// The button with the desk's name inside the PersistentDesksBarView.
class PersistentDesksBarDeskButton : public DeskButtonBase {
 public:
  explicit PersistentDesksBarDeskButton(const Desk* desk);
  PersistentDesksBarDeskButton(const PersistentDesksBarDeskButton&) = delete;
  PersistentDesksBarDeskButton& operator=(const PersistentDesksBarDeskButton) =
      delete;
  ~PersistentDesksBarDeskButton() override = default;

  const Desk* desk() const { return desk_; }
  void UpdateText(std::u16string name);

 private:
  // DeskButtonBase:
  const char* GetClassName() const override;
  void OnButtonPressed() override;
  void OnThemeChanged() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  const Desk* desk_;
};

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
  const char* GetClassName() const override;
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

 private:
  friend class DesksTestApi;

  // PersistentDesksBarCircularButton:
  const char* GetClassName() const override;
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
  const char* GetClassName() const override;
  void OnButtonPressed() override;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_PERSISTENT_DESKS_BAR_BUTTON_H_
