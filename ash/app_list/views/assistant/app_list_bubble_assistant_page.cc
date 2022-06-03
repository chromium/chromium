// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/app_list_bubble_assistant_page.h"

#include <memory>

#include "ash/app_list/views/assistant/assistant_dialog_plate.h"
#include "ash/app_list/views/assistant/assistant_main_stage.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace ash {

AppListBubbleAssistantPage::AppListBubbleAssistantPage(
    AssistantViewDelegate* delegate) {
  auto* layout = SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  dialog_plate_ =
      AddChildView(std::make_unique<AssistantDialogPlate>(delegate));
  dialog_plate_->SetPaintToLayer();
  dialog_plate_->layer()->SetFillsBoundsOpaquely(false);
  main_stage_ =
      AddChildView(std::make_unique<AppListAssistantMainStage>(delegate));
  layout->SetFlexForView(main_stage_, 1);
}

AppListBubbleAssistantPage::~AppListBubbleAssistantPage() = default;

void AppListBubbleAssistantPage::RequestFocus() {
  dialog_plate_->RequestFocus();
}

BEGIN_METADATA(AppListBubbleAssistantPage, views::View)
END_METADATA

}  // namespace ash
