// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_row_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#endif

namespace {
const int kBubbleRowIconSize = 16;
const int kRedirectIconSize = 20;

const gfx::VectorIcon& GetRowIcon(actor::ActorTask::State state) {
#if BUILDFLAG(ENABLE_GLIC)
  if (base::FeatureList::IsEnabled(features::kGlicActorUiTaskIconV2)) {
    if (state == actor::ActorTask::State::kPausedByActor ||
        state == actor::ActorTask::State::kWaitingOnUser) {
      return kHourglassIcon;
    }
    return glic::GlicVectorIconManager::GetVectorIcon(
        IDR_ACTOR_AUTO_BROWSE_ICON);
  }
#endif
  return kScreensaverAutoIcon;
}

ui::ColorId GetRowColor(actor::ActorTask::State state,
                        bool requires_processing) {
  if (requires_processing &&
      (state == actor::ActorTask::State::kPausedByActor ||
       state == actor::ActorTask::State::kWaitingOnUser)) {
    return ui::kColorSysPrimary;
  }
  return ui::kColorMenuIcon;
}

// TODO(crbug.com/470101572): Return correct subtitles for active tasks.
std::u16string GetRowSubtitle() {
  return l10n_util::GetStringUTF16(
      IDR_ACTOR_TASK_LIST_BUBBLE_ROW_CHECK_TASK_SUBTITLE);
}

}  // namespace

ActorTaskListBubbleRowButton::ActorTaskListBubbleRowButton(
    views::Button::PressedCallback on_row_clicked,
    actor::ActorTask::State state,
    std::u16string title,
    bool requires_processing)
    : RichHoverButton(std::move(on_row_clicked),
                      /*icon=*/
                      ui::ImageModel::FromVectorIcon(
                          GetRowIcon(state),
                          GetRowColor(state, requires_processing),
                          kBubbleRowIconSize),
                      /*title_text=*/title,
                      /*subtitle_text=*/GetRowSubtitle()) {
  SetSubtitleTextStyleAndColor(/*default_style*/ views::style::STYLE_BODY_5,
                               GetRowColor(state, requires_processing));
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
  SetActionIcon(ui::ImageModel::FromVectorIcon(
      vector_icons::kLaunchIcon, ui::kColorMenuIcon, kRedirectIconSize));
}

void ActorTaskListBubbleRowButton::OnMouseExited(const ui::MouseEvent& event) {
  View::OnMouseExited(event);
  SetState(Button::STATE_NORMAL);
  SetActionIcon(ui::ImageModel());
}

BEGIN_METADATA(ActorTaskListBubbleRowButton)
END_METADATA
