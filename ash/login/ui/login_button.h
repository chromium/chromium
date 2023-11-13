// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_BUTTON_H_
#define ASH_LOGIN_UI_LOGIN_BUTTON_H_

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

namespace ash {

// This class adds ripple effects for touch targets in the lockscreen.
class ASH_EXPORT LoginButton : public views::ImageButton {
  METADATA_HEADER(LoginButton, views::ImageButton)

 public:
  explicit LoginButton(PressedCallback callback);

  LoginButton(const LoginButton&) = delete;
  LoginButton& operator=(const LoginButton&) = delete;

  ~LoginButton() override;

  base::WeakPtr<LoginButton> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 protected:
  virtual int GetInkDropRadius() const;

 private:
  base::WeakPtrFactory<LoginButton> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_BUTTON_H_
