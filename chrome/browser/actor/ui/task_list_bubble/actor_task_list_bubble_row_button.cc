// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_row_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/common/chrome_features.h"
#include "components/vector_icons/vector_icons.h"
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

const gfx::VectorIcon& GetRowIcon() {
#if BUILDFLAG(ENABLE_GLIC)
  if (base::FeatureList::IsEnabled(features::kGlicActorUiTaskIconV2)) {
    glic::GlicVectorIconManager::GetVectorIcon(IDR_ACTOR_AUTO_BROWSE_ICON);
  }
#endif
  return kScreensaverAutoIcon;
}
}  // namespace

ActorTaskListBubbleRowButton::ActorTaskListBubbleRowButton(
    ActorTaskListBubbleRowButtonParams params)
    : RichHoverButton(std::move(params.on_click_callback),
                      /*icon=*/
                      ui::ImageModel::FromVectorIcon(GetRowIcon(),
                                                     ui::kColorMenuIcon,
                                                     kBubbleRowIconSize),
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
