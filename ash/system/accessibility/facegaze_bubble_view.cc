// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/facegaze_bubble_view.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kIconSizeDip = 16;
constexpr int kSpaceBetweenIconAndTextDip = 4;

std::unique_ptr<views::Label> CreateLabelView(
    raw_ptr<views::Label>* destination_view,
    const std::u16string& text,
    ui::ColorId enabled_color_id) {
  return views::Builder<views::Label>()
      .CopyAddressTo(destination_view)
      .SetText(text)
      .SetEnabledColorId(enabled_color_id)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetMultiLine(false)
      .Build();
}

std::unique_ptr<views::ImageView> CreateImageView(
    raw_ptr<views::ImageView>* destination_view,
    const gfx::VectorIcon& icon) {
  return views::Builder<views::ImageView>()
      .CopyAddressTo(destination_view)
      .SetImage(ui::ImageModel::FromVectorIcon(icon, kColorAshTextColorPrimary,
                                               kIconSizeDip))
      .Build();
}

}  // namespace

FaceGazeBubbleView::FaceGazeBubbleView() {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_parent_window(
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_AccessibilityBubbleContainer));

  GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);
}

FaceGazeBubbleView::~FaceGazeBubbleView() = default;

void FaceGazeBubbleView::Update(const std::u16string& text) {
  label_->SetVisible(text != u"");
  label_->SetText(text);
  SizeToContents();
}

void FaceGazeBubbleView::Init() {
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  layout->set_between_child_spacing(kSpaceBetweenIconAndTextDip);
  SetLayoutManager(std::move(layout));
  UseCompactMargins();
  AddChildView(CreateImageView(&image_, kFacegazeIcon));
  AddChildView(
      CreateLabelView(&label_, std::u16string(), kColorAshTextColorPrimary));
}

void FaceGazeBubbleView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
  params->type = views::Widget::InitParams::TYPE_BUBBLE;
  params->opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params->activatable = views::Widget::InitParams::Activatable::kNo;
  params->shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params->name = "FaceGazeBubbleView";
}

const std::u16string& FaceGazeBubbleView::GetTextForTesting() const {
  return label_->GetText();
}

BEGIN_METADATA(FaceGazeBubbleView)
END_METADATA

}  // namespace ash
