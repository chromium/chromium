// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_DELETE_EDIT_SHORTCUT_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_DELETE_EDIT_SHORTCUT_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace arc::input_overlay {

class ActionViewListItem;
class DisplayOverlayController;

// DeleteEditShortcut displays a shortcut to either edit to the action or delete
// the action.
//
// View looks like this:
// +------+
// ||icon||
// |------|
// ||icon||
// +------+
class DeleteEditShortcut : public views::View, public views::ViewObserver {
 public:
  METADATA_HEADER(DeleteEditShortcut);
  DeleteEditShortcut(DisplayOverlayController* controller,
                     ActionViewListItem* anchor_view);
  DeleteEditShortcut(const DeleteEditShortcut&) = delete;
  DeleteEditShortcut& operator=(const DeleteEditShortcut&) = delete;
  ~DeleteEditShortcut() override;

  ActionViewListItem* anchor_view() { return anchor_view_; }

  void UpdateAnchorView(ActionViewListItem* anchor_view);

  // views::View:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // views::ViewObserver:
  void OnViewRemovedFromWidget(views::View*) override;
  void OnViewBoundsChanged(views::View*) override;

 private:
  friend class EditingListTest;

  class AnchorViewObserver;

  // Build related views.
  void Init();

  // Handle button functions.
  void OnEditButtonPressed();
  void OnDeleteButtonPressed();

  void UpdateWidget();

  // DisplayOverlayController owns this class, no need to deallocate.
  const raw_ptr<DisplayOverlayController> controller_ = nullptr;
  raw_ptr<ActionViewListItem> anchor_view_ = nullptr;

  // Watches for the anchor view to be destroyed or removed from its widget.
  // Prevents the delete edit shortcut from lingering after its anchor is
  // invalid, which can cause strange behavior.
  base::ScopedObservation<View, ViewObserver> observation_{this};
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_DELETE_EDIT_SHORTCUT_H_
