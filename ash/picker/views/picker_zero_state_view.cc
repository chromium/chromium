// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_zero_state_view.h"

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/picker/picker_asset_fetcher.h"
#include "ash/picker/picker_clipboard_provider.h"
#include "ash/picker/views/picker_category_type.h"
#include "ash/picker/views/picker_icons.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/picker/views/picker_item_with_submenu_view.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_section_list_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "ash/picker/views/picker_zero_state_view_delegate.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/components/editor_menu/public/cpp/preset_text_query.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/image.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

enum class EditorSubmenu { kNone, kLength, kTone };

EditorSubmenu GetEditorSubmenu(
    std::optional<chromeos::editor_menu::PresetQueryCategory> category) {
  if (!category.has_value()) {
    return EditorSubmenu::kNone;
  }

  switch (*category) {
    case chromeos::editor_menu::PresetQueryCategory::kUnknown:
      return EditorSubmenu::kNone;
    case chromeos::editor_menu::PresetQueryCategory::kShorten:
      return EditorSubmenu::kLength;
    case chromeos::editor_menu::PresetQueryCategory::kElaborate:
      return EditorSubmenu::kLength;
    case chromeos::editor_menu::PresetQueryCategory::kRephrase:
      return EditorSubmenu::kNone;
    case chromeos::editor_menu::PresetQueryCategory::kFormalize:
      return EditorSubmenu::kTone;
    case chromeos::editor_menu::PresetQueryCategory::kEmojify:
      return EditorSubmenu::kTone;
    case chromeos::editor_menu::PresetQueryCategory::kProofread:
      return EditorSubmenu::kNone;
  }
}

}  // namespace

PickerZeroStateView::PickerZeroStateView(
    PickerZeroStateViewDelegate* delegate,
    base::span<const PickerCategory> available_categories,
    int picker_view_width,
    PickerAssetFetcher* asset_fetcher,
    PickerSubmenuController* submenu_controller)
    : delegate_(delegate), submenu_controller_(submenu_controller) {
  SetLayoutManager(std::make_unique<views::BoxLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  section_list_view_ = AddChildView(std::make_unique<PickerSectionListView>(
      picker_view_width, asset_fetcher, submenu_controller_));

  delegate_->GetZeroStateSuggestedResults(
      base::BindRepeating(&PickerZeroStateView::OnFetchSuggestedResults,
                          weak_ptr_factory_.GetWeakPtr()));

  for (PickerCategory category : available_categories) {
    // kEditorRewrite is not visible in the zero-state, since it's replaced with
    // the rewrite suggestions.
    if (category == PickerCategory::kEditorRewrite) {
      continue;
    }

    auto result = PickerSearchResult::Category(category);
    GetOrCreateSectionView(category)->AddResult(
        std::move(result), &preview_controller_,
        base::BindRepeating(&PickerZeroStateView::OnCategorySelected,
                            weak_ptr_factory_.GetWeakPtr(), category));
  }
}

PickerZeroStateView::~PickerZeroStateView() = default;

views::View* PickerZeroStateView::GetTopItem() {
  return section_list_view_->GetTopItem();
}

views::View* PickerZeroStateView::GetBottomItem() {
  return section_list_view_->GetBottomItem();
}

views::View* PickerZeroStateView::GetItemAbove(views::View* item) {
  if (!Contains(item)) {
    return nullptr;
  }
  if (views::IsViewClass<PickerItemView>(item)) {
    // Skip views that aren't PickerItemViews, to allow users to quickly
    // navigate between items.
    return section_list_view_->GetItemAbove(item);
  }
  views::View* prev_item = GetNextPickerPseudoFocusableView(
      item, PickerPseudoFocusDirection::kBackward, /*should_loop=*/false);
  return Contains(prev_item) ? prev_item : nullptr;
}

views::View* PickerZeroStateView::GetItemBelow(views::View* item) {
  if (!Contains(item)) {
    return nullptr;
  }
  if (views::IsViewClass<PickerItemView>(item)) {
    // Skip views that aren't PickerItemViews, to allow users to quickly
    // navigate between items.
    return section_list_view_->GetItemBelow(item);
  }
  views::View* next_item = GetNextPickerPseudoFocusableView(
      item, PickerPseudoFocusDirection::kForward, /*should_loop=*/false);
  return Contains(next_item) ? next_item : nullptr;
}

views::View* PickerZeroStateView::GetItemLeftOf(views::View* item) {
  if (!Contains(item) || !views::IsViewClass<PickerItemView>(item)) {
    return nullptr;
  }
  return section_list_view_->GetItemLeftOf(item);
}

views::View* PickerZeroStateView::GetItemRightOf(views::View* item) {
  if (!Contains(item) || !views::IsViewClass<PickerItemView>(item)) {
    return nullptr;
  }
  return section_list_view_->GetItemRightOf(item);
}

bool PickerZeroStateView::ContainsItem(views::View* item) {
  return Contains(item);
}

PickerSectionView* PickerZeroStateView::GetOrCreateSectionView(
    PickerCategoryType category_type) {
  auto section_view_iterator = category_section_views_.find(category_type);
  if (section_view_iterator != category_section_views_.end()) {
    return section_view_iterator->second;
  }

  auto* section_view = section_list_view_->AddSection();
  section_view->AddTitleLabel(
      GetSectionTitleForPickerCategoryType(category_type));
  category_section_views_.insert({category_type, section_view});
  return section_view;
}

PickerSectionView* PickerZeroStateView::GetOrCreateSectionView(
    PickerCategory category) {
  return GetOrCreateSectionView(GetPickerCategoryType(category));
}

void PickerZeroStateView::OnCategorySelected(PickerCategory category) {
  delegate_->SelectZeroStateCategory(category);
}

void PickerZeroStateView::OnResultSelected(const PickerSearchResult& result) {
  delegate_->SelectZeroStateResult(result);
}

void PickerZeroStateView::OnFetchSuggestedResults(
    std::vector<PickerSearchResult> results) {
  if (results.empty()) {
    return;
  }
  // TODO: b/343092747 - Remove this to the top once the `primary_section_view_`
  // always has at least one child.
  if (primary_section_view_ == nullptr) {
    primary_section_view_ = section_list_view_->AddSectionAt(0);
  }

  auto new_window_submenu =
      views::Builder<PickerItemWithSubmenuView>()
          .SetSubmenuController(submenu_controller_)
          .SetText(l10n_util::GetStringUTF16(IDS_PICKER_NEW_MENU_LABEL))
          .SetLeadingIcon(ui::ImageModel::FromVectorIcon(
              kSystemMenuPlusIcon, cros_tokens::kCrosSysOnSurface))
          .Build();

  // Some Editor results are shown directly in the section, some shown behind
  // submenus. Iterate through the results and put them into the right View.
  auto length_submenu =
      views::Builder<PickerItemWithSubmenuView>()
          .SetSubmenuController(submenu_controller_)
          .SetText(
              l10n_util::GetStringUTF16(IDS_PICKER_CHANGE_LENGTH_MENU_LABEL))
          .SetLeadingIcon(ui::ImageModel::FromVectorIcon(
              chromeos::kEditorMenuShortenIcon, cros_tokens::kCrosSysOnSurface))
          .Build();

  auto tone_submenu =
      views::Builder<PickerItemWithSubmenuView>()
          .SetSubmenuController(submenu_controller_)
          .SetText(l10n_util::GetStringUTF16(IDS_PICKER_CHANGE_TONE_MENU_LABEL))
          .SetLeadingIcon(ui::ImageModel::FromVectorIcon(
              chromeos::kEditorMenuEmojifyIcon, cros_tokens::kCrosSysOnSurface))
          .Build();

  // Case transformation results are shown in a submenu.
  auto case_transform_submenu =
      views::Builder<PickerItemWithSubmenuView>()
          .SetSubmenuController(submenu_controller_)
          .SetText(l10n_util::GetStringUTF16(
              IDS_PICKER_CHANGE_CAPITALIZATION_MENU_LABEL))
          .SetLeadingIcon(ui::ImageModel::FromVectorIcon(
              kPickerSentenceCaseIcon, cros_tokens::kCrosSysOnSurface))
          .Build();

  for (const PickerSearchResult& result : results) {
    if (std::holds_alternative<PickerSearchResult::NewWindowData>(
            result.data())) {
      new_window_submenu->AddEntry(
          result, base::BindRepeating(&PickerZeroStateView::OnResultSelected,
                                      weak_ptr_factory_.GetWeakPtr(), result));
    } else if (const auto* editor_data =
                   std::get_if<PickerSearchResult::EditorData>(
                       &result.data())) {
      auto callback =
          base::BindRepeating(&PickerZeroStateView::OnResultSelected,
                              weak_ptr_factory_.GetWeakPtr(), result);
      switch (GetEditorSubmenu(editor_data->category)) {
        case EditorSubmenu::kNone:
          primary_section_view_->AddResult(result, &preview_controller_,
                                           std::move(callback));
          break;
        case EditorSubmenu::kLength:
          length_submenu->AddEntry(result, std::move(callback));
          break;
        case EditorSubmenu::kTone:
          tone_submenu->AddEntry(result, std::move(callback));
          break;
      }
    } else if (std::holds_alternative<PickerSearchResult::CaseTransformData>(
                   result.data())) {
      case_transform_submenu->AddEntry(
          result, base::BindRepeating(&PickerZeroStateView::OnResultSelected,
                                      weak_ptr_factory_.GetWeakPtr(), result));
    } else {
      PickerItemView* view = primary_section_view_->AddResult(
          result, &preview_controller_,
          base::BindRepeating(&PickerZeroStateView::OnResultSelected,
                              weak_ptr_factory_.GetWeakPtr(), result));

      if (auto* list_item_view = views::AsViewClass<PickerListItemView>(view)) {
        list_item_view->SetBadgeAction(delegate_->GetActionForResult(result));
      }
    }
  }

  if (!new_window_submenu->IsEmpty()) {
    primary_section_view_->AddItemWithSubmenu(std::move(new_window_submenu));
  }

  if (!length_submenu->IsEmpty()) {
    primary_section_view_->AddItemWithSubmenu(std::move(length_submenu));
  }

  if (!tone_submenu->IsEmpty()) {
    primary_section_view_->AddItemWithSubmenu(std::move(tone_submenu));
  }

  // Add the submenu for case transformation.
  if (!case_transform_submenu->IsEmpty()) {
    GetOrCreateSectionView(PickerCategoryType::kCaseTransformations)
        ->AddItemWithSubmenu(std::move(case_transform_submenu));
  }

  delegate_->RequestPseudoFocus(section_list_view_->GetTopItem());
}

BEGIN_METADATA(PickerZeroStateView)
END_METADATA

}  // namespace ash
