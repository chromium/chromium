// Copyright 2020 The Chromium Authors
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
  SystemLabelButton(PressedCallback callback,
                    const std::u16string& text,
                    bool multiline = false);
  SystemLabelButton(const SystemLabelButton&) = delete;
  SystemLabelButton& operator=(const SystemLabelButton&) = delete;
  ~SystemLabelButton() override = default;

  // views::LabelButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  void OnThemeChanged() override;

  void SetBackgroundAndFont(bool alert_mode);

 private:
  bool alert_mode_ = false;

  // Absurd color to show the developer that background color has not been
  // initialized properly.
  SkColor background_color_ = SK_ColorGREEN;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_SYSTEM_LABEL_BUTTON_H_
