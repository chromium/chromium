// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/continue_browsing_chip.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr gfx::Insets kContinueBrowsingChipPadding(8, 8);
constexpr int kTaskContinuationChipRadius = 10;

}  // namespace

ContinueBrowsingChip::ContinueBrowsingChip() : views::Button(this) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kContinueBrowsingChipPadding));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  // TODO(leandre): Update chip with phone data and updated design.
  auto* header_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TASK_CONTINUATION_TITLE)));
  header_label->SetAutoColorReadabilityEnabled(false);
  header_label->SetSubpixelRenderingEnabled(false);
  header_label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));

  auto* sub_label = AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TASK_CONTINUATION_TITLE)));
  sub_label->SetAutoColorReadabilityEnabled(false);
  sub_label->SetSubpixelRenderingEnabled(false);
  sub_label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));
}

void ContinueBrowsingChip::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));
  gfx::Rect bounds = GetContentsBounds();
  canvas->DrawRoundRect(bounds, kTaskContinuationChipRadius, flags);
  views::View::OnPaintBackground(canvas);
}

void ContinueBrowsingChip::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  // TODO(leandre): Open browser when button pressed.
}

ContinueBrowsingChip::~ContinueBrowsingChip() = default;

const char* ContinueBrowsingChip::GetClassName() const {
  return "ContinueBrowsingChip";
}

}  // namespace ash
