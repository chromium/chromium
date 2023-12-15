// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_DELETE_EDIT_SHORTCUT_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_DELETE_EDIT_SHORTCUT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace views {
class NonClientFrameView;
}

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
class DeleteEditShortcut : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(DeleteEditShortcut);
  DeleteEditShortcut(DisplayOverlayController* controller,
                     ActionViewListItem* anchor_view);
  DeleteEditShortcut(const DeleteEditShortcut&) = delete;
  DeleteEditShortcut& operator=(const DeleteEditShortcut&) = delete;
  ~DeleteEditShortcut() override;

  void UpdateAnchorView(ActionViewListItem* anchor_view);

 private:
  friend class DeleteEditShortcutTest;

  // Handle button functions.
  void OnEditButtonPressed();
  void OnDeleteButtonPressed();

  // views::DialogDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;

  // views::View:
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnThemeChanged() override;

  // DisplayOverlayController owns this class, no need to deallocate.
  const raw_ptr<DisplayOverlayController> controller_ = nullptr;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_DELETE_EDIT_SHORTCUT_H_
