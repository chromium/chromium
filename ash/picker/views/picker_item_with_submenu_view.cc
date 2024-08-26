// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_item_with_submenu_view.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/bubble/bubble_utils.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_submenu_controller.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "components/vector_icons/vector_icons.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

// Size of both the leading and trailing icon.
constexpr gfx::Size kIconSizeDip(20, 20);
constexpr auto kLeadingIconRightPadding = gfx::Insets::TLBR(0, 0, 0, 16);
constexpr auto kBorderInsets = gfx::Insets::TLBR(8, 16, 8, 16);

}  // namespace

PickerItemWithSubmenuView::PickerItemWithSubmenuView()
    : PickerItemView(base::DoNothing(), FocusIndicatorStyle::kFocusBar) {
  SetCallback(base::BindRepeating(&PickerItemWithSubmenuView::ShowSubmenu,
                                  weak_ptr_factory_.GetWeakPtr()));

  // This view only contains one child for the moment, but treat this as a
  // full-width vertical list.
  SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  // TODO: b/347616202 - Align the leading icon to the top of the item.
  views::Builder<PickerItemWithSubmenuView>(this)
      .SetBorder(views::CreateEmptyBorder(kBorderInsets))
      .AddChild(
          // This is used to group child views that should not receive events.
          views::Builder<views::BoxLayoutView>()
              .SetCanProcessEventsWithinSubtree(false)
              .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
              .AddChildren(
                  // The leading icon should always be preferred size.
                  views::Builder<views::ImageView>()
                      .CopyAddressTo(&leading_icon_view_)
                      .SetImageSize(kIconSizeDip)
                      .SetProperty(views::kMarginsKey,
                                   kLeadingIconRightPadding),
                  // The main container should use the remaining horizontal
                  // space. Shrink to zero to allow the main contents to be
                  // elided.
                  views::Builder<views::Label>(
                      bubble_utils::CreateLabel(TypographyToken::kCrosBody2,
                                                u"",
                                                cros_tokens::kCrosSysOnSurface))
                      .CopyAddressTo(&label_)
                      .SetHorizontalAlignment(
                          gfx::HorizontalAlignment::ALIGN_LEFT)
                      .SetProperty(
                          views::kBoxLayoutFlexKey,
                          views::BoxLayoutFlexSpecification().WithWeight(1)),
                  // The trailing icon should always be preferred size.
                  views::Builder<views::ImageView>()
                      .SetImageSize(kIconSizeDip)
                      .SetImage(ui::ImageModel::FromVectorIcon(
                          vector_icons::kSubmenuArrowChromeRefreshIcon,
                          cros_tokens::kCrosSysOnSurface))))
      .BuildChildren();

  GetViewAccessibility().SetRole(ax::mojom::Role::kPopUpButton);
  GetViewAccessibility().SetHasPopup(ax::mojom::HasPopup::kMenu);
}

PickerItemWithSubmenuView::~PickerItemWithSubmenuView() = default;

void PickerItemWithSubmenuView::SetLeadingIcon(const ui::ImageModel& icon) {
  leading_icon_view_->SetImage(icon);
}

void PickerItemWithSubmenuView::SetText(const std::u16string& primary_text) {
  label_->SetText(primary_text);
  SetAccessibleName(primary_text);
}

void PickerItemWithSubmenuView::AddEntry(PickerSearchResult result,
                                         SelectItemCallback callback) {
  entries_.emplace_back(std::move(result), std::move(callback));
}

bool PickerItemWithSubmenuView::IsEmpty() const {
  return entries_.empty();
}

void PickerItemWithSubmenuView::ShowSubmenu() {
  if (GetSubmenuController() == nullptr) {
    return;
  }

  std::vector<std::unique_ptr<PickerListItemView>> items;
  items.reserve(entries_.size());
  for (const auto& [result, callback] : entries_) {
    // There are no image item results in submenus, so can pass 0 for
    // `available_width`.
    auto item = PickerSectionView::CreateItemFromResult(
        result, /*preview_controller=*/nullptr, /*asset_fetcher=*/nullptr,
        /*available_width=*/0, PickerSectionView::LocalFileResultStyle::kList,
        callback);
    auto list_item = base::WrapUnique(
        views::AsViewClass<PickerListItemView>(item.release()));
    CHECK(list_item);
    items.push_back(std::move(list_item));
  }
  GetSubmenuController()->Show(this, std::move(items));
}

void PickerItemWithSubmenuView::OnMouseEntered(const ui::MouseEvent& event) {
  ShowSubmenu();
}

const std::u16string& PickerItemWithSubmenuView::GetTextForTesting() const {
  return label_->GetText();
}

BEGIN_METADATA(PickerItemWithSubmenuView)
END_METADATA

}  // namespace ash
