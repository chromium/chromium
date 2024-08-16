// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_list_item_view.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/ash_element_identifiers.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/picker/views/picker_badge_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_preview_bubble.h"
#include "ash/picker/views/picker_preview_bubble_controller.h"
#include "ash/picker/views/picker_preview_metadata.h"
#include "ash/picker/views/picker_shortcut_hint_view.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/image_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

constexpr auto kBorderInsetsWithoutBadge = gfx::Insets::TLBR(8, 16, 8, 16);
constexpr auto kBorderInsetsWithBadge = gfx::Insets::TLBR(8, 16, 8, 12);

constexpr gfx::Size kLeadingIconSizeDip(20, 20);
constexpr int kImageDisplayHeight = 64;
constexpr int kImageRadius = 8;
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
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::LayoutOrientation::kVertical));

  // `item_contents` is used to group child views that should not receive
  // events.
  // TODO: Align the leading icon to the top of the item.
  auto* item_contents =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::LayoutOrientation::kHorizontal)
                       .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
                       .SetCanProcessEventsWithinSubtree(false)
                       .Build());

  // The leading icon should always be preferred size.
  leading_icon_view_ = item_contents->AddChildView(
      views::Builder<LeadingIconImageView>()
          .SetPreferredSize(kLeadingIconSizeDip)
          .SetCanProcessEventsWithinSubtree(false)
          .SetProperty(views::kMarginsKey, kLeadingIconRightPadding)
          .Build());

  // The main container should use the remaining horizontal space.
  // Shrink to zero to allow the main contents to be elided.
  auto* main_container = item_contents->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .Build());
  item_contents->SetFlexForView(main_container, 1);
  primary_container_ = main_container->AddChildView(
      views::Builder<views::View>().SetUseDefaultFillLayout(true).Build());
  secondary_container_ = main_container->AddChildView(
      views::Builder<views::View>().SetUseDefaultFillLayout(true).Build());

  shortcut_hint_container_ = item_contents->AddChildView(
      views::Builder<views::View>().SetUseDefaultFillLayout(true).Build());

  // Trailing badge should always be preferred size.
  trailing_badge_ = item_contents->AddChildView(
      views::Builder<PickerBadgeView>()
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

void PickerListItemView::SetItemState(ItemState item_state) {
  PickerItemView::SetItemState(item_state);
  if (GetItemState() == ItemState::kPseudoFocused) {
    ShowPreview();
    shortcut_hint_container_->SetVisible(false);
  } else {
    HidePreview();
    shortcut_hint_container_->SetVisible(true);
  }
}

void PickerListItemView::SetPrimaryText(const std::u16string& primary_text) {
  primary_container_->RemoveAllChildViews();
  primary_label_ = primary_container_->AddChildView(
      views::Builder<views::Label>(
          bubble_utils::CreateLabel(TypographyToken::kCrosBody2, primary_text,
                                    cros_tokens::kCrosSysOnSurface))
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL)
          .Build());
  UpdateAccessibleName();
}

void PickerListItemView::SetPrimaryImage(const ui::ImageModel& primary_image,
                                         int available_width) {
  primary_label_ = nullptr;
  primary_container_->RemoveAllChildViews();
  auto* image_view =
      primary_container_->AddChildView(std::make_unique<views::ImageView>(
          ui::ImageModel::FromImageSkia(image_util::ResizeAndCropImage(
              primary_image.Rasterize(GetColorProvider()),
              gfx::Size(available_width - kBorderInsetsWithoutBadge.width() -
                            kLeadingIconSizeDip.width() -
                            kLeadingIconRightPadding.right(),
                        kImageDisplayHeight)))));
  image_view->SetCanProcessEventsWithinSubtree(false);
  const gfx::Size cropped_size = image_view->GetImageModel().Size();
  if (cropped_size.height() > 0) {
    SkPath path;
    path.addRoundRect(gfx::RectToSkRect(gfx::Rect(gfx::Point(), cropped_size)),
                      SkIntToScalar(kImageRadius), SkIntToScalar(kImageRadius));
    image_view->SetClipPath(path);
  }
  UpdateAccessibleName();
}

void PickerListItemView::SetLeadingIcon(const ui::ImageModel& icon,
                                        std::optional<gfx::Size> icon_size) {
  leading_icon_view_->SetImage(icon);
  leading_icon_view_->SetImageSize(icon_size.value_or(kLeadingIconSizeDip));
}

void PickerListItemView::SetSecondaryText(
    const std::u16string& secondary_text) {
  secondary_label_ = nullptr;
  secondary_container_->RemoveAllChildViews();
  if (secondary_text.empty()) {
    UpdateAccessibleName();
    return;
  }
  secondary_label_ = secondary_container_->AddChildView(
      views::Builder<views::Label>(
          bubble_utils::CreateLabel(TypographyToken::kCrosAnnotation2,
                                    secondary_text,
                                    cros_tokens::kCrosSysOnSurfaceVariant))
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL)
          .Build());
  UpdateAccessibleName();
}

void PickerListItemView::SetShortcutHintView(
    std::unique_ptr<PickerShortcutHintView> shortcut_hint_view) {
  shortcut_hint_view_ = nullptr;
  shortcut_hint_container_->RemoveAllChildViews();
  shortcut_hint_view_ =
      shortcut_hint_container_->AddChildView(std::move(shortcut_hint_view));
}

void PickerListItemView::SetBadgeAction(PickerActionType action) {
  switch (action) {
    case PickerActionType::kDo:
      trailing_badge_->SetText(u"");
      break;
    case PickerActionType::kInsert:
      trailing_badge_->SetText(
          l10n_util::GetStringUTF16(IDS_PICKER_INSERT_RESULT_BADGE_LABEL));
      break;
    case PickerActionType::kOpen:
      trailing_badge_->SetText(
          l10n_util::GetStringUTF16(IDS_PICKER_OPEN_RESULT_BADGE_LABEL));
      break;
    case PickerActionType::kCreate:
      trailing_badge_->SetText(
          l10n_util::GetStringUTF16(IDS_PICKER_CREATE_RESULT_BADGE_LABEL));
      break;
  }
  badge_action_ = action;
  UpdateAccessibleName();
}

void PickerListItemView::SetBadgeVisible(bool visible) {
  if (primary_container_ != nullptr &&
      !primary_container_->children().empty() &&
      views::IsViewClass<views::ImageView>(
          primary_container_->children().front().get())) {
    // Badge should not be visible if the list item has a primary image.
    return;
  }

  trailing_badge_->SetVisible(visible);

  if (visible) {
    SetBorder(views::CreateEmptyBorder(kBorderInsetsWithBadge));
  } else {
    SetBorder(views::CreateEmptyBorder(kBorderInsetsWithoutBadge));
  }
}

void PickerListItemView::SetPreview(
    PickerPreviewBubbleController* preview_bubble_controller,
    FileInfoResolver get_file_info,
    const base::FilePath& file_path,
    AsyncBitmapResolver async_bitmap_resolver,
    bool update_icon) {
  if (preview_bubble_controller_ != nullptr) {
    preview_bubble_controller_->CloseBubble();
  }

  async_preview_image_ = std::make_unique<ash::HoldingSpaceImage>(
      PickerPreviewBubbleView::kPreviewImageSize, file_path,
      async_bitmap_resolver);
  file_path_ = file_path;
  preview_bubble_controller_ = preview_bubble_controller;

  // Can be null in tests.
  if (!get_file_info.is_null()) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        std::move(get_file_info),
        base::BindOnce(&PickerListItemView::OnFileInfoResolved,
                       weak_ptr_factory_.GetWeakPtr()));
  }

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

void PickerListItemView::OnMouseEntered(const ui::MouseEvent& event) {
  PickerItemView::OnMouseEntered(event);
  ShowPreview();
}

void PickerListItemView::OnMouseExited(const ui::MouseEvent& event) {
  PickerItemView::OnMouseExited(event);
  HidePreview();
}

std::u16string PickerListItemView::GetPrimaryTextForTesting() const {
  return primary_label_ == nullptr ? u"" : primary_label_->GetText();
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

std::u16string_view PickerListItemView::GetSecondaryTextForTesting() const {
  if (secondary_label_ == nullptr) {
    return base::EmptyString16();
  }
  return secondary_label_->GetText();
}

void PickerListItemView::UpdateIconWithPreview() {
  views::AsViewClass<LeadingIconImageView>(leading_icon_view_)
      ->SetCircularMaskEnabled(true);
  SetLeadingIcon(
      ui::ImageModel::FromImageSkia(async_preview_icon_->GetImageSkia()));
}

std::u16string PickerListItemView::GetAccessibilityLabel() const {
  // TODO: b/316936418 - Get accessible name for image contents.
  const std::u16string& primary_accessibililty_label =
      primary_label_ == nullptr ? u"image contents" : primary_label_->GetText();
  std::u16string label =
      secondary_label_ == nullptr
          ? primary_accessibililty_label
          : l10n_util::GetStringFUTF16(IDS_PICKER_LIST_ITEM_ACCESSIBLE_NAME,
                                       primary_accessibililty_label,
                                       secondary_label_->GetText());
  if (shortcut_hint_view_ != nullptr) {
    label = l10n_util::GetStringFUTF16(
        IDS_PICKER_LIST_ITEM_WITH_SHORTCUT_ACCESSIBLE_NAME, label,
        shortcut_hint_view_->GetShortcutText());
  }

  switch (badge_action_) {
    case PickerActionType::kDo:
      return label;
    case PickerActionType::kInsert:
      return l10n_util::GetStringFUTF16(
          IDS_PICKER_LIST_ITEM_INSERT_ACTION_ACCESSIBLE_NAME, label);
    case PickerActionType::kOpen:
      return l10n_util::GetStringFUTF16(
          IDS_PICKER_LIST_ITEM_OPEN_ACTION_ACCESSIBLE_NAME, label);
    case PickerActionType::kCreate:
      // TODO: b/345303965 - Add internal strings for Create.
      return label;
  }
}

void PickerListItemView::UpdateAccessibleName() {
  GetViewAccessibility().SetName(GetAccessibilityLabel());
}

void PickerListItemView::OnFileInfoResolved(
    std::optional<base::File::Info> info) {
  file_info_ = std::move(info);

  std::u16string description = PickerGetFilePreviewDescription(file_info_);

  if (preview_bubble_controller_ != nullptr) {
    // Update the bubble main text if it's open.
    preview_bubble_controller_->SetBubbleMainText(description);
  }

  GetViewAccessibility().SetDescription(std::move(description));
}

void PickerListItemView::ShowPreview() {
  if (preview_bubble_controller_ == nullptr) {
    return;
  }

  std::u16string description = PickerGetFilePreviewDescription(file_info_);

  // Update the bubble main text before it becomes visible.
  preview_bubble_controller_->ShowBubbleAfterDelay(async_preview_image_.get(),
                                                   file_path_, this);
  preview_bubble_controller_->SetBubbleMainText(description);

  GetViewAccessibility().SetDescription(std::move(description));
}

void PickerListItemView::HidePreview() {
  if (preview_bubble_controller_ != nullptr) {
    preview_bubble_controller_->CloseBubble();
  }
}

BEGIN_METADATA(PickerListItemView)
END_METADATA

}  // namespace ash
