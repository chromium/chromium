// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_test_api.h"

#include "ash/shell.h"
#include "ash/style/system_dialog_delegate_view.h"
#include "ash/wm/window_restore/pine_controller.h"
#include "ui/views/view_utils.h"

namespace ash {

PineContentsViewTestApi::PineContentsViewTestApi(
    const PineContentsView* pine_contents_view)
    : pine_contents_view_(pine_contents_view) {}

PineContentsViewTestApi::~PineContentsViewTestApi() = default;

PineItemViewTestApi::PineItemViewTestApi(const PineItemView* pine_item_view)
    : pine_item_view_(pine_item_view) {}

PineItemViewTestApi::~PineItemViewTestApi() = default;

PineItemsOverflowViewTestApi::PineItemsOverflowViewTestApi(
    const PineItemsOverflowView* overflow_view)
    : overflow_view_(overflow_view) {}

PineItemsOverflowViewTestApi::~PineItemsOverflowViewTestApi() = default;

PineTestApi::PineTestApi() = default;

PineTestApi::~PineTestApi() = default;

void PineTestApi::SetPineContentsDataForTesting(
    std::unique_ptr<PineContentsData> pine_contents_data) {
  Shell::Get()->pine_controller()->pine_contents_data_ =
      std::move(pine_contents_data);
}

SystemDialogDelegateView* PineTestApi::GetOnboardingDialog() {
  auto* onboarding_widget =
      Shell::Get()->pine_controller()->onboarding_widget_.get();
  return onboarding_widget ? views::AsViewClass<SystemDialogDelegateView>(
                                 onboarding_widget->GetContentsView())
                           : nullptr;
}

PineScreenshotIconRowViewTestApi::PineScreenshotIconRowViewTestApi(
    const PineScreenshotIconRowView* icon_row_view)
    : icon_row_view_(icon_row_view) {}

PineScreenshotIconRowViewTestApi::~PineScreenshotIconRowViewTestApi() = default;

}  // namespace ash
