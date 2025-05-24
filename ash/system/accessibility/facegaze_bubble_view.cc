// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/facegaze_bubble_view.h"

#include <memory>
#include <string_view>

#include "ash/ash_element_identifiers.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr ui::ColorId kBackgroundColorId =
    cros_tokens::kCrosSysSystemBaseElevatedOpaque;
constexpr ui::ColorId kWarningBackgroundColor =
    cros_tokens::kCrosSysWarningContainer;
constexpr ui::ColorId kWarningForegroundColor =
    cros_tokens::kCrosSysOnWarningContainer;
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
      .SetEnabledColor(enabled_color_id)
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

FaceGazeBubbleView::FaceGazeBubbleView(
    const base::RepeatingCallback<void()>& on_mouse_entered,
    const base::RepeatingCallback<void(const ui::Event& event)>&
        on_close_button_clicked) {
  SetBackgroundColor(kBackgroundColorId);
  set_parent_window(
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_AccessibilityBubbleContainer));

  // Create layout and specify other properties.
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
  SetNotifyEnterExitOnChild(true);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetProperty(views::kElementIdentifierKey, kFaceGazeBubbleElementId);

  // Add main content and close button to this view.
  main_content_view_ =
      AddChildView(std::make_unique<FaceGazeBubbleMainContentView>(
          std::move(on_mouse_entered)));
  close_view_ = AddChildView(std::make_unique<FaceGazeBubbleCloseView>(
      std::move(on_close_button_clicked)));
}

FaceGazeBubbleView::~FaceGazeBubbleView() = default;

void FaceGazeBubbleView::Update(const std::u16string& text, bool is_warning) {
  main_content_view_->Update(text, is_warning);
  UpdateColor(is_warning);
  SizeToContents();
}

void FaceGazeBubbleView::UpdateColor(bool is_warning) {
  SetBackgroundColor(is_warning ? kWarningBackgroundColor : kBackgroundColorId);
}

std::u16string_view FaceGazeBubbleView::GetTextForTesting() const {
  return main_content_view_->label()->GetText();
}

BEGIN_METADATA(FaceGazeBubbleView)
END_METADATA

FaceGazeBubbleMainContentView::FaceGazeBubbleMainContentView(
    const base::RepeatingCallback<void()>& on_mouse_entered)
    : on_mouse_entered_(std::move(on_mouse_entered)) {
  // Create layout and ensure we receive mouse events.
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  layout->set_between_child_spacing(kSpaceBetweenIconAndTextDip);
  SetLayoutManager(std::move(layout));
  SetNotifyEnterExitOnChild(true);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);

  // Add child views to the view structure.
  AddChildView(CreateImageView(&image_, kFacegazeIcon));
  AddChildView(
      CreateLabelView(&label_, std::u16string(), kColorAshTextColorPrimary));
}

FaceGazeBubbleMainContentView::~FaceGazeBubbleMainContentView() = default;

void FaceGazeBubbleMainContentView::OnMouseEntered(
    const ui::MouseEvent& event) {
  on_mouse_entered_.Run();
}

void FaceGazeBubbleMainContentView::Update(const std::u16string& text,
                                           bool is_warning) {
  UpdateColor(is_warning);
  label_->SetVisible(text != u"");
  label_->SetText(text);
}

void FaceGazeBubbleMainContentView::UpdateColor(bool is_warning) {
  ui::ColorId foreground_color =
      is_warning ? kWarningForegroundColor : kColorAshTextColorPrimary;
  image_->SetImage(ui::ImageModel::FromVectorIcon(
      kFacegazeIcon, foreground_color, kIconSizeDip));
  label_->SetEnabledColor(foreground_color);
}

BEGIN_METADATA(FaceGazeBubbleMainContentView)
END_METADATA

FaceGazeBubbleCloseView::FaceGazeBubbleCloseView(
    const base::RepeatingCallback<void(const ui::Event& event)>&
        on_close_button_clicked)
    : on_close_button_clicked_(std::move(on_close_button_clicked)) {
  // Create layout.
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  SetLayoutManager(std::move(layout));

  // Add close button to view structure.
  AddChildView(CreateImageView(&close_button_, kMediumOrLargeCloseButtonIcon));
  close_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_FACEGAZE_CLOSE_BUTTON_TEXT));
}

FaceGazeBubbleCloseView::~FaceGazeBubbleCloseView() = default;

bool FaceGazeBubbleCloseView::OnMousePressed(const ui::MouseEvent& event) {
  on_close_button_clicked_.Run(event);
  return false;
}

BEGIN_METADATA(FaceGazeBubbleCloseView)
END_METADATA

}  // namespace ash
