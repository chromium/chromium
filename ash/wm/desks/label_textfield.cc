// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/label_textfield.h"

#include "ash/style/ash_color_provider.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/background.h"
#include "ui/views/native_cursor.h"

namespace ash {

namespace {

constexpr int kLabelTextfieldMinHeight = 24;
constexpr int kLabelTextfieldHorizontalPadding = 6;

}  // namespace

LabelTextfield::LabelTextfield() {
  auto border = std::make_unique<WmHighlightItemBorder>(
      kLabelTextfieldBorderRadius,
      gfx::Insets(0, kLabelTextfieldHorizontalPadding));
  border_ptr_ = border.get();

  views::Builder<LabelTextfield>(this)
      .SetBorder(std::move(border))
      .SetCursorEnabled(true)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER)
      // TODO(crbug.com/1218186): Remove this, this is in place temporarily to
      // be able to submit accessibility checks, but this focusable View needs
      // to add a name so that the screen reader knows what to announce.
      .SetProperty(views::kSkipAccessibilityPaintChecks, true)
      .BuildChildren();
}

LabelTextfield::~LabelTextfield() = default;

// static
constexpr size_t LabelTextfield::kLabelTextfieldBorderRadius;
constexpr size_t LabelTextfield::kMaxLength;

void LabelTextfield::SetTextAndElideIfNeeded(const std::u16string& text) {
  // Use the potential max size of this to calculate elision, not its current
  // size to avoid eliding names that don't need to be.
  SetText(
      gfx::ElideText(text, GetFontList(),
                     parent()->GetPreferredSize().width() - GetInsets().width(),
                     gfx::ELIDE_TAIL));
  full_text_ = text;
}

void LabelTextfield::UpdateViewAppearance() {
  background()->SetNativeControlColor(GetBackgroundColor());
  UpdateBorderState();
}

gfx::Size LabelTextfield::CalculatePreferredSize() const {
  const std::u16string& text = GetText();
  int width = 0;
  int height = 0;
  gfx::Canvas::SizeStringInt(text, GetFontList(), &width, &height, 0,
                             gfx::Canvas::NO_ELLIPSIS);
  gfx::Size size{width + GetCaretBounds().width(), height};
  const auto insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  size.SetToMax(gfx::Size(0, kLabelTextfieldMinHeight));
  return size;
}

bool LabelTextfield::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  // The default behavior of the tab key is that it moves the focus to the next
  // available view.
  // We want that to be handled by OverviewHighlightController as part of moving
  // the highlight forward or backward when tab or shift+tab are pressed.
  return event.key_code() == ui::VKEY_TAB;
}

void LabelTextfield::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Textfield::GetAccessibleNodeData(node_data);
  node_data->SetName(full_text_.empty() ? GetAccessibleName() : full_text_);
}

void LabelTextfield::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateViewAppearance();
}

void LabelTextfield::OnMouseExited(const ui::MouseEvent& event) {
  UpdateViewAppearance();
}

void LabelTextfield::OnThemeChanged() {
  Textfield::OnThemeChanged();
  SetBackground(views::CreateRoundedRectBackground(
      GetBackgroundColor(), kLabelTextfieldBorderRadius));
  AshColorProvider* color_provider = AshColorProvider::Get();
  const SkColor text_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  SetTextColor(text_color);
  SetSelectionTextColor(text_color);

  const SkColor selection_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusAuraColor);
  SetSelectionBackgroundColor(selection_color);
  UpdateBorderState();
}

gfx::NativeCursor LabelTextfield::GetCursor(const ui::MouseEvent& event) {
  return views::GetNativeIBeamCursor();
}

views::View* LabelTextfield::GetView() {
  return this;
}

void LabelTextfield::MaybeActivateHighlightedView() {
  RequestFocus();
}

void LabelTextfield::MaybeCloseHighlightedView() {}

void LabelTextfield::MaybeSwapHighlightedView(bool right) {}

void LabelTextfield::OnViewHighlighted() {
  UpdateBorderState();
}

void LabelTextfield::OnViewUnhighlighted() {
  UpdateBorderState();
}

void LabelTextfield::UpdateBorderState() {
  border_ptr_->SetFocused(IsViewHighlighted() || HasFocus());
  SchedulePaint();
}

SkColor LabelTextfield::GetBackgroundColor() const {
  return HasFocus() || IsMouseHovered()
             ? AshColorProvider::Get()->GetControlsLayerColor(
                   AshColorProvider::ControlsLayerType::
                       kControlBackgroundColorInactive)
             : SK_ColorTRANSPARENT;
}

BEGIN_METADATA(LabelTextfield, views::Textfield)
END_METADATA

}  // namespace ash
