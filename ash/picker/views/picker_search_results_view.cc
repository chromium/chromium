// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_results_view.h"

#include <memory>

#include "ash/picker/model/picker_search_results.h"
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

bool PickerSearchResultsView::OnMousePressed(const ui::MouseEvent& event) {
  // TODO(b/316935667): Move this event handler to the individual result item
  // views.
  if (event.IsOnlyLeftMouseButton()) {
    return true;
  }

  return views::View::OnMousePressed(event);
}

void PickerSearchResultsView::OnMouseReleased(const ui::MouseEvent& event) {
  // TODO(b/316935667): Insert the result based on the click. For now, take the
  // very first result.
  std::move(select_search_result_callback_)
      .Run(search_results_.sections()[0].results()[0]);
}

void PickerSearchResultsView::SetSearchResults(
    const PickerSearchResults& search_results) {
  // TODO(b/316935667): Render the results.
  search_results_ = search_results;
}

BEGIN_METADATA(PickerSearchResultsView, views::View)
END_METADATA

}  // namespace ash
