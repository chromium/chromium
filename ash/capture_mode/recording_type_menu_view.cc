// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/recording_type_menu_view.h"

#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"

namespace ash {

namespace {

// The IDs of the options representing the available recording formats.
enum RecordingTypeOption {
  kWebM = 0,
  kGif,
};

// The padding around the menu options.
constexpr auto kMenuPadding = gfx::Insets::VH(12, 0);

// The vertical space between the two nearest edges of the capture label widget
// and the recording type menu widget.
constexpr int kYOffsetFromLabelWidget = 8;

constexpr int kMinimumWidth = 184;
constexpr gfx::Size kIdealSize{kMinimumWidth, 96};

constexpr gfx::RoundedCornersF kBorderRadius{12.f};

// Gets the ideal size of the widget hosting the `RecordingTypeMenuView` either
// from the preferred size of `contents_view` (if given), or the default size.
gfx::Size GetIdealSize(views::View* contents_view) {
  gfx::Size size =
      contents_view ? contents_view->GetPreferredSize() : kIdealSize;
  if (size.width() < kMinimumWidth)
    size.set_width(kMinimumWidth);
  return size;
}

}  // namespace

RecordingTypeMenuView::RecordingTypeMenuView()
    : CaptureModeMenuGroup(this, kMenuPadding) {
  SetPaintToLayer();
  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(kBorderRadius);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  AddOption(
      &kCaptureModeVideoIcon,
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_LABEL_VIDEO_RECORD),
      RecordingTypeOption::kWebM);
  AddOption(&kCaptureGifIcon,
            l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_LABEL_GIF_RECORD),
            RecordingTypeOption::kGif);
}

// static
gfx::Rect RecordingTypeMenuView::GetIdealScreenBounds(
    const gfx::Rect& capture_label_widget_screen_bounds,
    views::View* contents_view) {
  const auto size = GetIdealSize(contents_view);
  const auto bottom_center = capture_label_widget_screen_bounds.bottom_center();
  const int y = bottom_center.y() + kYOffsetFromLabelWidget;
  const int x = bottom_center.x() - (size.width() / 2);
  return gfx::Rect(gfx::Point(x, y), size);
}

void RecordingTypeMenuView::OnOptionSelected(int option_id) const {}

bool RecordingTypeMenuView::IsOptionChecked(int option_id) const {
  return option_id == RecordingTypeOption::kWebM;
}

bool RecordingTypeMenuView::IsOptionEnabled(int option_id) const {
  return true;
}

}  // namespace ash
