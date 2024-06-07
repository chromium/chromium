// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_list_item_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/ash_element_identifiers.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/picker/views/picker_badge_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_preview_bubble.h"
#include "ash/picker/views/picker_preview_bubble_controller.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

constexpr auto kBorderInsetsWithoutBadge = gfx::Insets::TLBR(8, 16, 8, 16);
constexpr auto kBorderInsetsWithBadge = gfx::Insets::TLBR(8, 16, 8, 12);

constexpr gfx::Size kLeadingIconSizeDip(20, 20);
constexpr int kImageDisplayHeight = 72;
constexpr auto kLeadingIconRightPadding = gfx::Insets::TLBR(0, 0, 0, 16);
constexpr auto kBadgeLeftPadding = gfx::Insets::TLBR(0, 8, 0, 0);

// An ImageView that can optionally be masked with a circle.
class LeadingIconImageView : public views::ImageView {
  METADATA_HEADER(LeadingIconImageView, views::ImageView)

 public:
  LeadingIconImageView() = default;
  LeadingIconImageView(const LeadingIconImageView&) = delete;
  LeadingIconImageView& operator=(const LeadingIconImageView&) = delete;

  void SetCircularMaskEnabled(bool enabled) {
    if (enabled) {
      const gfx::Rect& bounds = GetImageBounds();

      // Calculate the radius of the circle based on the minimum of width and
      // height in case the icon isn't square.
      SkPath mask;
      mask.addCircle(bounds.x() + bounds.width() / 2,
                     bounds.y() + bounds.height() / 2,
                     std::min(bounds.width(), bounds.height()) / 2);
      SetClipPath(mask);
    } else {
      SetClipPath(SkPath());
    }
  }
};

BEGIN_METADATA(LeadingIconImageView)
END_METADATA

BEGIN_VIEW_BUILDER(/*no export*/, LeadingIconImageView, views::ImageView)
END_VIEW_BUILDER

}  // namespace
}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::LeadingIconImageView)

namespace ash {

PickerListItemView::PickerListItemView(SelectItemCallback select_item_callback)
    : PickerItemView(std::move(select_item_callback),
                     FocusIndicatorStyle::kFocusBar) {
  // This view only contains one child for the moment, but treat this as a
  // full-width vertical list.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  // `item_contents` is used to group child views that should not receive
  // events.
  auto* item_contents = AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetCanProcessEventsWithinSubtree(false)
          .Build());

  // The leading icon should always be preferred size.
  leading_icon_view_ = item_contents->AddChildView(
      views::Builder<LeadingIconImageView>()
          .SetImageSize(kLeadingIconSizeDip)
          .SetCanProcessEventsWithinSubtree(false)
          .SetProperty(views::kMarginsKey, kLeadingIconRightPadding)
          .Build());

  // The main container should use the remaining horizontal space.
  // Shrink to zero to allow the main contents to be elided.
  auto* main_container = item_contents->AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetProperty(
              views::kFlexBehaviorKey,
              views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                       views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded))
          .Build());
  primary_container_ = main_container->AddChildView(
      views::Builder<views::View>().SetUseDefaultFillLayout(true).Build());
  secondary_container_ = main_container->AddChildView(
      views::Builder<views::View>().SetUseDefaultFillLayout(true).Build());

  // Trailing badge should always be preferred size and centered vertically.
  trailing_badge_ = item_contents->AddChildView(
      views::Builder<PickerBadgeView>()
          .SetProperty(views::kCrossAxisAlignmentKey,
                       views::LayoutAlignment::kCenter)
          .SetProperty(views::kMarginsKey, kBadgeLeftPadding)
          .SetVisible(false)
          .Build());
  SetBadgeVisible(false);

  SetProperty(views::kElementIdentifierKey,
              kPickerSearchResultsListItemElementId);
}

PickerListItemView::~PickerListItemView() {
  if (preview_bubble_controller_ != nullptr) {
    preview_bubble_controller_->CloseBubble();
  }
}

void PickerListItemView::SetPrimaryText(const std::u16string& primary_text) {
  primary_container_->RemoveAllChildViews();
  views::Label* label = primary_container_->AddChildView(
      bubble_utils::CreateLabel(TypographyToken::kCrosBody2, primary_text,
                                cros_tokens::kCrosSysOnSurface));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);
  GetViewAccessibility().SetName(primary_text);
}

void PickerListItemView::SetPrimaryImage(
    std::unique_ptr<views::ImageView> primary_image) {
  primary_container_->RemoveAllChildViews();
  auto* image_view = primary_container_->AddChildView(std::move(primary_image));
  image_view->SetCanProcessEventsWithinSubtree(false);
  const gfx::Size original_size = image_view->GetImageModel().Size();
  if (original_size.height() > 0) {
    image_view->SetImageSize(gfx::ScaleToRoundedSize(
        original_size,
        static_cast<float>(kImageDisplayHeight) / original_size.height()));
  }
  // TODO: b/316936418 - Get accessible name for image contents.
  GetViewAccessibility().SetName(u"image contents");
}

void PickerListItemView::SetLeadingIcon(const ui::ImageModel& icon) {
  leading_icon_view_->SetImage(icon);
}

void PickerListItemView::SetSecondaryText(
    const std::u16string& secondary_text) {
  secondary_container_->RemoveAllChildViews();
  if (secondary_text.empty()) {
    return;
  }
  views::Label* label =
      secondary_container_->AddChildView(bubble_utils::CreateLabel(
          TypographyToken::kCrosAnnotation2, secondary_text,
          cros_tokens::kCrosSysOnSurfaceVariant));
  label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);
}

void PickerListItemView::SetBadgeAction(PickerActionType action) {
  switch (action) {
    case PickerActionType::kDo:
      trailing_badge_->SetText(u"");
      break;
    case PickerActionType::kInsert:
      trailing_badge_->SetText(
          l10n_util::GetStringUTF16(IDS_PICKER_RESULT_BADGE_LABEL_INSERT));
      break;
    case PickerActionType::kOpen:
      trailing_badge_->SetText(
          l10n_util::GetStringUTF16(IDS_PICKER_RESULT_BADGE_LABEL_OPEN));
      break;
    case PickerActionType::kCreate:
      trailing_badge_->SetText(
          l10n_util::GetStringUTF16(IDS_PICKER_RESULT_BADGE_LABEL_CREATE));
      break;
  }
}

void PickerListItemView::SetBadgeVisible(bool visible) {
  trailing_badge_->SetVisible(visible);

  if (visible) {
    SetBorder(views::CreateEmptyBorder(kBorderInsetsWithBadge));
  } else {
    SetBorder(views::CreateEmptyBorder(kBorderInsetsWithoutBadge));
  }
}

void PickerListItemView::SetPreview(
    PickerPreviewBubbleController* preview_bubble_controller,
    base::FilePath file_path,
    AsyncBitmapResolver async_bitmap_resolver,
    bool update_icon) {
  if (preview_bubble_controller_ != nullptr) {
    preview_bubble_controller_->CloseBubble();
  }

  async_preview_image_ = std::make_unique<ash::HoldingSpaceImage>(
      PickerPreviewBubbleView::kPreviewImageSize, file_path,
      async_bitmap_resolver);
  preview_bubble_controller_ = preview_bubble_controller;

  if (update_icon) {
    // base::Unretained is safe here since `async_icon_subscription_` is a
    // member. During destruction, `async_icon_subscription_` will be destroyed
    // before the other members, so the callback is guaranteed to be safe.
    async_preview_icon_ = std::make_unique<ash::HoldingSpaceImage>(
        kLeadingIconSizeDip, file_path, std::move(async_bitmap_resolver));
    async_icon_subscription_ = async_preview_icon_->AddImageSkiaChangedCallback(
        base::BindRepeating(&PickerListItemView::UpdateIconWithPreview,
                            base::Unretained(this)));
    UpdateIconWithPreview();
  }
}

void PickerListItemView::OnMouseEntered(const ui::MouseEvent&) {
  if (preview_bubble_controller_ != nullptr) {
    preview_bubble_controller_->ShowBubble(async_preview_image_.get(), this);
  }
}

void PickerListItemView::OnMouseExited(const ui::MouseEvent&) {
  if (preview_bubble_controller_ != nullptr) {
    preview_bubble_controller_->CloseBubble();
  }
}

std::u16string PickerListItemView::GetPrimaryTextForTesting() const {
  if (primary_container_->children().empty()) {
    return u"";
  }
  if (const auto* label = views::AsViewClass<views::Label>(
          primary_container_->children().front().get())) {
    return label->GetText();
  }
  return u"";
}

ui::ImageModel PickerListItemView::GetPrimaryImageForTesting() const {
  if (primary_container_->children().empty()) {
    return ui::ImageModel();
  }
  if (const auto* image = views::AsViewClass<views::ImageView>(
          primary_container_->children().front().get())) {
    return image->GetImageModel();
  }
  return ui::ImageModel();
}

void PickerListItemView::UpdateIconWithPreview() {
  views::AsViewClass<LeadingIconImageView>(leading_icon_view_)
      ->SetCircularMaskEnabled(true);
  SetLeadingIcon(
      ui::ImageModel::FromImageSkia(async_preview_icon_->GetImageSkia()));
}

BEGIN_METADATA(PickerListItemView)
END_METADATA

}  // namespace ash
