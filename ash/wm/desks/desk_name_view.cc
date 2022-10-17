// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_name_view.h"

#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kDeskNameViewHorizontalPadding = 6;

bool IsDesksBarWidget(const views::Widget* widget) {
  if (!widget)
    return false;

  auto* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller->InOverviewSession())
    return false;

  auto* session = overview_controller->overview_session();
  for (const auto& grid : session->grid_list()) {
    if (widget == grid->desks_widget())
      return true;
  }

  return false;
}

}  // namespace

DeskNameView::DeskNameView(DeskMiniView* mini_view) : mini_view_(mini_view) {
  views::Builder<DeskNameView>(this)
      .SetBorder(views::CreateEmptyBorder(
          gfx::Insets::VH(0, kDeskNameViewHorizontalPadding)))
      .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER)
      .BuildChildren();
}

DeskNameView::~DeskNameView() = default;

// static
void DeskNameView::CommitChanges(views::Widget* widget) {
  DCHECK(IsDesksBarWidget(widget));

  auto* focus_manager = widget->GetFocusManager();
  focus_manager->ClearFocus();
  // Avoid having the focus restored to the same DeskNameView when the desks bar
  // widget is refocused, e.g. when the new desk button is pressed.
  focus_manager->SetStoredFocusView(nullptr);
}

void DeskNameView::OnViewHighlighted() {
  if (!HasFocus()) {
    // When the highlight is the result of tabbing, as opposed to clicking or
    // chromevoxing, the name view will not have focus, so the user should be
    // told how to focus and edit the field.
    Shell::Get()
        ->accessibility_controller()
        ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringUTF8(
            IDS_ASH_DESKS_NAME_HIGHLIGHT_NOTIFICATION));
  }

  DesksTextfield::OnViewHighlighted();
  mini_view_->owner_bar()->ScrollToShowMiniViewIfNecessary(mini_view_);
}

BEGIN_METADATA(DeskNameView, DesksTextfield)
END_METADATA

}  // namespace ash
