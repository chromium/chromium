// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/dictation_bubble_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {
constexpr int kIconSizeDip = 15;
constexpr int kSpaceBetweenIconAndTextDip = 10;
constexpr SkColor kIconAndLabelColor = SK_ColorBLACK;
constexpr SkColor kBackgroundColor = SK_ColorWHITE;
}  // namespace

DictationBubbleView::DictationBubbleView() {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_has_parent(false);
}

DictationBubbleView::~DictationBubbleView() = default;

void DictationBubbleView::Update(const absl::optional<std::u16string>& text) {
  bool visible = text.has_value();
  label_->SetVisible(visible);
  label_->SetText(visible ? text.value() : std::u16string());
  SizeToContents();
}

void DictationBubbleView::Init() {
  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  layout->set_between_child_spacing(kSpaceBetweenIconAndTextDip);
  SetLayoutManager(std::move(layout));

  UseCompactMargins();
  SetBackground(views::CreateSolidBackground(kBackgroundColor));

  AddChildView(CreateIcon());
  AddChildView(CreateLabel(std::u16string()));
}

void DictationBubbleView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  params->type = views::Widget::InitParams::TYPE_BUBBLE;
  params->opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  params->activatable = views::Widget::InitParams::Activatable::kNo;
  params->shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params->name = "DictationBubbleView";
}

void DictationBubbleView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kGenericContainer;
}

std::u16string DictationBubbleView::GetTextForTesting() {
  return label_->GetText();
}

std::unique_ptr<views::ImageView> DictationBubbleView::CreateIcon() {
  return views::Builder<views::ImageView>()
      .CopyAddressTo(&image_view_)
      .SetImage(gfx::CreateVectorIcon(kDictationBubbleIcon, kIconSizeDip,
                                      kIconAndLabelColor))
      .Build();
}

std::unique_ptr<views::Label> DictationBubbleView::CreateLabel(
    const std::u16string& text) {
  return views::Builder<views::Label>()
      .CopyAddressTo(&label_)
      .SetText(text)
      .SetEnabledColor(kIconAndLabelColor)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetMultiLine(false)
      .Build();
}

BEGIN_METADATA(DictationBubbleView, views::View)
END_METADATA

}  // namespace ash
