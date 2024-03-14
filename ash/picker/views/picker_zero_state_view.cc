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

#include "ash/picker/model/picker_model.h"
#include "ash/picker/picker_clipboard_provider.h"
#include "ash/picker/views/picker_caps_nudge_view.h"
#include "ash/picker/views/picker_category_type.h"
#include "ash/picker/views/picker_icons.h"
#include "ash/picker/views/picker_list_item_view.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_section_list_view.h"
#include "ash/picker/views/picker_section_view.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {
constexpr base::TimeDelta kNudgeHideAnimationDuration = base::Milliseconds(50);
}  // namespace

PickerZeroStateView::PickerZeroStateView(
    int picker_view_width,
    SelectCategoryCallback select_category_callback,
    SelectSearchResultCallback select_result_callback) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  caps_nudge_view_ =
      AddChildView(std::make_unique<PickerCapsNudgeView>(base::BindRepeating(
          &PickerZeroStateView::ClearCapsNudge, base::Unretained(this))));

  section_list_view_ =
      AddChildView(std::make_unique<PickerSectionListView>(picker_view_width));

  clipboard_provider_ = std::make_unique<PickerClipboardProvider>(
      std::move(select_result_callback));
  clipboard_provider_->FetchResult(
      base::BindRepeating(&PickerZeroStateView::OnFetchSuggestedResult,
                          weak_ptr_factory_.GetWeakPtr()));

  for (auto category : PickerModel().GetAvailableCategories()) {
    auto item_view = std::make_unique<PickerListItemView>(
        base::BindRepeating(select_category_callback, category));
    item_view->SetPrimaryText(GetLabelForPickerCategory(category));
    item_view->SetLeadingIcon(GetIconForPickerCategory(category));
    GetOrCreateSectionView(category)->AddListItem(std::move(item_view));
  }
  SetPseudoFocusedView(section_list_view_->GetTopItem());
}

PickerZeroStateView::~PickerZeroStateView() = default;

bool PickerZeroStateView::DoPseudoFocusedAction() {
  if (pseudo_focused_view_ == nullptr) {
    return false;
  }

  return DoPickerPseudoFocusedActionOnView(pseudo_focused_view_);
}

bool PickerZeroStateView::MovePseudoFocusUp() {
  if (pseudo_focused_view_ == nullptr) {
    return false;
  }

  if (views::IsViewClass<PickerItemView>(pseudo_focused_view_)) {
    // Try to move directly to an item above the currently pseudo focused item,
    // i.e. skip non-item views.
    if (PickerItemView* item = section_list_view_->GetItemAbove(
            views::AsViewClass<PickerItemView>(pseudo_focused_view_))) {
      SetPseudoFocusedView(item);
      return true;
    }
  }

  // Default to backward pseudo focus traversal.
  AdvancePseudoFocus(PseudoFocusDirection::kBackward);
  return true;
}

bool PickerZeroStateView::MovePseudoFocusDown() {
  if (pseudo_focused_view_ == nullptr) {
    return false;
  }

  if (views::IsViewClass<PickerItemView>(pseudo_focused_view_)) {
    // Try to move directly to an item below the currently pseudo focused item,
    // i.e. skip non-item views.
    if (PickerItemView* item = section_list_view_->GetItemBelow(
            views::AsViewClass<PickerItemView>(pseudo_focused_view_))) {
      SetPseudoFocusedView(item);
      return true;
    }
  }

  // Default to forward pseudo focus traversal.
  AdvancePseudoFocus(PseudoFocusDirection::kForward);
  return true;
}

bool PickerZeroStateView::MovePseudoFocusLeft() {
  if (pseudo_focused_view_ == nullptr ||
      !views::IsViewClass<PickerItemView>(pseudo_focused_view_)) {
    return false;
  }

  // Only allow left pseudo focus movement if there is an item directly to the
  // left of the current pseudo focused item. In other situations, we prefer not
  // to handle the movement here so that it can instead be used for other
  // purposes, e.g. moving the caret in the search field.
  if (PickerItemView* item = section_list_view_->GetItemLeftOf(
          views::AsViewClass<PickerItemView>(pseudo_focused_view_))) {
    SetPseudoFocusedView(item);
    return true;
  }
  return false;
}

bool PickerZeroStateView::MovePseudoFocusRight() {
  if (pseudo_focused_view_ == nullptr ||
      !views::IsViewClass<PickerItemView>(pseudo_focused_view_)) {
    return false;
  }

  // Only allow right pseudo focus movement if there is an item directly to the
  // right of the current pseudo focused item. In other situations, we prefer
  // not to handle the movement here so that it can instead be used for other
  // purposes, e.g. moving the caret in the search field.
  if (PickerItemView* item = section_list_view_->GetItemRightOf(
          views::AsViewClass<PickerItemView>(pseudo_focused_view_))) {
    SetPseudoFocusedView(item);
    return true;
  }
  return false;
}

void PickerZeroStateView::AdvancePseudoFocus(PseudoFocusDirection direction) {
  if (pseudo_focused_view_ == nullptr) {
    return;
  }

  views::View* view = GetFocusManager()->GetNextFocusableView(
      pseudo_focused_view_, GetWidget(),
      direction == PseudoFocusDirection::kBackward,
      /*dont_loop=*/false);
  // If the next view is outside this PickerZeroStateView, then loop back to
  // the first (or last) view.
  if (!Contains(view)) {
    view = GetFocusManager()->GetNextFocusableView(
        this, GetWidget(), direction == PseudoFocusDirection::kBackward,
        /*dont_loop=*/false);
  }

  // There can be a short period of time where child views have been added but
  // not drawn yet, so are not considered focusable. The computed `view` may not
  // be valid in these cases. If so, just leave the current pseudo focused view.
  if (view == nullptr || !Contains(view)) {
    return;
  }

  SetPseudoFocusedView(view);
}

PickerSectionView* PickerZeroStateView::GetOrCreateSectionView(
    PickerCategory category) {
  const PickerCategoryType category_type = GetPickerCategoryType(category);
  auto section_view_iterator = section_views_.find(category_type);
  if (section_view_iterator != section_views_.end()) {
    return section_view_iterator->second;
  }

  auto* section_view = section_list_view_->AddSection();
  section_view->AddTitleLabel(
      GetSectionTitleForPickerCategoryType(category_type));
  section_views_.insert({category_type, section_view});
  return section_view;
}

void PickerZeroStateView::ClearCapsNudge() {
  // Animation builder needs layers to animate so add layers to the two views we
  // are animating.
  SetPaintToLayer();
  caps_nudge_view_->SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  caps_nudge_view_->layer()->SetFillsBoundsOpaquely(false);
  views::AnimationBuilder()
      .SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                 IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&PickerZeroStateView::DeleteNudge,
                              weak_ptr_factory_.GetWeakPtr()))
      .Once()
      // Technically, the specs have easing functions for these - but its only 3
      // frames so just use the defaults since the difference won't matter.
      .SetDuration(kNudgeHideAnimationDuration)
      // To hide the caps nudge, we just animate the entire view upwards whilst
      // fading the opacity.
      .SetTransform(
          this,
          gfx::Transform::MakeTranslation(
              0,
              -(caps_nudge_view_->bounds().height() +
                caps_nudge_view_->GetProperty(views::kMarginsKey)->height())))
      .SetOpacity(caps_nudge_view_, /*opacity=*/0);
}

void PickerZeroStateView::DeleteNudge() {
  // Now we are not animating, get rid of the layer.
  DestroyLayer();
  // If the nudge contains the currently pseudo focused view, move pseudo focus
  // to an item before deleting the nudge.
  if (caps_nudge_view_->Contains(pseudo_focused_view_)) {
    SetPseudoFocusedView(section_list_view_->GetTopItem());
  }
  RemoveChildViewT(caps_nudge_view_.ExtractAsDangling());
}

void PickerZeroStateView::SetPseudoFocusedView(views::View* view) {
  if (pseudo_focused_view_ == view) {
    return;
  }

  RemovePickerPseudoFocusFromView(pseudo_focused_view_);
  pseudo_focused_view_ = view;
  ApplyPickerPseudoFocusToView(pseudo_focused_view_);
  ScrollPseudoFocusedViewToVisible();
}

void PickerZeroStateView::ScrollPseudoFocusedViewToVisible() {
  if (pseudo_focused_view_ == nullptr) {
    return;
  }

  if (!views::IsViewClass<PickerItemView>(pseudo_focused_view_)) {
    pseudo_focused_view_->ScrollViewToVisible();
    return;
  }

  auto* pseudo_focused_item =
      views::AsViewClass<PickerItemView>(pseudo_focused_view_);
  if (section_list_view_->GetItemAbove(pseudo_focused_item) == nullptr) {
    // For items at the top, scroll all the way up to let users see that they
    // have reached the top of the zero state view.
    ScrollRectToVisible(gfx::Rect(GetLocalBounds().origin(), gfx::Size()));
  } else if (section_list_view_->GetItemBelow(pseudo_focused_item) == nullptr) {
    // For items at the bottom, scroll all the way down to let users see that
    // they have reached the bottom of the zero state view.
    ScrollRectToVisible(gfx::Rect(GetLocalBounds().bottom_left(), gfx::Size()));
  } else {
    // Otherwise, just ensure the item is visible.
    pseudo_focused_item->ScrollViewToVisible();
  }
}

void PickerZeroStateView::OnFetchSuggestedResult(
    std::unique_ptr<PickerListItemView> item_view) {
  if (!suggested_section_view_) {
    suggested_section_view_ = section_list_view_->AddSectionAt(0);
    suggested_section_view_->AddTitleLabel(
        l10n_util::GetStringUTF16(IDS_PICKER_SUGGESTED_SECTION_TITLE));
  }
  suggested_section_view_->AddListItem(std::move(item_view));
  SetPseudoFocusedView(section_list_view_->GetTopItem());
}

BEGIN_METADATA(PickerZeroStateView)
END_METADATA

}  // namespace ash
