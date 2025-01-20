// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_slider.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "base/notreached.h"
#include "cc/paint/paint_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/slider.h"

namespace ash {

using Style = QuickSettingsSlider::Style;

namespace {

// The radius used to draw the rounded empty slider ends.
constexpr float kEmptySliderRoundedRadius = 2.f;
constexpr float kEmptySliderWidth = 2 * kEmptySliderRoundedRadius;

// The radius used to draw the rounded full slider ends.
constexpr float kFullSliderRoundedRadius = 16.f;
constexpr float kFullSliderWidth = 2 * kFullSliderRoundedRadius;

// The radius used to draw the rounded corner for active/inactive slider on the
// audio subpage.
constexpr float kActiveRadioSliderRoundedRadius = 18.f;
constexpr float kInactiveRadioSliderRoundedRadius = 8.f;
constexpr float kRadioSliderWidth = 2 * kActiveRadioSliderRoundedRadius;

// The thickness of the focus ring border.
constexpr int kLineThickness = 2;
// The gap between the focus ring and the slider.
constexpr int kFocusOffset = 2;

// The offset for the slider top padding.
constexpr int kTopPaddingOffset = 4;

float GetSliderRoundedCornerRadius(Style slider_style) {
  switch (slider_style) {
    case Style::kDefault:
    case Style::kDefaultMuted:
      return kFullSliderRoundedRadius;
    case Style::kRadioActive:
    case Style::kRadioActiveMuted:
      return kActiveRadioSliderRoundedRadius;
    case Style::kRadioInactive:
      return kInactiveRadioSliderRoundedRadius;
    default:
      NOTREACHED();
  }
}

float GetSliderWidth(Style slider_style) {
  switch (slider_style) {
    case Style::kDefault:
    case Style::kDefaultMuted:
      return kFullSliderWidth;
    case Style::kRadioActive:
    case Style::kRadioActiveMuted:
    case Style::kRadioInactive:
      return kRadioSliderWidth;
    default:
      NOTREACHED();
  }
}

}  // namespace

QuickSettingsSlider::QuickSettingsSlider(views::SliderListener* listener,
                                         Style slider_style)
    : views::Slider(listener), slider_style_(slider_style) {
  SetValueIndicatorRadius(kFullSliderRoundedRadius);
  SetFocusBehavior(FocusBehavior::ALWAYS);

  GetViewAccessibility().SetRole(ax::mojom::Role::kSlider);
  GetViewAccessibility().AddAction(ax::mojom::Action::kIncrement);
  GetViewAccessibility().AddAction(ax::mojom::Action::kDecrement);
}

QuickSettingsSlider::~QuickSettingsSlider() = default;

void QuickSettingsSlider::SetSliderStyle(Style style) {
  if (slider_style_ == style)
    return;

  slider_style_ = style;

  if (slider_style_ == Style::kRadioInactive)
    SetFocusBehavior(FocusBehavior::NEVER);

  SchedulePaint();
}

gfx::Rect QuickSettingsSlider::GetInactiveRadioSliderRect() {
  const gfx::Rect content = GetContentsBounds();
  return gfx::Rect(content.x() - kFocusOffset,
                   content.height() / 2 - kRadioSliderWidth / 2 - kFocusOffset +
                       kTopPaddingOffset,
                   content.width() + 2 * kFocusOffset,
                   kRadioSliderWidth + 2 * kFocusOffset);
}

int QuickSettingsSlider::GetInactiveRadioSliderRoundedCornerRadius() {
  return kInactiveRadioSliderRoundedRadius + kFocusOffset;
}

void QuickSettingsSlider::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  View::GetAccessibleNodeData(node_data);
  std::u16string volume_level = base::UTF8ToUTF16(
      base::StringPrintf("%d%%", static_cast<int>(GetValue() * 100 + 0.5)));

  if (is_toggleable_volume_slider_) {
    std::u16string message = l10n_util::GetStringFUTF16(
        slider_style_ == Style::kDefaultMuted
            ? IDS_ASH_STATUS_TRAY_VOLUME_SLIDER_MUTED_ACCESSIBILITY_ANNOUNCEMENT
            : IDS_ASH_STATUS_TRAY_VOLUME_SLIDER_ACCESSIBILITY_ANNOUNCEMENT,
        volume_level);

    node_data->SetValue(message);
  } else {
    node_data->SetValue(volume_level);
  }
}

SkColor QuickSettingsSlider::GetThumbColor() const {
  switch (slider_style_) {
    case Style::kDefault:
    case Style::kRadioActive:
      return GetColorProvider()->GetColor(static_cast<ui::ColorId>(
          cros_tokens::kCrosSysSystemPrimaryContainer));
    case Style::kDefaultMuted:
      return GetColorProvider()->GetColor(
          static_cast<ui::ColorId>(cros_tokens::kCrosSysDisabledOpaque));
    case Style::kRadioActiveMuted:
    case Style::kRadioInactive:
      return GetColorProvider()->GetColor(
          static_cast<ui::ColorId>(cros_tokens::kCrosSysDisabledContainer));
    default:
      NOTREACHED();
  }
}

SkColor QuickSettingsSlider::GetTroughColor() const {
  switch (slider_style_) {
    case Style::kDefault:
      return GetColorProvider()->GetColor(
          static_cast<ui::ColorId>(cros_tokens::kCrosSysSystemOnBase));
    case Style::kRadioActive:
      return GetColorProvider()->GetColor(
          static_cast<ui::ColorId>(cros_tokens::kCrosSysHighlightShape));
    case Style::kDefaultMuted:
    case Style::kRadioActiveMuted:
    case Style::kRadioInactive:
      return GetColorProvider()->GetColor(
          static_cast<ui::ColorId>(cros_tokens::kCrosSysDisabledContainer));
    default:
      NOTREACHED();
  }
}

void QuickSettingsSlider::OnPaint(gfx::Canvas* canvas) {
  const gfx::Rect content = GetContentsBounds();
  const float slider_width = GetSliderWidth(slider_style_);
  const float slider_radius = GetSliderRoundedCornerRadius(slider_style_);
  const int width = content.width() - slider_width;
  const int full_width = GetAnimatingValue() * width + slider_width;
  const int x = content.x();
  const int y = content.height() / 2 - slider_width / 2 + kTopPaddingOffset;

  gfx::Rect empty_slider_rect;
  float empty_slider_radius;
  switch (slider_style_) {
    case Style::kDefault:
    case Style::kDefaultMuted: {
      const int empty_width =
          width + kFullSliderRoundedRadius - full_width + kEmptySliderWidth;
      const int x_empty = x + full_width - kEmptySliderRoundedRadius;
      const int y_empty =
          content.height() / 2 - kEmptySliderWidth / 2 + kTopPaddingOffset;

      empty_slider_rect =
          gfx::Rect(x_empty, y_empty, empty_width, kEmptySliderWidth);
      empty_slider_radius = kEmptySliderRoundedRadius;
      break;
    }
    case Style::kRadioActive:
    case Style::kRadioActiveMuted:
    case Style::kRadioInactive: {
      empty_slider_rect = gfx::Rect(x, y, content.width(), kRadioSliderWidth);
      empty_slider_radius = slider_radius;
      break;
    }
    default:
      NOTREACHED();
  }

  cc::PaintFlags slider_flags;
  slider_flags.setAntiAlias(true);

  slider_flags.setColor(GetTroughColor());
  canvas->DrawRoundRect(empty_slider_rect, empty_slider_radius, slider_flags);

  slider_flags.setColor(GetThumbColor());
  canvas->DrawRoundRect(gfx::Rect(x, y, full_width, slider_width),
                        slider_radius, slider_flags);

  // Paints the focusing ring for the slider. It should be painted last to be
  // on the top.
  if (HasFocus()) {
    cc::PaintFlags highlight_border;
    highlight_border.setColor(GetColorProvider()->GetColor(
        static_cast<ui::ColorId>(cros_tokens::kCrosSysPrimary)));
    highlight_border.setAntiAlias(true);
    highlight_border.setStyle(cc::PaintFlags::kStroke_Style);
    highlight_border.setStrokeWidth(kLineThickness);
    canvas->DrawRoundRect(gfx::Rect(x - kFocusOffset, y - kFocusOffset,
                                    full_width + 2 * kFocusOffset,
                                    slider_width + 2 * kFocusOffset),
                          slider_radius + kFocusOffset, highlight_border);
  }
}

void QuickSettingsSlider::OnThemeChanged() {
  views::View::OnThemeChanged();

  // Signals that this view needs to be repainted since `GetColorProvider()` is
  // called in `OnPaint()` and the views system won't know about it.
  SchedulePaint();
}

ReadOnlySlider::ReadOnlySlider(Style slider_style)
    : QuickSettingsSlider(/*listener=*/nullptr, slider_style) {}

ReadOnlySlider::~ReadOnlySlider() = default;

bool ReadOnlySlider::CanAcceptEvent(const ui::Event& event) {
  return false;
}

BEGIN_METADATA(QuickSettingsSlider)
END_METADATA

BEGIN_METADATA(ReadOnlySlider)
END_METADATA

}  // namespace ash
