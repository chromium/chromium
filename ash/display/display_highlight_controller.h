// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_HIGHLIGHT_CONTROLLER_H_
#define ASH_DISPLAY_DISPLAY_HIGHLIGHT_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {

// DisplayHighlightController manages which display should render display
// identification highlights. Highlights are translucent blue rectangles on the
// edges of a display.
// TODO(1091497): Consider combining DisplayHighlightController and
// DisplayAlignmentController.
class ASH_EXPORT DisplayHighlightController
    : public display::DisplayManagerObserver,
      public SessionObserver {
 public:
  DisplayHighlightController();
  ~DisplayHighlightController() override;

  // Sets the display to render the highlights on. To remove a currently-active
  // highlight, pass display::kInvalidDisplayId as |display_id|.
  void SetHighlightedDisplay(int64_t display_id);

  views::Widget* GetWidgetForTesting() { return highlight_widget_.get(); }

 private:
  // display::DisplayManagerObserver:
  void OnDidApplyDisplayChanges() override;
  void OnDisplaysInitialized() override;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;

  // Updates |highlight_| with new selected display.
  void UpdateDisplayIdentificationHighlight();

  // Widget used to render a blue highlight on the border of the specified
  // display.
  std::unique_ptr<views::Widget> highlight_widget_;

  int64_t selected_display_id_ = display::kInvalidDisplayId;

  // Tracks if the screen is locked in order to remove the highlight when screen
  // locks and show it when it unlocks.
  bool is_locked_;
};
}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_HIGHLIGHT_CONTROLLER_H_
