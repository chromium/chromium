// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_row_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace {
const int kIconSize = 20;
}  // namespace

ActorTaskListBubbleRowButton::ActorTaskListBubbleRowButton(
    ActorTaskListBubbleRowButtonParams params)
    : RichHoverButton(std::move(params.on_click_callback),
                      /*icon=*/
                      ui::ImageModel::FromVectorIcon(kScreensaverAutoIcon,
                                                     ui::kColorMenuIcon,
                                                     kIconSize),
                      /*title_text=*/params.title,
                      /*subtitle_text=*/params.subtitle) {
  if (subtitle()) {
    // TODO(crbug.com/460121008): Revisit when investigating a custom layout for
    // the row button. Hovering over the subtitle should also hover the row.
    subtitle()->SetCanProcessEventsWithinSubtree(false);
  }
}

ActorTaskListBubbleRowButton::~ActorTaskListBubbleRowButton() = default;

void ActorTaskListBubbleRowButton::OnMouseEntered(const ui::MouseEvent& event) {
  View::OnMouseEntered(event);
  SetState(Button::STATE_HOVERED);
  SetActionIcon(ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon,
                                               ui::kColorMenuIcon, kIconSize));
}

void ActorTaskListBubbleRowButton::OnMouseExited(const ui::MouseEvent& event) {
  View::OnMouseExited(event);
  SetState(Button::STATE_NORMAL);
  SetActionIcon(ui::ImageModel());
}

BEGIN_METADATA(ActorTaskListBubbleRowButton)
END_METADATA
