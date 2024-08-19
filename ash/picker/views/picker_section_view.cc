// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_section_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/views/picker_gif_view.h"
#include "ash/picker/views/picker_icons.h"
#include "ash/picker/views/picker_image_item_grid_view.h"
#include "ash/picker/views/picker_image_item_view.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_item_with_submenu_view.h"
#include "ash/picker/views/picker_list_item_container_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_shortcut_hint_view.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/functional/overloaded.h"
#include "base/notreached.h"
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

PickerCategory GetCategoryForEditorData(
    const PickerSearchResult::EditorData& data) {
  switch (data.mode) {
    case PickerSearchResult::EditorData::Mode::kWrite:
      return PickerCategory::kEditorWrite;
    case PickerSearchResult::EditorData::Mode::kRewrite:
      return PickerCategory::kEditorRewrite;
  }
}

std::u16string GetLabelForNewWindowType(
    PickerSearchResult::NewWindowData::Type type) {
  switch (type) {
    case PickerSearchResult::NewWindowData::Type::kDoc:
      return l10n_util::GetStringUTF16(IDS_PICKER_NEW_GOOGLE_DOC_MENU_LABEL);
    case PickerSearchResult::NewWindowData::Type::kSheet:
      return l10n_util::GetStringUTF16(IDS_PICKER_NEW_GOOGLE_SHEET_MENU_LABEL);
    case PickerSearchResult::NewWindowData::Type::kSlide:
      return l10n_util::GetStringUTF16(IDS_PICKER_NEW_GOOGLE_SLIDE_MENU_LABEL);
    case PickerSearchResult::NewWindowData::Type::kChrome:
      return l10n_util::GetStringUTF16(IDS_PICKER_NEW_GOOGLE_CHROME_MENU_LABEL);
  }
}

const gfx::VectorIcon& GetIconForNewWindowType(
    PickerSearchResult::NewWindowData::Type type) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  switch (type) {
    case PickerSearchResult::NewWindowData::Type::kDoc:
      return vector_icons::kGoogleDocsIcon;
    case PickerSearchResult::NewWindowData::Type::kSheet:
      return vector_icons::kGoogleSheetsIcon;
    case PickerSearchResult::NewWindowData::Type::kSlide:
      return vector_icons::kGoogleSlidesIcon;
    case PickerSearchResult::NewWindowData::Type::kChrome:
      return vector_icons::kProductRefreshIcon;
  }
#else
  return kPlaceholderAppIcon;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::u16string GetLabelForCaseTransformType(
    PickerSearchResult::CaseTransformData::Type type) {
  switch (type) {
    case PickerSearchResult::CaseTransformData::Type::kUpperCase:
      return l10n_util::GetStringUTF16(IDS_PICKER_UPPER_CASE_MENU_LABEL);
    case PickerSearchResult::CaseTransformData::Type::kLowerCase:
      return l10n_util::GetStringUTF16(IDS_PICKER_LOWER_CASE_MENU_LABEL);
    case PickerSearchResult::CaseTransformData::Type::kTitleCase:
      return l10n_util::GetStringUTF16(IDS_PICKER_TITLE_CASE_MENU_LABEL);
  }
}

const gfx::VectorIcon& GetIconForCaseTransformType(
    PickerSearchResult::CaseTransformData::Type type) {
  switch (type) {
    case PickerSearchResult::CaseTransformData::Type::kUpperCase:
      return kPickerUpperCaseIcon;
    case PickerSearchResult::CaseTransformData::Type::kLowerCase:
      return kPickerLowerCaseIcon;
    case PickerSearchResult::CaseTransformData::Type::kTitleCase:
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
    const PickerSearchResult::ClipboardData& data) {
  switch (data.display_format) {
    case PickerSearchResult::ClipboardData::DisplayFormat::kText:
      return chromeos::kTextIcon;
    case PickerSearchResult::ClipboardData::DisplayFormat::kUrl:
      return vector_icons::kLinkIcon;
    case PickerSearchResult::ClipboardData::DisplayFormat::kImage:
      return chromeos::kFiletypeImageIcon;
    case PickerSearchResult::ClipboardData::DisplayFormat::kFile:
      return data.file_count == 1 ? chromeos::GetIconForPath(base::FilePath(
                                        base::UTF16ToUTF8(data.display_text)))
                                  : vector_icons::kContentCopyIcon;
    case PickerSearchResult::ClipboardData::DisplayFormat::kHtml:
      NOTREACHED();
  }
  NOTREACHED();
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
    SelectResultCallback select_result_callback) {
  using ReturnType = std::unique_ptr<PickerItemView>;
  return std::visit(
      base::Overloaded{
          [&](const PickerSearchResult::TextData& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(data.primary_text);
            item_view->SetSecondaryText(data.secondary_text);
            item_view->SetLeadingIcon(data.icon);
            return item_view;
          },
          [&](const PickerSearchResult::SearchRequestData& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(data.primary_text);
            item_view->SetSecondaryText(data.secondary_text);
            item_view->SetLeadingIcon(data.icon);
            return item_view;
          },
          [&](const PickerSearchResult::EmojiData& data) -> ReturnType {
            NOTREACHED();
          },
          [&](const PickerSearchResult::ClipboardData& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            switch (data.display_format) {
              case PickerSearchResult::ClipboardData::DisplayFormat::kFile:
              case PickerSearchResult::ClipboardData::DisplayFormat::kText:
              case PickerSearchResult::ClipboardData::DisplayFormat::kUrl:
                item_view->SetPrimaryText(data.display_text);
                break;
              case PickerSearchResult::ClipboardData::DisplayFormat::kImage:
                if (!data.display_image.has_value()) {
                  return nullptr;
                }
                item_view->SetPrimaryImage(*data.display_image,
                                           available_width);
                break;
              case PickerSearchResult::ClipboardData::DisplayFormat::kHtml:
                NOTREACHED();
            }
            item_view->SetLeadingIcon(ui::ImageModel::FromVectorIcon(
                GetIconForClipboardData(data), cros_tokens::kCrosSysOnSurface,
                kIconSize));
            return item_view;
          },
          [&](const PickerSearchResult::BrowsingHistoryData& data)
              -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            std::u16string formatted_url = FormatBrowsingHistoryUrl(data.url);
            item_view->SetPrimaryText(data.title.empty() ? formatted_url
                                                         : data.title);
            item_view->SetSecondaryText(formatted_url);
            item_view->SetLeadingIcon(data.icon, kBrowsingHistoryIconSize);
            return item_view;
          },
          [&](const PickerSearchResult::LocalFileData& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(data.title);
            // `base::Unretained` is safe because `asset_fetcher` outlives the
            // return value.
            item_view->SetPreview(
                preview_controller,
                base::BindOnce(ResolveFileInfo, data.file_path), data.file_path,
                base::BindRepeating(&PickerAssetFetcher::FetchFileThumbnail,
                                    base::Unretained(asset_fetcher)),
                /*update_icon=*/true);
            return item_view;
          },
          [&](const PickerSearchResult::DriveFileData& data) -> ReturnType {
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
          [&](const PickerSearchResult::CategoryData& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(GetLabelForPickerCategory(data.category));
            item_view->SetLeadingIcon(GetIconForPickerCategory(data.category));
            return item_view;
          },
          [&](const PickerSearchResult::EditorData& data) -> ReturnType {
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
          [&](const PickerSearchResult::NewWindowData& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(GetLabelForNewWindowType(data.type));
            item_view->SetLeadingIcon(ui::ImageModel::FromVectorIcon(
                GetIconForNewWindowType(data.type),
                cros_tokens::kCrosSysOnSurface));
            return item_view;
          },
          [&](const PickerSearchResult::CapsLockData& data) -> ReturnType {
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
          [&](const PickerSearchResult::CaseTransformData& data) -> ReturnType {
            auto item_view = std::make_unique<PickerListItemView>(
                std::move(select_result_callback));
            item_view->SetPrimaryText(GetLabelForCaseTransformType(data.type));
            item_view->SetLeadingIcon(ui::ImageModel::FromVectorIcon(
                GetIconForCaseTransformType(data.type),
                cros_tokens::kCrosSysOnSurface));
            return item_view;
          },
      },
      result.data());
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
  title_trailing_link_->GetViewAccessibility().SetProperties(
      ax::mojom::Role::kButton, accessible_name);
}

PickerListItemView* PickerSectionView::AddListItem(
    std::unique_ptr<PickerListItemView> list_item) {
  if (list_item_container_ == nullptr) {
    list_item_container_ =
        AddChildView(std::make_unique<PickerListItemContainerView>());
  }
  list_item->SetSubmenuController(submenu_controller_);
  PickerListItemView* list_item_ptr =
      list_item_container_->AddListItem(std::move(list_item));
  item_views_.push_back(list_item_ptr);
  return list_item_ptr;
}

PickerImageItemView* PickerSectionView::AddImageItem(
    std::unique_ptr<PickerImageItemView> image_item) {
  if (image_item_grid_ == nullptr) {
    image_item_grid_ =
        AddChildView(std::make_unique<PickerImageItemGridView>(section_width_));
  }
  image_item->SetSubmenuController(submenu_controller_);
  PickerImageItemView* image_item_ptr =
      image_item_grid_->AddImageItem(std::move(image_item));
  item_views_.push_back(image_item_ptr);
  return image_item_ptr;
}

PickerItemWithSubmenuView* PickerSectionView::AddItemWithSubmenu(
    std::unique_ptr<PickerItemWithSubmenuView> item_with_submenu) {
  if (list_item_container_ == nullptr) {
    list_item_container_ =
        AddChildView(std::make_unique<PickerListItemContainerView>());
  }
  PickerItemWithSubmenuView* item_ptr =
      list_item_container_->AddItemWithSubmenu(std::move(item_with_submenu));
  item_views_.push_back(item_ptr);
  return item_ptr;
}

PickerItemView* PickerSectionView::AddItem(
    std::unique_ptr<PickerItemView> item) {
  if (views::IsViewClass<PickerListItemView>(item.get())) {
    return AddListItem(std::unique_ptr<PickerListItemView>(
        views::AsViewClass<PickerListItemView>(item.release())));
  }
  if (views::IsViewClass<PickerImageItemView>(item.get())) {
    return AddImageItem(std::unique_ptr<PickerImageItemView>(
        views::AsViewClass<PickerImageItemView>(item.release())));
  }
  if (views::IsViewClass<PickerItemWithSubmenuView>(item.get())) {
    return AddItemWithSubmenu(std::unique_ptr<PickerItemWithSubmenuView>(
        views::AsViewClass<PickerItemWithSubmenuView>(item.release())));
  }
  NOTREACHED();
}

PickerItemView* PickerSectionView::AddResult(
    const PickerSearchResult& result,
    PickerPreviewBubbleController* preview_controller,
    SelectResultCallback select_result_callback) {
  return AddItem(CreateItemFromResult(result, preview_controller,
                                      asset_fetcher_, section_width_,
                                      std::move(select_result_callback)));
}

void PickerSectionView::ClearItems() {
  item_views_.clear();
  if (image_item_grid_ != nullptr) {
    RemoveChildViewT(image_item_grid_.ExtractAsDangling());
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

PickerTraversableItemContainer* PickerSectionView::GetItemContainer() {
  if (image_item_grid_ != nullptr) {
    return image_item_grid_;
  }
  return list_item_container_;
}

BEGIN_METADATA(PickerSectionView)
END_METADATA

}  // namespace ash
