// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/faster_split_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_util.h"
#include "ash/system/toast/system_toast_view.h"
#include "ash/wm/wm_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/highlight_border.h"

namespace ash {

namespace {

// Distance from the right of the faster splitscreen toast to the left of the
// settings button.
constexpr int kSettingsButtonSpacingDp = 8;

}  // namespace

FasterSplitView::FasterSplitView(base::RepeatingClosure skip_callback,
                                 base::RepeatingClosure settings_callback) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetBetweenChildSpacing(kSettingsButtonSpacingDp);

  auto* toast = AddChildView(std::make_unique<SystemToastView>(
      /*text=*/l10n_util::GetStringUTF16(
          IDS_ASH_OVERVIEW_FASTER_SPLITSCREEN_TOAST),
      /*dismiss_text=*/
      l10n_util::GetStringUTF16(IDS_ASH_OVERVIEW_FASTER_SPLITSCREEN_TOAST_SKIP),
      /*dismiss_callback=*/std::move(skip_callback),
      /*leading_icon=*/&gfx::kNoneIcon, /*use_custom_focus=*/false));
  toast->dismiss_button()->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_OVERVIEW_FASTER_SPLITSCREEN_TOAST_DISMISS_WINDOW_SUGGESTIONS));

  auto* settings_button = AddChildView(std::make_unique<IconButton>(
      std::move(settings_callback), IconButton::Type::kLarge,
      &kOverviewSettingsIcon, IDS_ASH_OVERVIEW_SETTINGS_BUTTON_LABEL));
  settings_button->SetBackgroundColor(kColorAshShieldAndBase80);

  views::FocusRing* focus_ring = StyleUtil::SetUpFocusRingForView(
      settings_button, kWindowMiniViewFocusRingHaloInset);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetColorId(ui::kColorAshFocusRing);

  const int toast_height = settings_button->GetPreferredSize().height();
  const float toast_corner_radius = toast_height / 2.0f;
  settings_button->SetBorder(std::make_unique<views::HighlightBorder>(
      toast_corner_radius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
}

FasterSplitView::~FasterSplitView() = default;

BEGIN_METADATA(FasterSplitView)
END_METADATA

}  // namespace ash
