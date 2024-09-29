// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/facegaze_bubble_view.h"

#include <memory>

#include "ash/ash_element_identifiers.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr ui::ColorId kBackgroundColor =
    cros_tokens::kCrosSysSystemBaseElevatedOpaque;
constexpr int kIconSizeDip = 24;
constexpr int kLeftRightMarginDip = 20;
constexpr int kRoundedCornerRadius = 24.f;
constexpr int kSpaceBetweenIconAndTextDip = 16;
constexpr int kTopBottomMarginDip = 12;
const ui::ResourceBundle::FontStyle kKeyLabelFontStyle =
    ui::ResourceBundle::MediumFont;

std::unique_ptr<views::Label> CreateLabelView(
    raw_ptr<views::Label>* destination_view,
    const std::u16string& text,
    ui::ColorId enabled_color_id) {
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();

  return views::Builder<views::Label>()
      .CopyAddressTo(destination_view)
      .SetText(text)
      .SetEnabledColorId(enabled_color_id)
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER)
      .SetMultiLine(false)
      .SetFontList(rb->GetFontList(kKeyLabelFontStyle))
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
  set_parent_window(
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_AccessibilityBubbleContainer));

  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  layout->set_between_child_spacing(kSpaceBetweenIconAndTextDip);
  SetLayoutManager(std::move(layout));
  set_margins(gfx::Insets()
                  .set_top(kTopBottomMarginDip)
                  .set_bottom(kTopBottomMarginDip)
                  .set_left(kLeftRightMarginDip)
                  .set_right(kLeftRightMarginDip));
  set_corner_radius(kRoundedCornerRadius);
  set_highlight_button_when_shown(false);
  SetCanActivate(false);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetProperty(views::kElementIdentifierKey, kFaceGazeBubbleElementId);

  AddChildView(CreateImageView(&image_, kFacegazeIcon));
  AddChildView(
      CreateLabelView(&label_, std::u16string(), kColorAshTextColorPrimary));
}

FaceGazeBubbleView::~FaceGazeBubbleView() = default;

void FaceGazeBubbleView::Update(const std::u16string& text) {
  label_->SetVisible(text != u"");
  label_->SetText(text);
  SizeToContents();
}

void FaceGazeBubbleView::OnThemeChanged() {
  BubbleDialogDelegateView::OnThemeChanged();
  set_color(GetColorProvider()->GetColor(kBackgroundColor));
}

const std::u16string& FaceGazeBubbleView::GetTextForTesting() const {
  return label_->GetText();
}

BEGIN_METADATA(FaceGazeBubbleView)
END_METADATA

}  // namespace ash
