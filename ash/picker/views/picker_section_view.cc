// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_section_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/views/picker_emoji_item_view.h"
#include "ash/picker/views/picker_emoticon_item_view.h"
#include "ash/picker/views/picker_gif_view.h"
#include "ash/picker/views/picker_icons.h"
#include "ash/picker/views/picker_image_item_grid_view.h"
#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_list_item_container_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_small_item_grid_view.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/picker/views/picker_symbol_item_view.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/functional/overloaded.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

// Some of the icons we use do not have a default size, so we need to manually
// set it.
constexpr int kIconSize = 20;

constexpr auto kSectionTitleMargins = gfx::Insets::VH(8, 16);
constexpr auto kSectionTitleTrailingLinkMargins =
    gfx::Insets::TLBR(4, 8, 4, 16);

PickerCategory GetCategoryForEditorData(
    const PickerSearchResult::EditorData& data) {
  switch (data.mode) {
    case PickerSearchResult::EditorData::Mode::kWrite:
      return PickerCategory::kEditorWrite;
    case PickerSearchResult::EditorData::Mode::kRewrite:
      return PickerCategory::kEditorRewrite;
  }
}

}  // namespace

PickerSectionView::PickerSectionView(int section_width,
                                     PickerAssetFetcher* asset_fetcher)
    : section_width_(section_width), asset_fetcher_(asset_fetcher) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  title_container_ =
      AddChildView(views::Builder<views::FlexLayoutView>()
                       .SetOrientation(views::LayoutOrientation::kHorizontal)
                       .Build());
}

PickerSectionView::~PickerSectionView() = default;

void PickerSectionView::AddTitleLabel(const std::u16string& title_text) {
  if (title_text.empty()) {
    return;
  }

  title_label_ = title_container_->AddChildView(
      views::Builder<views::Label>(
          bubble_utils::CreateLabel(TypographyToken::kCrosAnnotation2,
                                    title_text,
                                    cros_tokens::kCrosSysOnSurfaceVariant))
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetProperty(views::kFlexBehaviorKey,
                       views::FlexSpecification(
                           views::LayoutOrientation::kHorizontal,
                           views::MinimumFlexSizeRule::kScaleToMinimum,
                           views::MaximumFlexSizeRule::kUnbounded)
                           .WithWeight(1))
          .SetProperty(views::kMarginsKey, kSectionTitleMargins)
          .Build());
}

void PickerSectionView::AddTitleTrailingLink(
    const std::u16string& link_text,
    views::Link::ClickedCallback link_callback) {
  title_trailing_link_ = title_container_->AddChildView(
      views::Builder<views::Link>()
          .SetText(link_text)
          .SetCallback(link_callback)
          .SetFontList(ash::TypographyProvider::Get()->ResolveTypographyToken(
              ash::TypographyToken::kCrosAnnotation2))
          .SetEnabledColorId(cros_tokens::kCrosSysPrimary)
          .SetForceUnderline(false)
          .SetProperty(views::kMarginsKey, kSectionTitleTrailingLinkMargins)
          .SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY)
          .Build());
}

PickerListItemView* PickerSectionView::AddListItem(
    std::unique_ptr<PickerListItemView> list_item) {
  if (list_item_container_ == nullptr) {
    list_item_container_ =
        AddChildView(std::make_unique<PickerListItemContainerView>());
  }
  PickerListItemView* list_item_ptr =
      list_item_container_->AddListItem(std::move(list_item));
  item_views_.push_back(list_item_ptr);
  return list_item_ptr;
}

PickerEmojiItemView* PickerSectionView::AddEmojiItem(
    std::unique_ptr<PickerEmojiItemView> emoji_item) {
  CreateSmallItemGridIfNeeded();
  PickerEmojiItemView* emoji_item_ptr =
      small_item_grid_->AddEmojiItem(std::move(emoji_item));
  item_views_.push_back(emoji_item_ptr);
  return emoji_item_ptr;
}

PickerSymbolItemView* PickerSectionView::AddSymbolItem(
    std::unique_ptr<PickerSymbolItemView> symbol_item) {
  CreateSmallItemGridIfNeeded();
  PickerSymbolItemView* symbol_item_ptr =
      small_item_grid_->AddSymbolItem(std::move(symbol_item));
  item_views_.push_back(symbol_item_ptr);
  return symbol_item_ptr;
}

PickerEmoticonItemView* PickerSectionView::AddEmoticonItem(
    std::unique_ptr<PickerEmoticonItemView> emoticon_item) {
  CreateSmallItemGridIfNeeded();
  PickerEmoticonItemView* emoticon_item_ptr =
      small_item_grid_->AddEmoticonItem(std::move(emoticon_item));
  item_views_.push_back(emoticon_item_ptr);
  return emoticon_item_ptr;
}

PickerImageItemView* PickerSectionView::AddImageItem(
    std::unique_ptr<PickerImageItemView> image_item) {
  if (image_item_grid_ == nullptr) {
    image_item_grid_ =
        AddChildView(std::make_unique<PickerImageItemGridView>(section_width_));
  }
  PickerImageItemView* image_item_ptr =
      image_item_grid_->AddImageItem(std::move(image_item));
  item_views_.push_back(image_item_ptr);
  return image_item_ptr;
}

PickerItemView* PickerSectionView::AddResult(
    const PickerSearchResult& result,
    PickerPreviewBubbleController* preview_controller,
    SelectResultCallback select_result_callback) {
  return std::visit(
      base::Overloaded{
          [&](const PickerSearchResult::TextData& data) -> PickerItemView* {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(data.primary_text);
            item_view->SetSecondaryText(data.secondary_text);
            item_view->SetLeadingIcon(data.icon);
            return AddListItem(std::move(item_view));
          },
          [&](const PickerSearchResult::SearchRequestData& data)
              -> PickerItemView* {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(data.text);
            item_view->SetLeadingIcon(data.icon);
            return AddListItem(std::move(item_view));
          },
          [&](const PickerSearchResult::EmojiData& data) -> PickerItemView* {
            auto emoji_item = std::make_unique<PickerEmojiItemView>(
                std::move(select_result_callback), data.emoji);
            return AddEmojiItem(std::move(emoji_item));
          },
          [&](const PickerSearchResult::SymbolData& data) -> PickerItemView* {
            auto symbol_item = std::make_unique<PickerSymbolItemView>(
                std::move(select_result_callback), data.symbol);
            return AddSymbolItem(std::move(symbol_item));
          },
          [&](const PickerSearchResult::EmoticonData& data) -> PickerItemView* {
            auto emoticon_item = std::make_unique<PickerEmoticonItemView>(
                std::move(select_result_callback), data.emoticon);
            return AddEmoticonItem(std::move(emoticon_item));
          },
          [&](const PickerSearchResult::ClipboardData& data)
              -> PickerItemView* {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            const gfx::VectorIcon* icon = nullptr;
            switch (data.display_format) {
              case PickerSearchResult::ClipboardData::DisplayFormat::kFile:
                icon = &vector_icons::kContentCopyIcon;
                item_view->SetPrimaryText(data.display_text);
                break;
              case PickerSearchResult::ClipboardData::DisplayFormat::kText:
                icon = &chromeos::kTextIcon;
                item_view->SetPrimaryText(data.display_text);
                break;
              case PickerSearchResult::ClipboardData::DisplayFormat::kImage:
                if (!data.display_image.has_value()) {
                  return nullptr;
                }
                icon = &chromeos::kFiletypeImageIcon;
                item_view->SetPrimaryImage(
                    std::make_unique<views::ImageView>(*data.display_image));
                break;
              case PickerSearchResult::ClipboardData::DisplayFormat::kHtml:
                icon = &vector_icons::kCodeIcon;
                item_view->SetPrimaryText(
                    l10n_util::GetStringUTF16(IDS_PICKER_HTML_CONTENT));
                break;
            }
            if (icon) {
              item_view->SetLeadingIcon(ui::ImageModel::FromVectorIcon(
                  *icon, cros_tokens::kCrosSysOnSurface, kIconSize));
            }
            return AddListItem(std::move(item_view));
          },
          [&,
           this](const PickerSearchResult::GifData& data) -> PickerItemView* {
            // `base::Unretained` is safe here because `this` will own the gif
            // view and `asset_fetcher_` outlives `this`.
            auto gif_view = std::make_unique<PickerGifView>(
                base::BindRepeating(&PickerAssetFetcher::FetchGifFromUrl,
                                    base::Unretained(asset_fetcher_),
                                    data.preview_url),
                base::BindRepeating(
                    &PickerAssetFetcher::FetchGifPreviewImageFromUrl,
                    base::Unretained(asset_fetcher_), data.preview_image_url),
                data.preview_dimensions,
                /*accessible_name=*/data.content_description);
            auto gif_item_view = std::make_unique<PickerImageItemView>(
                std::move(select_result_callback), std::move(gif_view));
            return AddImageItem(std::move(gif_item_view));
          },
          [&](const PickerSearchResult::BrowsingHistoryData& data)
              -> PickerItemView* {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(data.title);
            item_view->SetSecondaryText(base::UTF8ToUTF16(data.url.spec()));
            item_view->SetLeadingIcon(data.icon);
            return AddListItem(std::move(item_view));
          },
          [&](const PickerSearchResult::LocalFileData& data)
              -> PickerItemView* {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(data.title);
            item_view->SetPreview(
                preview_controller, data.file_path,
                // base::Unretained is safe here since asset_fetcher_ outlives
                // this class.
                base::BindRepeating(&PickerAssetFetcher::FetchFileThumbnail,
                                    base::Unretained(asset_fetcher_)),
                /*update_icon=*/true);
            return AddListItem(std::move(item_view));
          },
          [&](const PickerSearchResult::DriveFileData& data)
              -> PickerItemView* {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(data.title);
            // TODO: b/333609460 - Handle dark/light mode.
            item_view->SetLeadingIcon(
                ui::ImageModel::FromImageSkia(chromeos::GetIconForPath(
                    data.file_path, /*dark_background=*/false, kIconSize)));
            item_view->SetPreview(
                preview_controller, data.file_path,
                // base::Unretained is safe here since asset_fetcher_ outlives
                // this class.
                base::BindRepeating(&PickerAssetFetcher::FetchFileThumbnail,
                                    base::Unretained(asset_fetcher_)),
                /*update_icon=*/false);
            return AddListItem(std::move(item_view));
          },
          [&](const PickerSearchResult::CategoryData& data) -> PickerItemView* {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(GetLabelForPickerCategory(data.category));
            item_view->SetLeadingIcon(GetIconForPickerCategory(data.category));
            return AddListItem(std::move(item_view));
          },
          [&](const PickerSearchResult::EditorData& data) -> PickerItemView* {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            const PickerCategory category = GetCategoryForEditorData(data);
            item_view->SetPrimaryText(GetLabelForPickerCategory(category));
            item_view->SetLeadingIcon(GetIconForPickerCategory(category));
            return AddListItem(std::move(item_view));
          },
      },
      result.data());
}

void PickerSectionView::ClearItems() {
  item_views_.clear();
  if (image_item_grid_ != nullptr) {
    RemoveChildViewT(image_item_grid_.ExtractAsDangling());
  }
  if (small_item_grid_ != nullptr) {
    RemoveChildViewT(small_item_grid_.ExtractAsDangling());
  }
  if (list_item_container_ != nullptr) {
    RemoveChildViewT(list_item_container_.ExtractAsDangling());
  }
}

views::View* PickerSectionView::GetTopItem() {
  return GetItemContainer() != nullptr ? GetItemContainer()->GetTopItem()
                                       : nullptr;
}

views::View* PickerSectionView::GetBottomItem() {
  return GetItemContainer() != nullptr ? GetItemContainer()->GetBottomItem()
                                       : nullptr;
}

views::View* PickerSectionView::GetItemAbove(views::View* item) {
  return GetItemContainer() != nullptr ? GetItemContainer()->GetItemAbove(item)
                                       : nullptr;
}

views::View* PickerSectionView::GetItemBelow(views::View* item) {
  return GetItemContainer() != nullptr ? GetItemContainer()->GetItemBelow(item)
                                       : nullptr;
}

views::View* PickerSectionView::GetItemLeftOf(views::View* item) {
  return GetItemContainer() != nullptr ? GetItemContainer()->GetItemLeftOf(item)
                                       : nullptr;
}

views::View* PickerSectionView::GetItemRightOf(views::View* item) {
  return GetItemContainer() != nullptr
             ? GetItemContainer()->GetItemRightOf(item)
             : nullptr;
}

void PickerSectionView::CreateSmallItemGridIfNeeded() {
  if (small_item_grid_ == nullptr) {
    small_item_grid_ =
        AddChildView(std::make_unique<PickerSmallItemGridView>(section_width_));
  }
}

PickerTraversableItemContainer* PickerSectionView::GetItemContainer() {
  if (list_item_container_ != nullptr) {
    return list_item_container_;
  } else if (image_item_grid_ != nullptr) {
    return image_item_grid_;
  } else {
    return small_item_grid_;
  }
}

BEGIN_METADATA(PickerSectionView)
END_METADATA

}  // namespace ash
