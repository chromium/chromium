// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_TEXTFIELD_H_
#define ASH_WM_DESKS_DESK_TEXTFIELD_H_

#include "ash/ash_export.h"
#include "ash/style/system_textfield.h"
#include "ash/wm/overview/overview_focusable_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

// Defines a textfield styled so when it's not focused, it looks like a normal
// label. It can be focused and activated by the `OverviewFocusCycler`.
class ASH_EXPORT DeskTextfield : public SystemTextfield,
                                 public OverviewFocusableView {
 public:
  METADATA_HEADER(DeskTextfield);

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

  void set_use_default_focus_manager(bool use_default_focus_manager) {
    use_default_focus_manager_ = use_default_focus_manager;
  }

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;

  // OverviewFocusableView:
  views::View* GetView() override;
  void MaybeActivateFocusedView() override;
  void MaybeCloseFocusedView(bool primary_action) override;
  void MaybeSwapFocusedView(bool right) override;
  void OnFocusableViewFocused() override;
  void OnFocusableViewBlurred() override;

 private:
  bool use_default_focus_manager_ = false;
};

BEGIN_VIEW_BUILDER(/* no export */, DeskTextfield, views::Textfield)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DeskTextfield)

#endif  // ASH_WM_DESKS_DESK_TEXTFIELD_H_
