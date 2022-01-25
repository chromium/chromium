// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/dictation_bubble_view.h"

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "cc/paint/skottie_wrapper.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/lottie/animation.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {
constexpr int kIconSizeDip = 15;
constexpr int kSpaceBetweenIconAndTextDip = 10;

std::unique_ptr<views::ImageView> CreateImageView(
    views::ImageView** destination_view,
    const gfx::VectorIcon& icon) {
  return views::Builder<views::ImageView>()
      .CopyAddressTo(destination_view)
      .SetImage(gfx::CreateVectorIcon(
          icon, kIconSizeDip,
          AshColorProvider::Get()->GetContentLayerColor(
              AshColorProvider::ContentLayerType::kIconColorPrimary)))
      .Build();
}

void SetImageHelper(views::ImageView* image_view,
                    const gfx::VectorIcon& icon,
                    SkColor color) {
  image_view->SetImage(gfx::CreateVectorIcon(icon, kIconSizeDip, color));
}

}  // namespace

DictationBubbleView::DictationBubbleView() {
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_has_parent(false);
}

DictationBubbleView::~DictationBubbleView() = default;

void DictationBubbleView::Update(DictationBubbleIconType icon,
                                 const absl::optional<std::u16string>& text) {
  // Update visibility.
  if (use_standby_animation_) {
    standby_animation_->SetVisible(icon == DictationBubbleIconType::kStandby);
    icon == DictationBubbleIconType::kStandby ? standby_animation_->Play()
                                              : standby_animation_->Stop();
  } else {
    standby_image_->SetVisible(icon == DictationBubbleIconType::kStandby);
  }
  macro_succeeded_image_->SetVisible(icon ==
                                     DictationBubbleIconType::kMacroSuccess);
  macro_failed_image_->SetVisible(icon == DictationBubbleIconType::kMacroFail);

  // Update label.
  label_->SetVisible(text.has_value());
  label_->SetText(text.has_value() ? text.value() : std::u16string());
  SizeToContents();
}

void DictationBubbleView::OnColorModeChanged(bool dark_mode_enabled) {
  AshColorProvider* color_provider = AshColorProvider::Get();
  if (!color_provider)
    return;

  SkColor icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  SkColor text_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  if (!use_standby_animation_)
    SetImageHelper(standby_image_, kDictationBubbleIcon, icon_color);
  SetImageHelper(macro_succeeded_image_, kDictationBubbleMacroSucceededIcon,
                 icon_color);
  SetImageHelper(macro_failed_image_, kDictationBubbleMacroFailedIcon,
                 icon_color);
  label_->SetEnabledColor(text_color);
}

void DictationBubbleView::Init() {
  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  layout->set_between_child_spacing(kSpaceBetweenIconAndTextDip);
  SetLayoutManager(std::move(layout));

  UseCompactMargins();

  AddChildView(CreateStandbyView());
  AddChildView(CreateImageView(&macro_succeeded_image_,
                               kDictationBubbleMacroSucceededIcon));
  AddChildView(
      CreateImageView(&macro_failed_image_, kDictationBubbleMacroFailedIcon));
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

bool DictationBubbleView::IsStandbyViewVisibleForTesting() {
  if (use_standby_animation_)
    return standby_animation_->GetVisible();

  return standby_image_->GetVisible();
}

bool DictationBubbleView::IsMacroSucceededImageVisibleForTesting() {
  return macro_succeeded_image_->GetVisible();
}

bool DictationBubbleView::IsMacroFailedImageVisibleForTesting() {
  return macro_failed_image_->GetVisible();
}

SkColor DictationBubbleView::GetLabelBackgroundColorForTesting() {
  return label_->GetBackgroundColor();
}

SkColor DictationBubbleView::GetLabelTextColorForTesting() {
  return label_->GetEnabledColor();
}

std::unique_ptr<views::View> DictationBubbleView::CreateStandbyView() {
  absl::optional<std::string> json =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_DICTATION_BUBBLE_ANIMATION);
  if (json.has_value()) {
    use_standby_animation_ = true;
    auto skottie = cc::SkottieWrapper::CreateSerializable(
        std::vector<uint8_t>(json.value().begin(), json.value().end()));
    return views::Builder<views::AnimatedImageView>()
        .CopyAddressTo(&standby_animation_)
        .SetAnimatedImage(std::make_unique<lottie::Animation>(skottie))
        .SetImageSize(gfx::Size(32, 16))
        .Build();
  }

  use_standby_animation_ = false;
  return CreateImageView(&standby_image_, kDictationBubbleIcon);
}

std::unique_ptr<views::Label> DictationBubbleView::CreateLabel(
    const std::u16string& text) {
  return views::Builder<views::Label>()
      .CopyAddressTo(&label_)
      .SetText(text)
      .SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary))
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
      .SetMultiLine(false)
      .Build();
}

BEGIN_METADATA(DictationBubbleView, views::View)
END_METADATA

}  // namespace ash
