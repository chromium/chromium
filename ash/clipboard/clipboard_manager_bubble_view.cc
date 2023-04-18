// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_manager_bubble_view.h"

#include <memory>

#include "ash/bubble/bubble_constants.h"
#include "ash/style/system_shadow.h"
#include "base/memory/ptr_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/highlight_border.h"

using views::BubbleBorder;
using views::BubbleFrameView;
using views::HighlightBorder;
using views::NonClientFrameView;
using views::Widget;

namespace ash {

namespace {

constexpr int kClipboardManagerCornerRadius = 12;
constexpr int kClipboardManagerHeight = 100;
constexpr int kClipboardManagerWidth = 320;

}  // namespace

// static
ClipboardManagerBubbleView* ClipboardManagerBubbleView::Create(
    const gfx::Rect& anchor_rect) {
  auto* bubble_view = new ClipboardManagerBubbleView(anchor_rect);
  // Future calls to `GetWidget()` will return the `Widget` initialized in
  // `CreateBubble()`.
  CreateBubble(base::WrapUnique(bubble_view));
  return bubble_view;
}

ClipboardManagerBubbleView::~ClipboardManagerBubbleView() = default;

gfx::Size ClipboardManagerBubbleView::CalculatePreferredSize() const {
  // TODO(b/267694484): Calculate height based on clipboard contents.
  return gfx::Size(kClipboardManagerWidth, kClipboardManagerHeight);
}

std::unique_ptr<NonClientFrameView>
ClipboardManagerBubbleView::CreateNonClientFrameView(Widget* widget) {
  auto frame = BubbleDialogDelegateView::CreateNonClientFrameView(widget);

  auto* bubble_border =
      static_cast<BubbleFrameView*>(frame.get())->bubble_border();
  bubble_border->set_avoid_shadow_overlap(true);
  bubble_border->set_md_shadow_elevation(
      SystemShadow::GetElevationFromType(SystemShadow::Type::kElevation12));
  return frame;
}

void ClipboardManagerBubbleView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kMenu;
  node_data->SetNameChecked(GetAccessibleWindowTitle());
}

std::u16string ClipboardManagerBubbleView::GetAccessibleWindowTitle() const {
  // TODO(b/267694484): Finalize a11y label.
  return u"[i18n] Clipboard menu";
}

void ClipboardManagerBubbleView::OnThemeChanged() {
  views::BubbleDialogDelegateView::OnThemeChanged();

  set_color(
      GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemBaseElevated));
}

ClipboardManagerBubbleView::ClipboardManagerBubbleView(
    const gfx::Rect& anchor_rect) {
  SetAnchorRect(anchor_rect);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_force_create_contents_background(true);
  set_margins(gfx::Insets());
  set_corner_radius(kClipboardManagerCornerRadius);
  SetBorder(std::make_unique<views::HighlightBorder>(
      kClipboardManagerCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));

  // TODO(b/267694484): Create a clipboard manager container that serves as a
  // parent so that the launcher will know not to close when the manager shows.
  set_has_parent(false);

  // Like other menus, the clipboard manager should close in response to the Esc
  // key or the loss of focus.
  set_close_on_deactivate(true);

  // TODO(b/267694484): Enable arrow key traversal once the menu has items.
  // TODO(b/267694484): Set initially focused view once the menu has items.

  SetAccessibleWindowRole(ax::mojom::Role::kMenu);
}

BEGIN_METADATA(ClipboardManagerBubbleView, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace ash
