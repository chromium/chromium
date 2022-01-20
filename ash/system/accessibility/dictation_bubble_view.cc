// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/dictation_bubble_view.h"

#include "ash/public/cpp/accessibility_controller_enums.h"
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
}  // namespace

DictationBubbleView::DictationBubbleView() {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_has_parent(false);
}

DictationBubbleView::~DictationBubbleView() = default;

void DictationBubbleView::Update(DictationBubbleIconType icon,
                                 const absl::optional<std::u16string>& text) {
  // Update icon visibility.
  standby_image_->SetVisible(icon == DictationBubbleIconType::kStandby);
  macro_succeeded_image_->SetVisible(icon ==
                                     DictationBubbleIconType::kMacroSuccess);
  macro_failed_image_->SetVisible(icon == DictationBubbleIconType::kMacroFail);

  // Update label.
  label_->SetVisible(text.has_value());
  label_->SetText(text.has_value() ? text.value() : std::u16string());
  SizeToContents();
}

void DictationBubbleView::Init() {
  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  layout->set_between_child_spacing(kSpaceBetweenIconAndTextDip);
  SetLayoutManager(std::move(layout));

  UseCompactMargins();

  auto create_image_view = [](views::ImageView** destination_view,
                              const gfx::VectorIcon& icon) {
    return views::Builder<views::ImageView>()
        .CopyAddressTo(destination_view)
        .SetImage(gfx::CreateVectorIcon(icon, kIconSizeDip, kIconAndLabelColor))
        .Build();
  };

  AddChildView(create_image_view(&standby_image_, kDictationBubbleIcon));
  AddChildView(create_image_view(&macro_succeeded_image_,
                                 kDictationBubbleMacroSucceededIcon));
  AddChildView(
      create_image_view(&macro_failed_image_, kDictationBubbleMacroFailedIcon));
  AddChildView(CreateLabel(std::u16string()));
}

void DictationBubbleView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  params->type = views::Widget::InitParams::TYPE_BUBBLE;
  params->opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
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

bool DictationBubbleView::IsStandbyImageVisibleForTesting() {
  return standby_image_->GetVisible();
}

bool DictationBubbleView::IsMacroSucceededImageVisibleForTesting() {
  return macro_succeeded_image_->GetVisible();
}

bool DictationBubbleView::IsMacroFailedImageVisibleForTesting() {
  return macro_failed_image_->GetVisible();
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
