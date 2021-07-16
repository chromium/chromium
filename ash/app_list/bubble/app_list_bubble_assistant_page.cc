// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/app_list_bubble_assistant_page.h"

#include <memory>

#include "ash/app_list/views/assistant/assistant_dialog_plate.h"
#include "ash/app_list/views/assistant/assistant_main_stage.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

using views::BoxLayout;

namespace ash {

AppListBubbleAssistantPage::AppListBubbleAssistantPage(
    AssistantViewDelegate* delegate) {
  auto* layout = SetLayoutManager(
      std::make_unique<BoxLayout>(BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // TODO(crbug.com/1216098): Dark background support. The assistant answer
  // cards currently assume they are placed within a white container.
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));

  dialog_plate_ =
      AddChildView(std::make_unique<AssistantDialogPlate>(delegate));
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
