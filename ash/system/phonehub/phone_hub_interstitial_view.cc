// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_interstitial_view.h"

#include <memory>
#include <string>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "skia/ext/image_operations.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {
constexpr auto kLabelInsets = gfx::Insets::VH(0, 4);
constexpr int kTitleLabelLineHeight = 48;
}

PhoneHubInterstitialView::PhoneHubInterstitialView(bool show_progress,
                                                   bool show_image) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::BoxLayout::Orientation::kVertical);

  auto* color_provider = AshColorProvider::Get();
  if (show_progress) {
    auto* progress_bar_container =
        AddChildView(std::make_unique<views::BoxLayoutView>());
    progress_bar_container->SetOrientation(
        views::BoxLayout::Orientation::kVertical);
    progress_bar_container->SetMainAxisAlignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    progress_bar_ = progress_bar_container->AddChildView(
        std::make_unique<views::ProgressBar>());
    progress_bar_->SetPreferredHeight(2);
    progress_bar_->SetForegroundColor(color_provider->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconColorProminent));
    progress_bar_->SetValue(-1.0);
  }

  auto* content_container =
      AddChildView(std::make_unique<views::FlexLayoutView>());
  content_container->SetOrientation(views::LayoutOrientation::kVertical);
  content_container->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  content_container->SetInteriorMargin(
      gfx::Insets::VH(0, kBubbleHorizontalSidePaddingDip) +
      gfx::Insets::TLBR(0, 0, 16, 0));

  // Set up image if any.
  if (show_image) {
    image_ =
        content_container->AddChildView(std::make_unique<views::ImageView>());
    image_->SetProperty(views::kMarginsKey, gfx::Insets::VH(20, 0));
    image_->SetProperty(views::kCrossAxisAlignmentKey,
                        views::LayoutAlignment::kCenter);
    image_->SetImageSize(gfx::Size(216, 216));
  }

  // Set up title view, which should be left-aligned.
  title_ = content_container->AddChildView(std::make_unique<views::Label>());
  title_->SetProperty(views::kCrossAxisAlignmentKey,
                      views::LayoutAlignment::kStart);
  title_->SetProperty(views::kMarginsKey, kLabelInsets);
  auto label_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  title_->SetEnabledColor(label_color);
  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton1,
                                        *title_);

  // Overriding because the typography line height set does not match Phone
  // Hub specs.
  title_->SetLineHeight(kTitleLabelLineHeight);

  // Set up multi-line description view.
  description_ =
      content_container->AddChildView(std::make_unique<views::Label>());
  description_->SetProperty(views::kMarginsKey,
                            kLabelInsets + gfx::Insets::TLBR(0, 0, 12, 0));
  description_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded, true));
  description_->SetEnabledColor(label_color);
  description_->SetMultiLine(true);
  description_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  // TODO(b/281844561): Migrate the `description_` to use a slightly lighter
  // text color when tokens have been finalized.
  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosBody2,
                                        *description_);
  description_->SetLineHeight(20);

  // Set up button container view, which should be right-aligned.
  button_container_ =
      content_container->AddChildView(std::make_unique<views::BoxLayoutView>());
  button_container_->SetProperty(views::kCrossAxisAlignmentKey,
                                 views::LayoutAlignment::kEnd);
  button_container_->SetProperty(views::kMarginsKey,
                                 gfx::Insets::TLBR(16, 0, 0, 0));
  button_container_->SetBetweenChildSpacing(8);
}

PhoneHubInterstitialView::~PhoneHubInterstitialView() = default;

void PhoneHubInterstitialView::SetImage(const ui::ImageModel& image_model) {
  // Expect a non-null |image_| view and a nonempty |image_model|.
  DCHECK(image_);
  DCHECK(!image_model.IsEmpty());
  image_->SetImage(image_model);
}

void PhoneHubInterstitialView::SetTitle(const std::u16string& title) {
  // Expect a non-empty string for the title.
  DCHECK(!title.empty());
  title_->SetText(title);
}

void PhoneHubInterstitialView::SetDescription(const std::u16string& desc) {
  // Expect a non-empty string for the description.
  DCHECK(!desc.empty());
  description_->SetText(desc);
}

void PhoneHubInterstitialView::AddButton(
    std::unique_ptr<views::Button> button) {
  description_->SetProperty(views::kMarginsKey, kLabelInsets);
  button_container_->AddChildView(std::move(button));
}

BEGIN_METADATA(PhoneHubInterstitialView)
END_METADATA

}  // namespace ash
