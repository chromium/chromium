// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_textfield.h"

#include "ash/shell.h"
#include "ash/style/style_util.h"
#include "ash/wm/overview/overview_grid.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kDeskTextfieldMinHeight = 16;

}  // namespace

DeskTextfield::DeskTextfield(Type type) : SystemTextfield(type) {
  views::Builder<DeskTextfield>(this).SetCursorEnabled(true).BuildChildren();

  GetRenderText()->SetElideBehavior(gfx::ELIDE_TAIL);
}

DeskTextfield::~DeskTextfield() = default;

// static
constexpr size_t DeskTextfield::kMaxLength;

// static
void DeskTextfield::CommitChanges(views::Widget* widget) {
  auto* focus_manager = widget->GetFocusManager();
  focus_manager->ClearFocus();
  // Avoid having the focus restored to the same view when the parent view is
  // refocused.
  focus_manager->SetStoredFocusView(nullptr);
}

gfx::Size DeskTextfield::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const std::u16string& text = GetText();
  int width = 0;
  int height = 0;
  gfx::Canvas::SizeStringInt(text, GetFontList(), &width, &height, 0,
                             gfx::Canvas::NO_ELLIPSIS);
  gfx::Size size{width + GetCaretBounds().width(), height};
  const auto insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  size.SetToMax(gfx::Size(0, kDeskTextfieldMinHeight));
  return size;
}

bool DeskTextfield::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  // The default behavior of the tab key is that it moves the focus to the next
  // available view. This is done in either in `OverviewSession::OnKeyEvent()`
  // or `DeskBarController::OnKeyEvent()`.
  return event.key_code() == ui::VKEY_TAB;
}

std::u16string DeskTextfield::GetTooltipText(const gfx::Point& p) const {
  return GetPreferredSize().width() > width() ? GetText() : std::u16string();
}

ui::Cursor DeskTextfield::GetCursor(const ui::MouseEvent& event) {
  return ui::mojom::CursorType::kIBeam;
}

void DeskTextfield::OnFocus() {
  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);
  SystemTextfield::OnFocus();
}

void DeskTextfield::OnBlur() {
  GetRenderText()->SetElideBehavior(gfx::ELIDE_TAIL);
  SystemTextfield::OnBlur();

  // Avoid having the focus restored to the same DeskNameView when the desk bar
  // widget is refocused. Use a post task to avoid calling
  // `FocusManager::SetStoredFocusView()` while `FocusManager::ClearFocus()` is
  // still being activated. In this case, we want to set the stored focus view
  // to nullptr after the stack of the call to `FocusManager::ClearFocus()`
  // returns completely.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<views::Widget> w) {
                       if (w) {
                         w->GetFocusManager()->SetStoredFocusView(nullptr);
                       }
                     },
                     GetWidget()->GetWeakPtr()));
}

void DeskTextfield::OnDragEntered(const ui::DropTargetEvent& event) {
  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);
  views::Textfield::OnDragEntered(event);
}

void DeskTextfield::OnDragExited() {
  GetRenderText()->SetElideBehavior(gfx::ELIDE_TAIL);
  views::Textfield::OnDragExited();
}

BEGIN_METADATA(DeskTextfield)
END_METADATA

}  // namespace ash
