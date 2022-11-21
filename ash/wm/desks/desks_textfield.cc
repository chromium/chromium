// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_textfield.h"

#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
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
constexpr int kDesksTextfieldBorderRadius = 4;

constexpr int kDesksTextfieldMinHeight = 16;

#if DCHECK_IS_ON()
bool IsDesksBarOrSavedDeskLibraryWidget(const views::Widget* widget) {
  if (!widget)
    return false;

  auto* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller->InOverviewSession())
    return false;

  auto* session = overview_controller->overview_session();
  for (const auto& grid : session->grid_list()) {
    if (widget == grid->saved_desk_library_widget() ||
        widget == grid->desks_widget()) {
      return true;
    }
  }

  return false;
}
#endif  // DCHECK_IS_ON()

}  // namespace

DesksTextfield::DesksTextfield() {
  views::Builder<DesksTextfield>(this)
      .SetBorder(nullptr)
      .SetCursorEnabled(true)
      .BuildChildren();

  views::FocusRing* focus_ring =
      StyleUtil::SetUpFocusRingForView(this, kFocusRingHaloInset);
  focus_ring->SetHasFocusPredicate([](views::View* view) {
    return static_cast<DesksTextfield*>(view)->IsViewHighlighted() ||
           view->HasFocus();
  });
  focus_ring->SetColorId(ui::kColorAshFocusRing);

  GetRenderText()->SetElideBehavior(gfx::ELIDE_TAIL);
}

DesksTextfield::~DesksTextfield() = default;

// static
constexpr size_t DesksTextfield::kMaxLength;

// static
void DesksTextfield::CommitChanges(views::Widget* widget) {
#if DCHECK_IS_ON()
  DCHECK(IsDesksBarOrSavedDeskLibraryWidget(widget));
#endif  // DCHECK_IS_ON()

  auto* focus_manager = widget->GetFocusManager();
  focus_manager->ClearFocus();
  // Avoid having the focus restored to the same view when the parent view is
  // refocused.
  focus_manager->SetStoredFocusView(nullptr);
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

std::u16string DesksTextfield::GetTooltipText(const gfx::Point& p) const {
  return GetPreferredSize().width() > width() ? GetText() : std::u16string();
}

void DesksTextfield::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Textfield::GetAccessibleNodeData(node_data);
  node_data->SetNameChecked(GetAccessibleName());
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

  UpdateFocusRingState();
}

ui::Cursor DesksTextfield::GetCursor(const ui::MouseEvent& event) {
  return ui::mojom::CursorType::kIBeam;
}

void DesksTextfield::OnFocus() {
  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);
  views::Textfield::OnFocus();
  UpdateViewAppearance();
}

void DesksTextfield::OnBlur() {
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
                       if (w)
                         w->GetFocusManager()->SetStoredFocusView(nullptr);
                     },
                     GetWidget()->GetWeakPtr()));
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

void DesksTextfield::UpdateViewAppearance() {
  background()->SetNativeControlColor(GetBackgroundColor());
  // Paint the whole view to update the background. The `SchedulePaint` in
  // `UpdateFocusRingState` will only repaint the focus ring.
  SchedulePaint();
  UpdateFocusRingState();
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
