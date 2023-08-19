// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SYSTEM_TEXTFIELD_CONTROLLER_H_
#define ASH_STYLE_SYSTEM_TEXTFIELD_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/style/system_textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace ash {

// Defines the following textfield behaviors of system UI. Note that the
// controller can only be set to one textfield at a time:
// - When the textfield just gets focused with Tab key, it will not be activated
// until the user presses RETURN.
// - Clicking the textfield will both focus and activate the textfield. Single
// clicking will highlight all the text.
// - While editing the textfield, pressing RETURN will commit the changes and
// deactivate the textfield but keep focus.
// - While editing the textfield, pressing ESCAPE will discard the changes and
// deactivate the textfield but keep focus.
// - The focus ring would only show on active.
class ASH_EXPORT SystemTextfieldController : public views::TextfieldController {
 public:
  explicit SystemTextfieldController(SystemTextfield* textfield);
  SystemTextfieldController(const SystemTextfieldController&) = delete;
  SystemTextfieldController& operator=(const SystemTextfieldController&) =
      delete;
  ~SystemTextfieldController() override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;
  bool HandleGestureEvent(views::Textfield* sender,
                          const ui::GestureEvent& gesture_event) override;

 private:
  // The textfield that the controller binds with.
  raw_ptr<SystemTextfield> const textfield_;
  // Indicates if selecting all text should be deferred.
  bool defer_select_all_ = false;
};

}  // namespace ash

#endif  // ASH_STYLE_SYSTEM_TEXTFIELD_CONTROLLER_H_
