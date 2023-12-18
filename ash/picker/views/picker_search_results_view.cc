// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_results_view.h"

#include <memory>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/view_class_properties.h"

namespace ash {

PickerSearchResultsView::PickerSearchResultsView(
    SelectSearchResultCallback callback)
    : select_search_result_callback_(std::move(callback)) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

PickerSearchResultsView::~PickerSearchResultsView() = default;

BEGIN_METADATA(PickerSearchResultsView, views::View)
END_METADATA

}  // namespace ash
