// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_section_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/picker_category.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/views/picker_async_preview_image_view.h"
#include "ash/picker/views/picker_gif_view.h"
#include "ash/picker/views/picker_icons.h"
#include "ash/picker/views/picker_image_item_grid_view.h"
#include "ash/picker/views/picker_image_item_row_view.h"
#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_item_with_submenu_view.h"
#include "ash/picker/views/picker_list_item_container_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_shortcut_hint_view.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chromeos/components/editor_menu/public/cpp/icon.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

// Some of the icons we use do not have a default size, so we need to manually
// set it.
constexpr int kIconSize = 20;

// Icons for browsing history should be smaller than the normal icon size.
constexpr auto kBrowsingHistoryIconSize = gfx::Size(18, 18);

constexpr auto kSectionTitleMargins = gfx::Insets::VH(8, 16);
constexpr auto kSectionTitleTrailingLinkMargins =
    gfx::Insets::TLBR(4, 8, 4, 16);

PickerCategory GetCategoryForEditorData(const PickerEditorResult& data) {
  switch (data.mode) {
    case PickerEditorResult::Mode::kWrite:
      return PickerCategory::kEditorWrite;
    case PickerEditorResult::Mode::kRewrite:
      return PickerCategory::kEditorRewrite;
  }
}

std::u16string GetLabelForNewWindowType(PickerNewWindowResult::Type type) {
  switch (type) {
    case PickerNewWindowResult::Type::kDoc:
      return l10n_util::GetStringUTF16(IDS_PICKER_NEW_GOOGLE_DOC_MENU_LABEL);
    case PickerNewWindowResult::Type::kSheet:
      return l10n_util::GetStringUTF16(IDS_PICKER_NEW_GOOGLE_SHEET_MENU_LABEL);
    case PickerNewWindowResult::Type::kSlide:
      return l10n_util::GetStringUTF16(IDS_PICKER_NEW_GOOGLE_SLIDE_MENU_LABEL);
    case PickerNewWindowResult::Type::kChrome:
      return l10n_util::GetStringUTF16(IDS_PICKER_NEW_GOOGLE_CHROME_MENU_LABEL);
  }
}

const gfx::VectorIcon& GetIconForNewWindowType(
    PickerNewWindowResult::Type type) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  switch (type) {
    case PickerNewWindowResult::Type::kDoc:
      return vector_icons::kGoogleDocsIcon;
    case PickerNewWindowResult::Type::kSheet:
      return vector_icons::kGoogleSheetsIcon;
    case PickerNewWindowResult::Type::kSlide:
      return vector_icons::kGoogleSlidesIcon;
    case PickerNewWindowResult::Type::kChrome:
      return vector_icons::kProductRefreshIcon;
  }
#else
  return kPlaceholderAppIcon;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::u16string GetLabelForCaseTransformType(
    PickerCaseTransformResult::Type type) {
  switch (type) {
    case PickerCaseTransformResult::Type::kUpperCase:
      return l10n_util::GetStringUTF16(IDS_PICKER_UPPER_CASE_MENU_LABEL);
    case PickerCaseTransformResult::Type::kLowerCase:
      return l10n_util::GetStringUTF16(IDS_PICKER_LOWER_CASE_MENU_LABEL);
    case PickerCaseTransformResult::Type::kTitleCase:
      return l10n_util::GetStringUTF16(IDS_PICKER_TITLE_CASE_MENU_LABEL);
  }
}

const gfx::VectorIcon& GetIconForCaseTransformType(
    PickerCaseTransformResult::Type type) {
  switch (type) {
    case PickerCaseTransformResult::Type::kUpperCase:
      return kPickerUpperCaseIcon;
    case PickerCaseTransformResult::Type::kLowerCase:
      return kPickerLowerCaseIcon;
    case PickerCaseTransformResult::Type::kTitleCase:
      return kPickerTitleCaseIcon;
  }
}

std::u16string FormatBrowsingHistoryUrl(const GURL& url) {
  return url_formatter::FormatUrl(
      url,
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrivialSubdomains,
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
}

std::optional<base::File::Info> ResolveFileInfo(const base::FilePath& path) {
  base::File::Info info;
  if (!base::GetFileInfo(path, &info)) {
    return std::nullopt;
  }
  return info;
}

// This should align with `chromeos::clipboard_history::GetIconForDescriptor`.
const gfx::VectorIcon& GetIconForClipboardData(
    const PickerClipboardResult& data) {
  switch (data.display_format) {
    case PickerClipboardResult::DisplayFormat::kText:
      return GURL(data.display_text).is_valid() ? vector_icons::kLinkIcon
                                                : chromeos::kTextIcon;
    case PickerClipboardResult::DisplayFormat::kImage:
      return chromeos::kFiletypeImageIcon;
    case PickerClipboardResult::DisplayFormat::kFile:
      return data.file_count == 1 ? chromeos::GetIconForPath(base::FilePath(
                                        base::UTF16ToUTF8(data.display_text)))
                                  : vector_icons::kContentCopyIcon;
    case PickerClipboardResult::DisplayFormat::kHtml:
      NOTREACHED();
  }
  NOTREACHED();
}

template <typename Range>
auto FindContainerForItem(Range&& containers, views::View* item) {
  return base::ranges::find_if(
      containers, [item](PickerTraversableItemContainer* container) {
        return container->ContainsItem(item);
      });
}

}  // namespace

PickerSectionView::PickerSectionView(
    int section_width,
    PickerAssetFetcher* asset_fetcher,
    PickerSubmenuController* submenu_controller)
    : section_width_(section_width),
      asset_fetcher_(asset_fetcher),
      submenu_controller_(submenu_controller) {
  SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  title_container_ =
      AddChildView(views::Builder<views::BoxLayoutView>()
                       .SetOrientation(views::LayoutOrientation::kHorizontal)
                       .Build());
  GetViewAccessibility().SetRole(ax::mojom::Role::kList);
}

PickerSectionView::~PickerSectionView() = default;

std::unique_ptr<PickerItemView> PickerSectionView::CreateItemFromResult(
    const PickerSearchResult& result,
    PickerPreviewBubbleController* preview_controller,
    PickerAssetFetcher* asset_fetcher,
    int available_width,
    LocalFileResultStyle local_file_result_style,
    SelectResultCallback select_result_callback) {
  using ReturnType = std::unique_ptr<PickerItemView>;
  return std::visit(
      base::Overloaded{
          [&](const PickerTextResult& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(data.primary_text);
            item_view->SetSecondaryText(data.secondary_text);
            item_view->SetLeadingIcon(data.icon);
            return item_view;
          },
          [&](const PickerSearchRequestResult& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(data.primary_text);
            item_view->SetSecondaryText(data.secondary_text);
            item_view->SetLeadingIcon(data.icon);
            return item_view;
          },
          [&](const PickerEmojiResult& data) -> ReturnType { NOTREACHED(); },
          [&](const PickerClipboardResult& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            switch (data.display_format) {
              case PickerClipboardResult::DisplayFormat::kFile:
              case PickerClipboardResult::DisplayFormat::kText:
                item_view->SetPrimaryText(data.display_text);
                break;
              case PickerClipboardResult::DisplayFormat::kImage:
                if (!data.display_image.has_value()) {
                  return nullptr;
                }
                item_view->SetPrimaryImage(*data.display_image,
                                           available_width);
                break;
              case PickerClipboardResult::DisplayFormat::kHtml:
                NOTREACHED();
            }
            item_view->SetLeadingIcon(ui::ImageModel::FromVectorIcon(
                GetIconForClipboardData(data), cros_tokens::kCrosSysOnSurface,
                kIconSize));
            return item_view;
          },
          [&](const PickerGifResult& data) -> ReturnType {
            // `base::Unretained` is safe because `asset_fetcher` outlives the
            // return value.
            auto gif_view = std::make_unique<PickerGifView>(
                base::BindRepeating(&PickerAssetFetcher::FetchGifFromUrl,
                                    base::Unretained(asset_fetcher),
                                    data.preview_url),
                base::BindRepeating(
                    &PickerAssetFetcher::FetchGifPreviewImageFromUrl,
                    base::Unretained(asset_fetcher), data.preview_image_url),
                data.preview_dimensions);
            return std::make_unique<PickerImageItemView>(
                std::move(gif_view), data.content_description,
                std::move(select_result_callback));
          },
          [&](const PickerBrowsingHistoryResult& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            std::u16string formatted_url = FormatBrowsingHistoryUrl(data.url);
            item_view->SetPrimaryText(data.title.empty() ? formatted_url
                                                         : data.title);
            item_view->SetSecondaryText(formatted_url);
            item_view->SetLeadingIcon(data.icon, kBrowsingHistoryIconSize);
            return item_view;
          },
          [&](const PickerLocalFileResult& data) -> ReturnType {
            switch (local_file_result_style) {
              case LocalFileResultStyle::kList: {
                auto item_view = std::make_unique<PickerListItemView>(
                    std::move(select_result_callback));
                item_view->SetPrimaryText(data.title);
                // `base::Unretained` is safe because `asset_fetcher` outlives
                // the return value.
                item_view->SetPreview(
                    preview_controller,
                    base::BindOnce(ResolveFileInfo, data.file_path),
                    data.file_path,
                    base::BindRepeating(&PickerAssetFetcher::FetchFileThumbnail,
                                        base::Unretained(asset_fetcher)),
                    /*update_icon=*/true);
                return item_view;
              }
              case LocalFileResultStyle::kGrid:
              case LocalFileResultStyle::kRow: {
                // `base::Unretained` is safe because `asset_fetcher` outlives
                // the return value.
                auto image_view = std::make_unique<PickerAsyncPreviewImageView>(
                    data.file_path, gfx::Size(available_width, available_width),
                    base::BindRepeating(&PickerAssetFetcher::FetchFileThumbnail,
                                        base::Unretained(asset_fetcher)));
                return std::make_unique<PickerImageItemView>(
                    std::move(image_view), data.title,
                    std::move(select_result_callback));
              }
            }
          },
          [&](const PickerDriveFileResult& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(data.title);
            // TODO: b/333609460 - Handle dark/light mode.
            item_view->SetLeadingIcon(
                ui::ImageModel::FromImageSkia(chromeos::GetIconForPath(
                    data.file_path, /*dark_background=*/false, kIconSize)));
            // `base::Unretained` is safe because `asset_fetcher` outlives the
            // return value.
            item_view->SetPreview(
                preview_controller,
                base::BindOnce(ResolveFileInfo, data.file_path), data.file_path,
                base::BindRepeating(&PickerAssetFetcher::FetchFileThumbnail,
                                    base::Unretained(asset_fetcher)),
                /*update_icon=*/false);
            return item_view;
          },
          [&](const PickerCategoryResult& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(GetLabelForPickerCategory(data.category));
            item_view->SetLeadingIcon(GetIconForPickerCategory(data.category));
            return item_view;
          },
          [&](const PickerEditorResult& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            if (data.category.has_value()) {
              // Preset write or rewrite.
              item_view->SetPrimaryText(data.display_name);
              item_view->SetLeadingIcon(ui::ImageModel::FromVectorIcon(
                  chromeos::editor_menu::GetIconForPresetQueryCategory(
                      *data.category),
                  cros_tokens::kCrosSysOnSurface));
            } else {
              // Freeform write or rewrite.
              const PickerCategory category = GetCategoryForEditorData(data);
              item_view->SetPrimaryText(GetLabelForPickerCategory(category));
              item_view->SetLeadingIcon(GetIconForPickerCategory(category));
            }
            return item_view;
          },
          [&](const PickerLobsterResult& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));

            const PickerCategory category = PickerCategory::kLobster;
            item_view->SetPrimaryText(GetLabelForPickerCategory(category));
            item_view->SetLeadingIcon(GetIconForPickerCategory(category));
            return item_view;
          },
          [&](const PickerNewWindowResult& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(GetLabelForNewWindowType(data.type));
            item_view->SetLeadingIcon(ui::ImageModel::FromVectorIcon(
                GetIconForNewWindowType(data.type),
                cros_tokens::kCrosSysOnSurface));
            return item_view;
          },
          [&](const PickerCapsLockResult& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(l10n_util::GetStringUTF16(
                data.enabled ? IDS_PICKER_CAPS_LOCK_ON_MENU_LABEL
                             : IDS_PICKER_CAPS_LOCK_OFF_MENU_LABEL));
            item_view->SetLeadingIcon(ui::ImageModel::FromVectorIcon(
                data.enabled ? kPickerCapsLockOnIcon : kPickerCapsLockOffIcon,
                cros_tokens::kCrosSysOnSurface));
            item_view->SetShortcutHintView(
                std::make_unique<PickerShortcutHintView>(data.shortcut));
            return item_view;
          },
          [&](const PickerCaseTransformResult& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(GetLabelForCaseTransformType(data.type));
            item_view->SetLeadingIcon(ui::ImageModel::FromVectorIcon(
                GetIconForCaseTransformType(data.type),
                cros_tokens::kCrosSysOnSurface));
            return item_view;
          },
      },
      result);
}

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
          .SetProperty(views::kMarginsKey, kSectionTitleMargins)
          .Build());
  title_label_->GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
  title_container_->SetFlexForView(title_label_, 1);
}

void PickerSectionView::AddTitleTrailingLink(
    const std::u16string& link_text,
    const std::u16string& accessible_name,
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
          .Build());
  title_trailing_link_->GetViewAccessibility().SetRole(
      ax::mojom::Role::kButton);
  title_trailing_link_->GetViewAccessibility().SetName(accessible_name);
}

PickerListItemView* PickerSectionView::AddListItem(
    std::unique_ptr<PickerListItemView> list_item) {
  list_item->SetSubmenuController(submenu_controller_);
  PickerListItemView* list_item_ptr =
      GetOrCreateListItemContainer()->AddListItem(std::move(list_item));
  item_views_.push_back(list_item_ptr);
  return list_item_ptr;
}

PickerImageItemView* PickerSectionView::AddImageGridItem(
    std::unique_ptr<PickerImageItemView> image_item) {
  image_item->SetSubmenuController(submenu_controller_);
  PickerImageItemView* image_item_ptr =
      GetOrCreateImageItemGrid()->AddImageItem(std::move(image_item));
  item_views_.push_back(image_item_ptr);
  return image_item_ptr;
}

PickerImageItemView* PickerSectionView::AddImageRowItem(
    std::unique_ptr<PickerImageItemView> image_item) {
  image_item->SetSubmenuController(submenu_controller_);
  PickerImageItemView* image_item_ptr =
      GetOrCreateImageItemRow()->AddImageItem(std::move(image_item));
  item_views_.push_back(image_item_ptr);
  return image_item_ptr;
}

PickerItemWithSubmenuView* PickerSectionView::AddItemWithSubmenu(
    std::unique_ptr<PickerItemWithSubmenuView> item_with_submenu) {
  PickerItemWithSubmenuView* item_ptr =
      GetOrCreateListItemContainer()->AddItemWithSubmenu(
          std::move(item_with_submenu));
  item_views_.push_back(item_ptr);
  return item_ptr;
}

PickerItemView* PickerSectionView::AddResult(
    const PickerSearchResult& result,
    PickerPreviewBubbleController* preview_controller,
    LocalFileResultStyle local_file_result_style,
    SelectResultCallback select_result_callback) {
  auto item = CreateItemFromResult(result, preview_controller, asset_fetcher_,
                                   section_width_, local_file_result_style,
                                   std::move(select_result_callback));
  if (views::IsViewClass<PickerListItemView>(item.get())) {
    return AddListItem(std::unique_ptr<PickerListItemView>(
        views::AsViewClass<PickerListItemView>(item.release())));
  }
  if (views::IsViewClass<PickerImageItemView>(item.get())) {
    std::unique_ptr<PickerImageItemView> image_item(
        views::AsViewClass<PickerImageItemView>(item.release()));
    if (local_file_result_style == LocalFileResultStyle::kRow) {
      return AddImageRowItem(std::move(image_item));
    } else {
      return AddImageGridItem(std::move(image_item));
    }
  }
  if (views::IsViewClass<PickerItemWithSubmenuView>(item.get())) {
    return AddItemWithSubmenu(std::unique_ptr<PickerItemWithSubmenuView>(
        views::AsViewClass<PickerItemWithSubmenuView>(item.release())));
  }
  NOTREACHED();
}

void PickerSectionView::ClearItems() {
  item_containers_.clear();
  item_views_.clear();
  if (image_item_grid_ != nullptr) {
    RemoveChildViewT(image_item_grid_.ExtractAsDangling());
  }
  if (list_item_container_ != nullptr) {
    RemoveChildViewT(list_item_container_.ExtractAsDangling());
  }
}

views::View* PickerSectionView::GetTopItem() {
  return item_containers_.empty() ? nullptr
                                  : item_containers_.front()->GetTopItem();
}

views::View* PickerSectionView::GetBottomItem() {
  return item_containers_.empty() ? nullptr
                                  : item_containers_.back()->GetBottomItem();
}

views::View* PickerSectionView::GetItemAbove(views::View* item) {
  auto it = FindContainerForItem(item_containers_, item);
  if (it == item_containers_.end()) {
    return nullptr;
  }

  if (views::View* result = (*it)->GetItemAbove(item)) {
    return result;
  }

  // Get the bottom item of the above container.
  return it == item_containers_.begin() ? nullptr
                                        : (*std::prev(it))->GetBottomItem();
}

views::View* PickerSectionView::GetItemBelow(views::View* item) {
  auto it = FindContainerForItem(item_containers_, item);
  if (it == item_containers_.end()) {
    return nullptr;
  }

  if (views::View* result = (*it)->GetItemBelow(item)) {
    return result;
  }

  // Get the top item of the below container.
  return it == item_containers_.end() - 1 ? nullptr
                                          : (*std::next(it))->GetTopItem();
}

views::View* PickerSectionView::GetItemLeftOf(views::View* item) {
  auto it = FindContainerForItem(item_containers_, item);
  return it == item_containers_.end() ? nullptr : (*it)->GetItemLeftOf(item);
}

views::View* PickerSectionView::GetItemRightOf(views::View* item) {
  auto it = FindContainerForItem(item_containers_, item);
  return it == item_containers_.end() ? nullptr : (*it)->GetItemRightOf(item);
}

void PickerSectionView::SetImageRowProperties(
    std::u16string accessible_name,
    base::RepeatingClosure more_items_button_callback,
    std::u16string more_items_button_accessible_name) {
  image_row_properties_.accessible_name = std::move(accessible_name);
  image_row_properties_.more_items_button_callback =
      std::move(more_items_button_callback);
  image_row_properties_.more_items_button_accessible_name =
      std::move(more_items_button_accessible_name);
}

views::View* PickerSectionView::GetImageRowMoreItemsButtonForTesting() {
  return image_item_row_ == nullptr
             ? nullptr
             : image_item_row_->GetMoreItemsButtonForTesting();  // IN-TEST
}

PickerSectionView::ImageRowProperties::ImageRowProperties() = default;

PickerSectionView::ImageRowProperties::~ImageRowProperties() = default;

PickerListItemContainerView* PickerSectionView::GetOrCreateListItemContainer() {
  if (list_item_container_ == nullptr) {
    list_item_container_ =
        AddChildView(std::make_unique<PickerListItemContainerView>());
    item_containers_.push_back(list_item_container_);
  }
  return list_item_container_;
}

PickerImageItemGridView* PickerSectionView::GetOrCreateImageItemGrid() {
  if (image_item_grid_ == nullptr) {
    image_item_grid_ =
        AddChildView(std::make_unique<PickerImageItemGridView>(section_width_));
    item_containers_.push_back(image_item_grid_);
  }
  return image_item_grid_;
}

PickerImageItemRowView* PickerSectionView::GetOrCreateImageItemRow() {
  if (image_item_row_ == nullptr) {
    image_item_row_ = AddChildView(std::make_unique<PickerImageItemRowView>(
        image_row_properties_.more_items_button_callback,
        image_row_properties_.more_items_button_accessible_name));
    image_item_row_->SetLeadingIcon(ui::ImageModel::FromVectorIcon(
        kFilesAppIcon, cros_tokens::kCrosSysOnSurface, kIconSize));
    image_item_row_->GetViewAccessibility().SetName(
        image_row_properties_.accessible_name);
    item_containers_.push_back(image_item_row_);
  }
  return image_item_row_;
}

BEGIN_METADATA(PickerSectionView)
END_METADATA

}  // namespace ash
