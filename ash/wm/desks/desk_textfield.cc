// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_textfield.h"

#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The border radius on the text field.
constexpr int kDeskTextfieldBorderRadius = 4;

constexpr int kDeskTextfieldMinHeight = 16;

}  // namespace

DeskTextfield::DeskTextfield() {
  views::Builder<DeskTextfield>(this)
      .SetBorder(nullptr)
      .SetCursorEnabled(true)
      .BuildChildren();

  views::FocusRing* focus_ring =
      StyleUtil::SetUpFocusRingForView(this, kFocusRingHaloInset);
  focus_ring->SetHasFocusPredicate([](views::View* view) {
    return static_cast<DeskTextfield*>(view)->IsViewHighlighted() ||
           view->HasFocus();
  });
  focus_ring->SetColorId(ui::kColorAshFocusRing);

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

gfx::Size DeskTextfield::CalculatePreferredSize() const {
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

void DeskTextfield::SetBorder(std::unique_ptr<views::Border> b) {
  // `views::Textfield` override of `SetBorder()` removes an installed focus
  // ring, which we want to keep.
  views::View::SetBorder(std::move(b));
}

bool DeskTextfield::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  // The default behavior of the tab key is that it moves the focus to the next
  // available view.
  // We want that to be handled by OverviewHighlightController as part of moving
  // the highlight forward or backward when tab or shift+tab are pressed.
  return event.key_code() == ui::VKEY_TAB;
}

std::u16string DeskTextfield::GetTooltipText(const gfx::Point& p) const {
  return GetPreferredSize().width() > width() ? GetText() : std::u16string();
}

void DeskTextfield::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Textfield::GetAccessibleNodeData(node_data);
  node_data->SetNameChecked(GetAccessibleName());
}

void DeskTextfield::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateViewAppearance();
}

void DeskTextfield::OnMouseExited(const ui::MouseEvent& event) {
  UpdateViewAppearance();
}

void DeskTextfield::OnThemeChanged() {
  Textfield::OnThemeChanged();
  SetBackground(views::CreateRoundedRectBackground(GetBackgroundColor(),
                                                   kDeskTextfieldBorderRadius));
  AshColorProvider* color_provider = AshColorProvider::Get();
  const SkColor text_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  SetTextColor(text_color);
  SetSelectionTextColor(text_color);

  const SkColor selection_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusAuraColor);
  SetSelectionBackgroundColor(selection_color);

  UpdateFocusRingState();
}

ui::Cursor DeskTextfield::GetCursor(const ui::MouseEvent& event) {
  return ui::mojom::CursorType::kIBeam;
}

void DeskTextfield::OnFocus() {
  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);
  views::Textfield::OnFocus();
  UpdateViewAppearance();
}

void DeskTextfield::OnBlur() {
  GetRenderText()->SetElideBehavior(gfx::ELIDE_TAIL);
  views::Textfield::OnBlur();
  UpdateViewAppearance();

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

views::View* DeskTextfield::GetView() {
  return this;
}

void DeskTextfield::MaybeActivateHighlightedView() {
  RequestFocus();
}

void DeskTextfield::MaybeCloseHighlightedView(bool primary_action) {}

void DeskTextfield::MaybeSwapHighlightedView(bool right) {}

void DeskTextfield::OnViewHighlighted() {
  UpdateFocusRingState();
}

void DeskTextfield::OnViewUnhighlighted() {
  UpdateFocusRingState();
}

void DeskTextfield::UpdateFocusRingState() {
  views::FocusRing* focus_ring = views::FocusRing::Get(this);
  DCHECK(focus_ring);
  focus_ring->SchedulePaint();
}

void DeskTextfield::UpdateViewAppearance() {
  background()->SetNativeControlColor(GetBackgroundColor());
  // Paint the whole view to update the background. The `SchedulePaint` in
  // `UpdateFocusRingState` will only repaint the focus ring.
  SchedulePaint();
  UpdateFocusRingState();
}

SkColor DeskTextfield::GetBackgroundColor() const {
  // Admin desk templates may be read only.
  if (GetReadOnly()) {
    return SK_ColorTRANSPARENT;
  }

  return HasFocus() || IsMouseHovered()
             ? AshColorProvider::Get()->GetControlsLayerColor(
                   AshColorProvider::ControlsLayerType::
                       kControlBackgroundColorInactive)
             : SK_ColorTRANSPARENT;
}

BEGIN_METADATA(DeskTextfield, views::Textfield)
END_METADATA

}  // namespace ash
