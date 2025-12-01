// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_row_button.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kActorTaskListBubbleView);

namespace {
const int kVerticalMargin = 8;
}  // namespace

// static
views::Widget* ActorTaskListBubble::ShowBubble(
    views::View* anchor_view,
    std::vector<ActorTaskListBubbleRowButtonParams> param_list) {
  auto contents_view = CreateContentsView(std::move(param_list));

  auto dialog_model =
      ui::DialogModel::Builder()
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  std::move(contents_view),
                  views::BubbleDialogModelHost::FieldType::kMenuItem),
              kActorTaskListBubbleView)
          .OverrideShowCloseButton(false)
          .Build();

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_view, views::BubbleBorder::TOP_RIGHT);
  bubble->set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  bubble->set_margins(gfx::Insets::VH(kVerticalMargin, 0));

  auto* widget = views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
  // Bubble can always show activated as it will only show in the active window.
  widget->Show();

  return widget;
}

std::unique_ptr<views::View> ActorTaskListBubble::CreateContentsView(
    std::vector<ActorTaskListBubbleRowButtonParams> param_list) {
  std::unique_ptr<views::View> contents_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build();

  for (auto& params : param_list) {
    contents_view->AddChildView(
        std::make_unique<ActorTaskListBubbleRowButton>(std::move(params)));
  }
  return contents_view;
}
