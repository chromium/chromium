// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_textfield.h"

#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ash/wm/overview/overview_constants.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/native_cursor.h"

namespace ash {

namespace {

// The border radius on the text field.
constexpr int kDesksTextfieldBorderRadius = 4;

constexpr int kDesksTextfieldMinHeight = 16;

}  // namespace

DesksTextfield::DesksTextfield() {
  views::Builder<DesksTextfield>(this)
      .SetBorder(nullptr)
      .SetCursorEnabled(true)
      // TODO(crbug.com/1218186): Remove this, this is in place temporarily to
      // be able to submit accessibility checks, but this focusable View needs
      // to add a name so that the screen reader knows what to announce.
      .SetProperty(views::kSkipAccessibilityPaintChecks, true)
      .BuildChildren();

  views::FocusRing* focus_ring =
      StyleUtil::SetUpFocusRingForView(this, kFocusRingHaloInset);
  focus_ring->SetHasFocusPredicate([](views::View* view) {
    return static_cast<DesksTextfield*>(view)->IsViewHighlighted() ||
           view->HasFocus();
  });
  GetRenderText()->SetElideBehavior(gfx::ELIDE_TAIL);
}

DesksTextfield::~DesksTextfield() = default;

// static
constexpr size_t DesksTextfield::kMaxLength;

void DesksTextfield::UpdateViewAppearance() {
  background()->SetNativeControlColor(GetBackgroundColor());
  // Paint the whole view to update the background. The `SchedulePaint` in
  // `UpdateFocusRingState` will only repaint the focus ring.
  SchedulePaint();
  UpdateFocusRingState();
}

gfx::Size DesksTextfield::CalculatePreferredSize() const {
  const std::u16string& text = GetText();
  int width = 0;
  int height = 0;
  gfx::Canvas::SizeStringInt(text, GetFontList(), &width, &height, 0,
                             gfx::Canvas::NO_ELLIPSIS);
  gfx::Size size{width + GetCaretBounds().width(), height};
  const auto insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  size.SetToMax(gfx::Size(0, kDesksTextfieldMinHeight));
  return size;
}

void DesksTextfield::SetBorder(std::unique_ptr<views::Border> b) {
  // `views::Textfield` override of `SetBorder()` removes an installed focus
  // ring, which we want to keep.
  views::View::SetBorder(std::move(b));
}

bool DesksTextfield::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  // The default behavior of the tab key is that it moves the focus to the next
  // available view.
  // We want that to be handled by OverviewHighlightController as part of moving
  // the highlight forward or backward when tab or shift+tab are pressed.
  return event.key_code() == ui::VKEY_TAB;
}

void DesksTextfield::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Textfield::GetAccessibleNodeData(node_data);
  node_data->SetName(GetAccessibleName());
}

void DesksTextfield::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateViewAppearance();
}

void DesksTextfield::OnMouseExited(const ui::MouseEvent& event) {
  UpdateViewAppearance();
}

void DesksTextfield::OnThemeChanged() {
  Textfield::OnThemeChanged();
  SetBackground(views::CreateRoundedRectBackground(
      GetBackgroundColor(), kDesksTextfieldBorderRadius));
  AshColorProvider* color_provider = AshColorProvider::Get();
  const SkColor text_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  SetTextColor(text_color);
  SetSelectionTextColor(text_color);

  const SkColor selection_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusAuraColor);
  SetSelectionBackgroundColor(selection_color);

  views::FocusRing::Get(this)->SetColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusRingColor));

  UpdateFocusRingState();
}

gfx::NativeCursor DesksTextfield::GetCursor(const ui::MouseEvent& event) {
  return views::GetNativeIBeamCursor();
}

void DesksTextfield::OnFocus() {
  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);
  views::Textfield::OnFocus();
}

void DesksTextfield::OnBlur() {
  GetRenderText()->SetElideBehavior(gfx::ELIDE_TAIL);
  views::Textfield::OnBlur();
}

void DesksTextfield::OnDragEntered(const ui::DropTargetEvent& event) {
  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);
  views::Textfield::OnDragEntered(event);
}

void DesksTextfield::OnDragExited() {
  GetRenderText()->SetElideBehavior(gfx::ELIDE_TAIL);
  views::Textfield::OnDragExited();
}

views::View* DesksTextfield::GetView() {
  return this;
}

void DesksTextfield::MaybeActivateHighlightedView() {
  RequestFocus();
}

void DesksTextfield::MaybeCloseHighlightedView(bool primary_action) {}

void DesksTextfield::MaybeSwapHighlightedView(bool right) {}

void DesksTextfield::OnViewHighlighted() {
  UpdateFocusRingState();
}

void DesksTextfield::OnViewUnhighlighted() {
  UpdateFocusRingState();
}

void DesksTextfield::UpdateFocusRingState() {
  views::FocusRing* focus_ring = views::FocusRing::Get(this);
  DCHECK(focus_ring);
  focus_ring->SchedulePaint();
}

SkColor DesksTextfield::GetBackgroundColor() const {
  // Admin desk templates may be read only.
  if (GetReadOnly())
    return SK_ColorTRANSPARENT;

  return HasFocus() || IsMouseHovered()
             ? AshColorProvider::Get()->GetControlsLayerColor(
                   AshColorProvider::ControlsLayerType::
                       kControlBackgroundColorInactive)
             : SK_ColorTRANSPARENT;
}

BEGIN_METADATA(DesksTextfield, views::Textfield)
END_METADATA

}  // namespace ash
