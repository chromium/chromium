// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_image_item_view.h"

#include <memory>
#include <utility>

#include "ash/ash_element_identifiers.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr int kPickerImageItemCornerRadius = 8;

}  // namespace

PickerImageItemView::PickerImageItemView(
    std::unique_ptr<views::ImageView> image,
    std::u16string accessible_name,
    SelectItemCallback select_item_callback)
    : PickerItemView(std::move(select_item_callback),
                     FocusIndicatorStyle::kFocusRingWithInsetGap),
      accessible_name_(std::move(accessible_name)) {
  SetUseDefaultFillLayout(true);
  SetCornerRadius(kPickerImageItemCornerRadius);
  GetViewAccessibility().SetName(accessible_name_);
  SetProperty(views::kElementIdentifierKey,
              kPickerSearchResultsImageItemElementId);

  image_view_ = AddChildView(std::move(image));
  image_view_->SetCanProcessEventsWithinSubtree(false);
}

PickerImageItemView::~PickerImageItemView() = default;

void PickerImageItemView::SetAction(PickerActionType action) {
  switch (action) {
    case PickerActionType::kDo:
      GetViewAccessibility().SetName(accessible_name_);
      break;
    case PickerActionType::kInsert:
      GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
          IDS_PICKER_LIST_ITEM_INSERT_ACTION_ACCESSIBLE_NAME,
          accessible_name_));
      break;
    case PickerActionType::kOpen:
      GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
          IDS_PICKER_LIST_ITEM_OPEN_ACTION_ACCESSIBLE_NAME, accessible_name_));
      break;
    case PickerActionType::kCreate:
      // TODO: b/345303965 - Add internal strings for Create.
      GetViewAccessibility().SetName(accessible_name_);
      break;
  }
}

BEGIN_METADATA(PickerImageItemView)
END_METADATA

}  // namespace ash
