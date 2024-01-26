// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_INSTALL_ATTRIBUTES_ERROR_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_INSTALL_ATTRIBUTES_ERROR_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class InstallAttributesErrorView;

// Controller for the install attributes error screen.
class InstallAttributesErrorScreen : public BaseScreen {
 public:
  explicit InstallAttributesErrorScreen(
      base::WeakPtr<InstallAttributesErrorView> view);
  InstallAttributesErrorScreen(const InstallAttributesErrorScreen&) = delete;
  InstallAttributesErrorScreen& operator=(const InstallAttributesErrorScreen&) =
      delete;
  ~InstallAttributesErrorScreen() override;

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::WeakPtr<InstallAttributesErrorView> view_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_INSTALL_ATTRIBUTES_ERROR_SCREEN_H_
