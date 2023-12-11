// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_contents_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

PickerContentsView::PickerContentsView() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

PickerContentsView::~PickerContentsView() = default;

void PickerContentsView::SetActivePage(views::View* view) {
  for (views::View* child : children()) {
    child->SetVisible(child == view);
  }
}

BEGIN_METADATA(PickerContentsView, views::View)
END_METADATA

}  // namespace ash
