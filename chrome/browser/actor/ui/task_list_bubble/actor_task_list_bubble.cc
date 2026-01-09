// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_row_button.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view_class_properties.h"

DEFINE_ELEMENT_IDENTIFIER_VALUE(kActorTaskListBubbleView);

namespace {
const int kVerticalMargin = 8;
}  // namespace

// static
views::Widget* ActorTaskListBubble::ShowBubble(
    Profile* profile,
    views::View* anchor_view,
    const absl::flat_hash_map<actor::TaskId, bool>& task_list,
    base::RepeatingCallback<void(actor::TaskId)> on_row_clicked) {
  auto contents_view =
      CreateContentsView(profile, task_list, std::move(on_row_clicked));

  // If there are no rows, don't show the bubble.
  if (contents_view->children().empty()) {
    return nullptr;
  }

  auto dialog_model =
      ui::DialogModel::Builder()
          .SetAccessibleTitle(
              l10n_util::GetStringUTF16(IDS_ACTOR_TASK_LIST_BUBBLE_A11Y_LABEL))
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
    Profile* profile,
    const absl::flat_hash_map<actor::TaskId, bool>& task_list,
    base::RepeatingCallback<void(actor::TaskId)> on_row_clicked) {
  std::unique_ptr<views::View> contents_view =
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .Build();

  actor::ui::ActorUiStateManagerInterface* actor_ui_state_manager =
      actor::ActorKeyedService::Get(profile)->GetActorUiStateManager();
  for (auto [task_id, requires_processing] : task_list) {
    if (!actor_ui_state_manager->GetActorTaskState(task_id)) {
      actor::ui::RecordTaskIconError(
          actor::ui::ActorUiTaskIconError::kBubbleTaskDoesntExist);
      continue;
    }

    auto task_state = actor_ui_state_manager->GetActorTaskState(task_id);
    auto task_title = actor_ui_state_manager->GetActorTaskTitle(task_id);
    auto task_tab = actor_ui_state_manager->GetLastActedOnTab(task_id);
    CHECK(task_state.has_value());
    CHECK(task_title.has_value());
    CHECK(task_tab.has_value());

    contents_view->AddChildView(std::make_unique<ActorTaskListBubbleRowButton>(
        base::BindRepeating(on_row_clicked, task_id), task_state.value(),
        base::UTF8ToUTF16(task_title.value()), requires_processing,
        task_tab.value() != nullptr));
  }
  return contents_view;
}
