// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/base/assistant_button.h"

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/base/assistant_button_listener.h"
#include "ash/assistant/util/histogram_util.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

// Appearance.
constexpr float kInkDropHighlightOpacity = 0.08f;
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
  constexpr SkColor kInkDropBaseColor = SK_ColorBLACK;
  constexpr float kInkDropVisibleOpacity = 0.06f;

  // Avoid drawing default focus rings since Assistant buttons use
  // a custom highlight on focus.
  SetInstallFocusRingOnFocus(false);

  // Image.
  SetFlipCanvasOnPaintForRTLUI(false);
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);

  // Ink drop.
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  views::InkDrop::Get(this)->SetBaseColor(kInkDropBaseColor);
  views::InkDrop::Get(this)->SetVisibleOpacity(kInkDropVisibleOpacity);
  views::InstallCircleHighlightPathGenerator(this, gfx::Insets(kInkDropInset));
  views::InkDrop::UseInkDropForFloodFillRipple(views::InkDrop::Get(this));
  views::InkDrop::Get(this)->SetCreateHighlightCallback(base::BindRepeating(
      [](Button* host) {
        auto highlight = std::make_unique<views::InkDropHighlight>(
            gfx::SizeF(host->size()),
            views::InkDrop::Get(host)->GetBaseColor());
        highlight->set_visible_opacity(kInkDropHighlightOpacity);
        return highlight;
      },
      this));
  views::InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
      [](Button* host) -> std::unique_ptr<views::InkDropRipple> {
        return std::make_unique<views::FloodFillInkDropRipple>(
            host->size(), gfx::Insets(kInkDropInset),
            views::InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
            views::InkDrop::Get(host)->GetBaseColor(),
            views::InkDrop::Get(host)->GetVisibleOpacity());
      },
      this));
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
  button->SetAccessibleName(
      l10n_util::GetStringUTF16(params.accessible_name_id.value()));

  if (params.tooltip_id) {
    button->SetTooltipText(
        l10n_util::GetStringUTF16(params.tooltip_id.value()));
  }

  button->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(icon, params.icon_size_in_dip, params.icon_color));
  button->SetPreferredSize(gfx::Size(params.size_in_dip, params.size_in_dip));
  return button;
}

void AssistantButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // Note that the current assumption is that button bounds are square.
  DCHECK_EQ(width(), height());
  SetFocusPainter(views::Painter::CreateSolidRoundRectPainter(
      SkColorSetA(views::InkDrop::Get(this)->GetBaseColor(),
                  0xff * kInkDropHighlightOpacity),
      width() / 2 - kInkDropInset, gfx::Insets(kInkDropInset)));
}

void AssistantButton::OnButtonPressed() {
  assistant::util::IncrementAssistantButtonClickCount(id_);
  listener_->OnButtonPressed(id_);
}

BEGIN_METADATA(AssistantButton, views::ImageButton)
END_METADATA

}  // namespace ash
