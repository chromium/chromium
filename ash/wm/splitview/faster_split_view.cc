// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/faster_split_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/style_util.h"
#include "ash/system/toast/system_toast_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/wm_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/highlight_border.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

// Distance from the right of the faster splitscreen toast to the left of the
// settings button.
constexpr int kSettingsButtonSpacingDp = 8;

}  // namespace

// A toast in faster splitscreen setup. Contains a dialog and skip button.
class FasterSplitViewToast : public SystemToastView,
                             public OverviewFocusableView {
  METADATA_HEADER(FasterSplitViewToast, SystemToastView)

 public:
  explicit FasterSplitViewToast(base::RepeatingClosure skip_callback)
      : SystemToastView(/*text=*/l10n_util::GetStringUTF16(
                            IDS_ASH_OVERVIEW_FASTER_SPLITSCREEN_TOAST),
                        /*dismiss_text=*/
                        l10n_util::GetStringUTF16(
                            IDS_ASH_OVERVIEW_FASTER_SPLITSCREEN_TOAST_SKIP),
                        /*dismiss_callback=*/std::move(skip_callback)) {
    dismiss_button()->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_ASH_OVERVIEW_FASTER_SPLITSCREEN_TOAST_DISMISS_WINDOW_SUGGESTIONS));
  }
  FasterSplitViewToast(const FasterSplitViewToast&) = delete;
  FasterSplitViewToast& operator=(const FasterSplitViewToast&) = delete;
  ~FasterSplitViewToast() override = default;

  // OverviewFocusableView:
  views::View* GetView() override { return dismiss_button(); }

  void MaybeActivateFocusedView() override {
    // Destroys `this`.
    dismiss_button()->button_controller()->NotifyClick();
  }

  void MaybeCloseFocusedView(bool primary_action) override {}

  void MaybeSwapFocusedView(bool right) override {}

  void OnFocusableViewFocused() override { ToggleButtonA11yFocus(); }

  void OnFocusableViewBlurred() override { ToggleButtonA11yFocus(); }
};

BEGIN_METADATA(FasterSplitViewToast)
END_METADATA

FasterSplitViewSettingsButton::FasterSplitViewSettingsButton(
    base::RepeatingClosure settings_callback)
    : IconButton(std::move(settings_callback),
                 IconButton::Type::kLarge,
                 &kOverviewSettingsIcon,
                 IDS_ASH_OVERVIEW_SETTINGS_BUTTON_LABEL) {
  SetBackgroundColor(kColorAshShieldAndBase80);

  views::FocusRing* focus_ring =
      StyleUtil::SetUpFocusRingForView(this, kWindowMiniViewFocusRingHaloInset);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
  focus_ring->SetHasFocusPredicate(
      base::BindRepeating([](const views::View* view) {
        const auto* v = views::AsViewClass<FasterSplitViewSettingsButton>(view);
        CHECK(v);
        return v->is_focused();
      }));
}

FasterSplitViewSettingsButton::~FasterSplitViewSettingsButton() = default;

views::View* FasterSplitViewSettingsButton::GetView() {
  return this;
}

void FasterSplitViewSettingsButton::MaybeActivateFocusedView() {
  // Destroys `this`.
  button_controller()->NotifyClick();
}

void FasterSplitViewSettingsButton::MaybeCloseFocusedView(bool primary_action) {
}

void FasterSplitViewSettingsButton::MaybeSwapFocusedView(bool right) {}

void FasterSplitViewSettingsButton::OnFocusableViewFocused() {
  views::FocusRing::Get(this)->SchedulePaint();
}

void FasterSplitViewSettingsButton::OnFocusableViewBlurred() {
  views::FocusRing::Get(this)->SchedulePaint();
}

BEGIN_METADATA(FasterSplitViewSettingsButton)
END_METADATA

FasterSplitView::FasterSplitView(base::RepeatingClosure skip_callback,
                                 base::RepeatingClosure settings_callback) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetBetweenChildSpacing(kSettingsButtonSpacingDp);

  toast_ = AddChildView(
      std::make_unique<FasterSplitViewToast>(std::move(skip_callback)));

  settings_button_ =
      AddChildView(std::make_unique<FasterSplitViewSettingsButton>(
          std::move(settings_callback)));

  const int toast_height = settings_button_->GetPreferredSize().height();
  const float toast_corner_radius = toast_height / 2.0f;
  settings_button_->SetBorder(std::make_unique<views::HighlightBorder>(
      toast_corner_radius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
}

OverviewFocusableView* FasterSplitView::GetToast() {
  return static_cast<OverviewFocusableView*>(toast_);
}

views::LabelButton* FasterSplitView::GetDismissButton() {
  return toast_->dismiss_button();
}

BEGIN_METADATA(FasterSplitView)
END_METADATA

}  // namespace ash
