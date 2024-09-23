// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_PAGE_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_PAGE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

// View for a page that can act as the main contents of the Picker.
class ASH_EXPORT PickerPageView : public views::View,
                                  public PickerTraversableItemContainer {
  METADATA_HEADER(PickerPageView, views::View)
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_PAGE_VIEW_H_
