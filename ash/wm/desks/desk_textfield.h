// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_TEXTFIELD_H_
#define ASH_WM_DESKS_DESK_TEXTFIELD_H_

#include "ash/ash_export.h"
#include "ash/style/system_textfield.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// Defines a textfield styled so when it's not focused, it looks like a normal
// label.
class ASH_EXPORT DeskTextfield : public SystemTextfield {
  METADATA_HEADER(DeskTextfield, SystemTextfield)

 public:
  DeskTextfield();
  explicit DeskTextfield(Type type);
  DeskTextfield(const DeskTextfield&) = delete;
  DeskTextfield& operator=(const DeskTextfield&) = delete;
  ~DeskTextfield() override;

  // The max number of characters (UTF-16) allowed for the textfield.
  static constexpr size_t kMaxLength = 300;

  // Commits an on-going name change (if any) by blurring the focus away from
  // any view on `widget`, where `widget` should be the saved desk library
  // widget or the desk bar widget.
  static void CommitChanges(views::Widget* widget);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
};

BEGIN_VIEW_BUILDER(/* no export */, DeskTextfield, views::Textfield)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DeskTextfield)

#endif  // ASH_WM_DESKS_DESK_TEXTFIELD_H_
