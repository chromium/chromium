// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_row_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"

namespace {
const int kBubbleRowIconSize = 16;
const int kRedirectIconSize = 20;

const gfx::VectorIcon& GetRowIcon(actor::ActorTask::State state) {
  if (tabs::GlicActorTaskIconManager::RequiresAttention(state)) {
    return kHourglassIcon;
  } else if (state == actor::ActorTask::State::kFinished) {
    return kTaskSparkIcon;
  }
  return glic::GlicVectorIconManager::GetVectorIcon(IDR_ACTOR_AUTO_BROWSE_ICON);
}

bool IsProcessedTabClosedRow(bool has_tab, bool requires_processing) {
  return !has_tab && !requires_processing;
}

ui::ColorId GetRowColor(actor::ActorTask::State state,
                        bool has_tab,
                        bool requires_processing) {
  if (IsProcessedTabClosedRow(has_tab, requires_processing)) {
    return ui::kColorSysStateDisabled;
  }
  if (requires_processing &&
      tabs::GlicActorTaskIconManager::RequiresAttention(state)) {
    return ui::kColorSysPrimary;
  }
  return ui::kColorMenuIcon;
}

std::u16string GetRowSubtitle(actor::ActorTask::State state, bool has_tab) {
  if (!has_tab) {
    return l10n_util::GetStringUTF16(
        IDR_ACTOR_TASK_LIST_BUBBLE_ROW_TAB_CLOSED_SUBTITLE);
  }
  if (tabs::GlicActorTaskIconManager::RequiresAttention(state)) {
    return l10n_util::GetStringUTF16(
        IDR_ACTOR_TASK_LIST_BUBBLE_ROW_CHECK_TASK_SUBTITLE);
  }
  if (state == actor::ActorTask::State::kFinished) {
    return l10n_util::GetStringUTF16(
        IDR_ACTOR_TASK_LIST_BUBBLE_ROW_COMPLETED_TASK_SUBTITLE);
  } else if (state == actor::ActorTask::State::kFailed) {
    return l10n_util::GetStringUTF16(
        IDR_ACTOR_TASK_LIST_BUBBLE_ROW_FAILED_TASK_SUBTITLE);
  } else if (state == actor::ActorTask::State::kPausedByUser) {
    return l10n_util::GetStringUTF16(
        IDR_ACTOR_TASK_LIST_BUBBLE_ROW_PAUSED_TASK_SUBTITLE);
  }
  return l10n_util::GetStringUTF16(
      IDR_ACTOR_TASK_LIST_BUBBLE_ROW_ACTING_TASK_SUBTITLE);
}

}  // namespace

ActorTaskListBubbleRowButton::ActorTaskListBubbleRowButton(
    views::Button::PressedCallback on_row_clicked,
    actor::ActorTask::State state,
    std::u16string title,
    bool requires_processing,
    bool has_tab)
    : RichHoverButton(std::move(on_row_clicked),
                      /*icon=*/
                      ui::ImageModel::FromVectorIcon(
                          GetRowIcon(state),
                          GetRowColor(state, has_tab, requires_processing),
                          kBubbleRowIconSize),
                      /*title_text=*/title,
                      /*subtitle_text=*/GetRowSubtitle(state, has_tab)),
      has_tab_(has_tab),
      requires_processing_(requires_processing) {
  SetSubtitleTextStyleAndColor(
      /*default_style*/ views::style::STYLE_BODY_5,
      GetRowColor(state, has_tab, requires_processing));
  if (subtitle()) {
    // TODO(crbug.com/460121008): Revisit when investigating a custom layout for
    // the row button. Hovering over the subtitle should also hover the row.
    subtitle()->SetCanProcessEventsWithinSubtree(false);
  }
  MaybeSetDisabledRowUi();
}

void ActorTaskListBubbleRowButton::StateChanged(ButtonState old_state) {
  // Disable hover for "Tab closed" row after its first appearance.
  if (IsProcessedTabClosedRow(has_tab_, requires_processing_)) {
    views::LabelButton::StateChanged(old_state);
  } else {
    HoverButton::StateChanged(old_state);
  }
}

void ActorTaskListBubbleRowButton::MaybeSetDisabledRowUi() {
  // Update UI for "Tab closed" row after its first appearance.
  if (IsProcessedTabClosedRow(has_tab_, requires_processing_)) {
    SetEnabled(false);
    SetTitleTextStyleAndColor(
        /*default_style*/ views::style::STYLE_BODY_3_MEDIUM,
        ui::kColorSysStateDisabled);
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);
  }
}

ActorTaskListBubbleRowButton::~ActorTaskListBubbleRowButton() = default;

void ActorTaskListBubbleRowButton::OnMouseEntered(const ui::MouseEvent& event) {
  View::OnMouseEntered(event);
  // If the tab is closed, we never want to render the redirect icon.
  if (!has_tab_) {
    return;
  }
  SetState(Button::STATE_HOVERED);
  SetActionIcon(ui::ImageModel::FromVectorIcon(
      vector_icons::kLaunchIcon, ui::kColorMenuIcon, kRedirectIconSize));
}

void ActorTaskListBubbleRowButton::OnMouseExited(const ui::MouseEvent& event) {
  View::OnMouseExited(event);
  // If the tab is closed, we never want to render the redirect icon.
  if (!has_tab_) {
    return;
  }
  SetState(Button::STATE_NORMAL);
  SetActionIcon(ui::ImageModel());
}

BEGIN_METADATA(ActorTaskListBubbleRowButton)
END_METADATA
