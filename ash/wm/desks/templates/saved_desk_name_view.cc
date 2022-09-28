// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_name_view.h"

#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "base/cxx17_backports.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The font size increase for the name view. The default font size is 12, so
// this will make the name view font size 16.
constexpr int kNameFontSizeDeltaDp = 4;

// The distance from between the name view and its associated focus ring.
constexpr int kFocusRingGapDp = 2;

#if DCHECK_IS_ON()
bool IsSavedDeskLibraryWidget(const views::Widget* widget) {
  if (!widget)
    return false;

  auto* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller->InOverviewSession())
    return false;

  auto* session = overview_controller->overview_session();
  for (const auto& grid : session->grid_list()) {
    if (widget == grid->saved_desk_library_widget())
      return true;
  }

  return false;
}
#endif  // DCHECK_IS_ON()

}  // namespace

SavedDeskNameView::SavedDeskNameView() {
  SetFontList(GetFontList().Derive(kNameFontSizeDeltaDp, gfx::Font::NORMAL,
                                   gfx::Font::Weight::MEDIUM));

  // The focus ring is created in `DesksTextfield`'s constructor.
  views::FocusRing* focus_ring = views::FocusRing::Get(this);
  DCHECK(focus_ring);
  focus_ring->SetHaloInset(-kFocusRingGapDp);
}

SavedDeskNameView::~SavedDeskNameView() = default;

// static
void SavedDeskNameView::CommitChanges(views::Widget* widget) {
  // TODO(crbug.com/1277302): Refactor this logic to be shared with
  // `DeskNameView::CommitChanges`.
#if DCHECK_IS_ON()
  DCHECK(IsSavedDeskLibraryWidget(widget));
#endif  // DCHECK_IS_ON()

  auto* focus_manager = widget->GetFocusManager();
  focus_manager->ClearFocus();
  // Avoid having the focus restored to the same `SavedDeskNameView` when
  // the desks templates grid widget is refocused.
  focus_manager->SetStoredFocusView(nullptr);
}

void SavedDeskNameView::OnContentsChanged() {
  PreferredSizeChanged();
}

gfx::Size SavedDeskNameView::CalculatePreferredSize() const {
  return gfx::Size(DesksTextfield::CalculatePreferredSize().width(),
                   kSavedDeskNameViewHeight);
}

void SavedDeskNameView::OnGestureEvent(ui::GestureEvent* event) {
  DesksTextfield::OnGestureEvent(event);
  // Stop propagating this event so that the parent of `this`, which is a button
  // does not get the event.
  event->StopPropagation();
}

void SavedDeskNameView::SetViewName(const std::u16string& name) {
  SetText(temporary_name_.value_or(name));
  PreferredSizeChanged();
}

BEGIN_METADATA(SavedDeskNameView, DesksTextfield)
END_METADATA

}  // namespace ash
