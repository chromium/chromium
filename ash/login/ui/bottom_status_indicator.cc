// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/login/ui/bottom_status_indicator.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

BottomStatusIndicator::BottomStatusIndicator(TappedCallback on_tapped_callback)
    : LabelButton(std::move(on_tapped_callback)) {
  label()->SetAutoColorReadabilityEnabled(false);
  label()->SetFontList(
      views::Label::GetDefaultFontList().DeriveWithSizeDelta(1));
  label()->SetSubpixelRenderingEnabled(false);

  SetFocusBehavior(FocusBehavior::ALWAYS);

  SetVisible(false);
}

BottomStatusIndicator::~BottomStatusIndicator() = default;

void BottomStatusIndicator::SetIcon(const gfx::VectorIcon& vector_icon,
                                    AshColorProvider::ContentLayerType type) {
  SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(
          vector_icon, AshColorProvider::Get()->GetContentLayerColor(type)));
}

void BottomStatusIndicator::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = role_;
  node_data->SetName(label()->GetText());
}

}  // namespace ash
