// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/faster_split_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_util.h"
#include "ash/style/system_shadow.h"
#include "ash/system/toast/system_toast_view.h"
#include "ash/wm/wm_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/highlight_border.h"

namespace ash {

namespace {

// Distance from the right of the faster splitscreen toast to the left of the
// settings button.
constexpr int kSettingsButtonSpacingDp = 8;

// -----------------------------------------------------------------------------
// FasterSplitViewSettingsButton:

// Settings button in the faster split screen setup that can deep link to the
// window snap preference section upon clicking.
class FasterSplitViewSettingsButton : public IconButton {
  METADATA_HEADER(FasterSplitViewSettingsButton, IconButton)

 public:
  explicit FasterSplitViewSettingsButton(
      base::RepeatingClosure settings_callback)
      : IconButton(std::move(settings_callback),
                   IconButton::Type::kLarge,
                   &kOverviewSettingsIcon,
                   IDS_ASH_OVERVIEW_SETTINGS_BUTTON_LABEL),
        shadow_(SystemShadow::CreateShadowOnTextureLayer(
            SystemShadow::Type::kElevation4)) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetBackgroundColor(kColorAshShieldAndBase80);

    const float settings_button_corner_radius =
        GetPreferredSize().height() / 2.0f;
    SetBorder(std::make_unique<views::HighlightBorder>(
        settings_button_corner_radius,
        views::HighlightBorder::Type::kHighlightBorderOnShadow));
    shadow_->SetRoundedCornerRadius(settings_button_corner_radius);

    StyleUtil::SetUpFocusRingForView(this, kWindowMiniViewFocusRingHaloInset);
  }

  FasterSplitViewSettingsButton(const FasterSplitViewSettingsButton&) = delete;
  FasterSplitViewSettingsButton& operator=(
      const FasterSplitViewSettingsButton&) = delete;
  ~FasterSplitViewSettingsButton() override = default;

  // views::View:
  void AddedToWidget() override {
    // Since the layer of the shadow has to be added as a sibling to this view's
    // layer, we need to wait until the view is added to the widget.
    auto* parent = layer()->parent();
    parent->Add(shadow_->GetLayer());
    parent->StackAtBottom(shadow_->GetLayer());
  }

  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    // The shadow layer is a sibling of this view's layer, whose contents bounds
    // should be the same as the view's bounds.
    shadow_->SetContentBounds(layer()->bounds());
  }

 private:
  std::unique_ptr<SystemShadow> shadow_;
};

BEGIN_METADATA(FasterSplitViewSettingsButton)
END_METADATA

}  // namespace

// -----------------------------------------------------------------------------
// FasterSplitView:

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
  auto* dismiss_button = toast->dismiss_button();
  dismiss_button->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_OVERVIEW_FASTER_SPLITSCREEN_TOAST_DISMISS_WINDOW_SUGGESTIONS));
  dismiss_button->SetID(kDismissButtonIDForTest);

  auto* settings_button =
      AddChildView(std::make_unique<FasterSplitViewSettingsButton>(
          std::move(settings_callback)));
  settings_button->SetID(kSettingsButtonIDForTest);
}

FasterSplitView::~FasterSplitView() = default;

BEGIN_METADATA(FasterSplitView)
END_METADATA

}  // namespace ash
