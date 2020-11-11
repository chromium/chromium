// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_SYSTEM_LABEL_BUTTON_H_
#define ASH_LOGIN_UI_SYSTEM_LABEL_BUTTON_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

// SystemLabelButton provides styled buttons with label for the login screen.
class ASH_EXPORT SystemLabelButton : public views::LabelButton {
 public:
  enum class DisplayType { DEFAULT, ALERT_NO_ICON, ALERT_WITH_ICON };

  SystemLabelButton(PressedCallback callback,
                    const base::string16& text,
                    DisplayType display_type,
                    bool multiline = false);
  SystemLabelButton(const SystemLabelButton&) = delete;
  SystemLabelButton& operator=(const SystemLabelButton&) = delete;
  ~SystemLabelButton() override = default;

  // views::LabelButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;

  // Switch display type from {DEFAULT, ALERT_NO_ICON} to
  // {DEFAULT, ALERT_NO_ICON}. We can't change display type from or to
  // ALERT_WITH_ICON once it has been set (no UX interest to do so right now).
  void SetDisplayType(DisplayType display_type);

 private:
  // Mode could be either default or alert. This methods set the background and
  // font accordingly.
  void SetAlertMode(bool alert_mode);

  // Absurd color to show the developer that background color has not been
  // initialized properly.
  SkColor background_color_ = SK_ColorGREEN;
  // Used only to ensure that we do not call SetDisplayType when the current
  // display type is ALERT_WITH_ICON.
  DisplayType display_type_ = DisplayType::DEFAULT;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_SYSTEM_LABEL_BUTTON_H_
