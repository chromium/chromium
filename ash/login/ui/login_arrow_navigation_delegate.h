// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_ARROW_NAVIGATION_DELEGATE_H_
#define ASH_LOGIN_UI_LOGIN_ARROW_NAVIGATION_DELEGATE_H_

#include "ash/ash_export.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

class ASH_EXPORT LoginArrowNavigationDelegate {
 public:
  LoginArrowNavigationDelegate() = default;
  LoginArrowNavigationDelegate(const LoginArrowNavigationDelegate&) = delete;
  virtual ~LoginArrowNavigationDelegate() = default;
  bool HandleKeyEvent(const ui::KeyEvent& key_event);

 protected:
  virtual bool HandleKeyLeft() = 0;
  virtual bool HandleKeyRight() = 0;
  virtual bool HandleKeyUp() = 0;
  virtual bool HandleKeyDown() = 0;
};

class ASH_EXPORT LoginScreenArrowNavigationDelegate
    : public LoginArrowNavigationDelegate {
 public:
  LoginScreenArrowNavigationDelegate() = default;
  LoginScreenArrowNavigationDelegate(
      const LoginScreenArrowNavigationDelegate&) = delete;
  ~LoginScreenArrowNavigationDelegate() override = default;

 protected:
  bool HandleKeyLeft() override;
  bool HandleKeyRight() override;
  bool HandleKeyUp() override;
  bool HandleKeyDown() override;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_ARROW_NAVIGATION_DELEGATE_H_
