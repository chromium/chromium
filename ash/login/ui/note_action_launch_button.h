// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_NOTE_ACTION_LAUNCH_BUTTON_H_
#define ASH_LOGIN_UI_NOTE_ACTION_LAUNCH_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/shell.h"
#include "ash/tray_action/tray_action.h"
#include "ash/tray_action/tray_action_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

namespace mojom {
enum class TrayActionState;
}

// View for lock sreen UI element for launching note taking action handler app.
// The element is an image button with a semi-transparent bubble background,
// which is expanded upon hovering/focusing the element.
// The bubble is a quarter of a circle with the center in top right corner of
// the view (in LTR layout).
// The button is only visible if the lock screen note taking action is available
// (the view observes the action availability using login data dispatcher, and
// updates itself accordingly).
class ASH_EXPORT NoteActionLaunchButton : public NonAccessibleView {
  METADATA_HEADER(NoteActionLaunchButton, NonAccessibleView)

 public:
  // Used by tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(NoteActionLaunchButton* launch_button);

    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    ~TestApi();

    // Gets the foreground, action image button view.
    const views::View* ActionButtonView() const;

    // Gets the background view.
    const views::View* BackgroundView() const;

   private:
    raw_ptr<NoteActionLaunchButton> launch_button_;
  };

  explicit NoteActionLaunchButton(
      mojom::TrayActionState initial_note_action_state);

  NoteActionLaunchButton(const NoteActionLaunchButton&) = delete;
  NoteActionLaunchButton& operator=(const NoteActionLaunchButton&) = delete;

  ~NoteActionLaunchButton() override;

  // Updates the bubble visibility depending on the note taking action state.
  void UpdateVisibility(mojom::TrayActionState action_state);

 private:
  class BackgroundView;
  class ActionButton;

  // The background bubble view.
  raw_ptr<BackgroundView> background_ = nullptr;

  // The actionable image button view.
  raw_ptr<ActionButton> action_button_ = nullptr;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_NOTE_ACTION_LAUNCH_BUTTON_H_
