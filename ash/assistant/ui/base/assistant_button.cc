// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/base/assistant_button.h"

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/base/assistant_button_listener.h"
#include "ash/assistant/util/histogram_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/style_util.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Appearance.
constexpr int kFocusRingStrokeWidth = 2;
constexpr int kInkDropInset = 2;

}  // namespace

// AssistantButton::InitParams -------------------------------------------------

AssistantButton::InitParams::InitParams() = default;
AssistantButton::InitParams::InitParams(InitParams&&) = default;
AssistantButton::InitParams::~InitParams() = default;

// AssistantButton -------------------------------------------------------------

AssistantButton::AssistantButton(AssistantButtonListener* listener,
                                 AssistantButtonId button_id)
    : views::ImageButton(base::BindRepeating(&AssistantButton::OnButtonPressed,
                                             base::Unretained(this))),
      listener_(listener),
      id_(button_id) {
  // Avoid drawing default focus ring and draw customized focus instead.
  SetInstallFocusRingOnFocus(false);
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // Inkdrop only on click.
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  views::InkDrop::UseInkDropForFloodFillRipple(views::InkDrop::Get(this),
                                               /*highlight_on_hover=*/false);
  views::InstallCircleHighlightPathGenerator(this, gfx::Insets(kInkDropInset));

  // Image.
  SetFlipCanvasOnPaintForRTLUI(false);
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
}

AssistantButton::~AssistantButton() = default;

// static
std::unique_ptr<AssistantButton> AssistantButton::Create(
    AssistantButtonListener* listener,
    const gfx::VectorIcon& icon,
    AssistantButtonId button_id,
    InitParams params) {
  DCHECK_GT(params.size_in_dip, 0);
  DCHECK_GT(params.icon_size_in_dip, 0);
  DCHECK(params.accessible_name_id.has_value());

  auto button = std::make_unique<AssistantButton>(listener, button_id);
  button->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(params.accessible_name_id.value()));

  if (params.tooltip_id) {
    button->SetTooltipText(
        l10n_util::GetStringUTF16(params.tooltip_id.value()));
  }

  button->SetPreferredSize(gfx::Size(params.size_in_dip, params.size_in_dip));

  gfx::IconDescription icon_description(icon, params.icon_size_in_dip,
                                        gfx::kPlaceholderColor);

  if (params.icon_color_type.has_value()) {
    // If we have an `icon_color_type`, that color needs to be resolved in
    // OnThemeChanged(). Since we can't do anything else now, just set the data
    // and return the button.
    button->icon_color_type_ = params.icon_color_type.value();
    // We cannot copy IconDescription as copy assignment operator of
    // IconDescription is deleted since it has a non-static reference member,
    // icon.
    button->icon_description_.emplace(icon_description);
    return button;
  }

  button->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(*icon_description.icon, params.icon_color,
                                     icon_description.dip_size));
  return button;
}

void AssistantButton::OnBlur() {
  views::ImageButton::OnBlur();
  SchedulePaint();
}

void AssistantButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Note that the current assumption is that button bounds are square.
  DCHECK_EQ(width(), height());
}

void AssistantButton::OnFocus() {
  views::ImageButton::OnFocus();
  SchedulePaint();
}

void AssistantButton::OnPaintBackground(gfx::Canvas* canvas) {
  // Hide focus ring when keyboard traversal is not enabled.
  // This is specifically applicable to tablet mode when
  // keyboard traversal may be off.
  const bool hide_focus_ring_when_not_keyboard_traversal =
      !AssistantUiController::Get()->GetModel()->keyboard_traversal_mode();
  const bool should_show_focus_ring =
      HasFocus() && !hide_focus_ring_when_not_keyboard_traversal;

  if (should_show_focus_ring) {
    cc::PaintFlags circle_flags;
    circle_flags.setAntiAlias(true);
    circle_flags.setColor(
        GetColorProvider()->GetColor(cros_tokens::kFocusRingColor));
    circle_flags.setStyle(cc::PaintFlags::kStroke_Style);
    circle_flags.setStrokeWidth(kFocusRingStrokeWidth);
    canvas->DrawCircle(GetLocalBounds().CenterPoint(),
                       width() / 2 - kFocusRingStrokeWidth, circle_flags);
  }
}

void AssistantButton::OnThemeChanged() {
  views::View::OnThemeChanged();

  // Updates inkdrop color and opacity.
  auto* ink_drop = views::InkDrop::Get(this);
  ink_drop->SetBaseColor(
      GetColorProvider()->GetColor(kColorAshInkDropOpaqueColor));
  ink_drop->SetVisibleOpacity(StyleUtil::GetInkDropOpacity());

  if (!icon_color_type_.has_value() || !icon_description_.has_value())
    return;

  // This might be the first time the image is rendered since `icon_color_type_`
  // may not resolvable until now.
  icon_description_->color = GetColorProvider()->GetColor(*icon_color_type_);
  SetImageModel(views::Button::STATE_NORMAL,
                ui::ImageModel::FromVectorIcon(*icon_description_->icon,
                                               icon_description_->color,
                                               icon_description_->dip_size));
}

void AssistantButton::OnButtonPressed() {
  assistant::util::IncrementAssistantButtonClickCount(id_);
  listener_->OnButtonPressed(id_);
}

BEGIN_METADATA(AssistantButton)
END_METADATA

}  // namespace ash
