// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/rich_nudge.h"

#include <algorithm>

#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/system/toast/system_nudge_view.h"
#include "chrome/grit/component_extension_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/flex_layout.h"

namespace arc::input_overlay {

namespace {

// Margin around this view relative to the inside of the parent window.
constexpr int kMargin = 24;

constexpr char kRichNudgeId[] = "RichNudgeID";

}  // namespace

RichNudge::RichNudge(aura::Window* parent_window)
    : views::BubbleDialogDelegateView(/*anchor_view=*/nullptr,
                                      views::BubbleBorder::FLOAT,
                                      views::BubbleBorder::NO_SHADOW) {
  set_parent_window(parent_window);
  set_color(SK_ColorTRANSPARENT);
  set_margins(gfx::Insets());
  set_close_on_deactivate(false);
  set_accept_events(false);
  set_adjust_if_offscreen(false);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetCanActivate(false);
  // Ignore this view for accessibility purposes.
  SetAccessibleWindowRole(ax::mojom::Role::kNone);

  SetLayoutManager(std::make_unique<views::FlexLayout>());

  auto nudge_data = ash::AnchoredNudgeData(
      kRichNudgeId, ash::NudgeCatalogName::kGameDashboardControlsNudge,
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_BUTTON_PLACEMENT_NUDGE_BODY));
  nudge_data.title_text =
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_BUTTON_PLACEMENT_NUDGE_TITLE);
  nudge_data.image_model =
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_ARC_INPUT_OVERLAY_BUTTON_PLACEMENT_MODE_NUDGE_JSON);
  nudge_data.background_color_id = cros_tokens::kCrosSysBaseHighlight;
  nudge_data.image_background_color_id = cros_tokens::kCrosSysOnBaseHighlight;

  AddChildView(std::make_unique<ash::SystemNudgeView>(nudge_data));
}

RichNudge::~RichNudge() = default;

void RichNudge::FlipPosition() {
  on_top_ = !on_top_;
  SetAnchorRect(GetAnchorRect());
}

gfx::Rect RichNudge::GetAnchorRect() const {
  auto* parent = parent_window();
  DCHECK(parent);
  const auto parent_bounds = parent->GetBoundsInScreen();

  auto* widget = GetWidget();
  DCHECK(widget);
  auto size = widget->GetWindowBoundsInScreen().size();
  size.Enlarge(2 * kMargin, 2 * kMargin);

  auto bounds = gfx::Rect(parent_bounds.origin(), size);
  if (!on_top_) {
    bounds.set_x(std::max(bounds.x(), parent_bounds.right() - size.width()));
    bounds.set_y(std::max(bounds.y(), parent_bounds.bottom() - size.height()));
  }
  return bounds;
}

void RichNudge::VisibilityChanged(views::View* starting_from, bool is_visible) {
  if (is_visible) {
    SetAnchorRect(GetAnchorRect());
  }
}

BEGIN_METADATA(RichNudge)
END_METADATA
}  // namespace arc::input_overlay
