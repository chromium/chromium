// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/back_button.h"

#include "ash/keyboard/keyboard_util.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

namespace ash {

BackButton::BackButton(Shelf* shelf) : ShelfControlButton(shelf, this) {
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_BACK_BUTTON_TITLE));
  SetFlipCanvasOnPaintForRTLUI(true);
}

BackButton::~BackButton() {}

void BackButton::HandleLocaleChange() {
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_BACK_BUTTON_TITLE));
  TooltipTextChanged();
}

void BackButton::PaintButtonContents(gfx::Canvas* canvas) {
  // Use PaintButtonContents instead of SetImage so the icon gets drawn at
  // |GetCenterPoint| coordinates instead of always in the center.
  gfx::ImageSkia img = CreateVectorIcon(
      kShelfBackIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kButtonIconColor));
  canvas->DrawImageInt(img, GetCenterPoint().x() - img.width() / 2,
                       GetCenterPoint().y() - img.height() / 2);
}

std::u16string BackButton::GetTooltipText(const gfx::Point& p) const {
  return GetViewAccessibility().GetCachedName();
}

void BackButton::OnShelfButtonAboutToRequestFocusFromTabTraversal(
    ShelfButton* button,
    bool reverse) {
  DCHECK_EQ(button, this);
  if (!reverse) {
    // We're trying to focus this button by advancing from the last view of
    // the shelf. Let the focus manager advance to the status area instead.
    shelf()->shelf_focus_cycler()->FocusOut(reverse,
                                            SourceView::kShelfNavigationView);
  }
}

void BackButton::ButtonPressed(views::Button* sender,
                               const ui::Event& event,
                               views::InkDrop* ink_drop) {
  base::RecordAction(base::UserMetricsAction("AppList_BackButtonPressed"));

  if (keyboard_util::CloseKeyboardIfActive())
    return;

  if (window_util::ShouldMinimizeTopWindowOnBack()) {
    auto* top_window = window_util::GetTopWindow();
    DCHECK(top_window);
    WindowState::Get(top_window)->Minimize();
    return;
  }

  window_util::SendBackKeyEvent(
      GetWidget()->GetNativeWindow()->GetRootWindow());
}

void BackButton::OnThemeChanged() {
  ShelfControlButton::OnThemeChanged();
  SchedulePaint();
}

BEGIN_METADATA(BackButton)
END_METADATA

}  // namespace ash
