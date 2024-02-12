// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_name_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/focus_ring.h"

namespace ash {

namespace {

// The distance from between the name view and its associated focus ring.
constexpr int kFocusRingGapDp = 2;

}  // namespace

SavedDeskNameView::SavedDeskNameView()
    : DeskTextfield(SystemTextfield::Type::kLarge) {
  // The focus ring is created in `DeskTextfield`'s constructor.
  views::FocusRing* focus_ring = views::FocusRing::Get(this);
  DCHECK(focus_ring);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetHaloInset(-kFocusRingGapDp);
}

SavedDeskNameView::~SavedDeskNameView() = default;

void SavedDeskNameView::OnContentsChanged() {
  PreferredSizeChanged();
}

void SavedDeskNameView::OnGestureEvent(ui::GestureEvent* event) {
  DeskTextfield::OnGestureEvent(event);
  // Stop propagating this event so that the parent of `this`, which is a button
  // does not get the event.
  event->StopPropagation();
}

void SavedDeskNameView::SetViewName(const std::u16string& name) {
  SetText(temporary_name_.value_or(name));
  PreferredSizeChanged();
}

BEGIN_METADATA(SavedDeskNameView)
END_METADATA

}  // namespace ash
