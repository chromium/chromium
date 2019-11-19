// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_VIEWS_USER_BOARD_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_VIEWS_USER_BOARD_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"

class AccountId;

namespace chromeos {

class UserSelectionScreen;

// TODO(jdufault): Rename UserBoardView to UserSelectionView. See
// crbug.com/672142.

// Interface between user board screen and its representation, either WebUI
// or Views one.
class UserBoardView {
 public:
  constexpr static StaticOobeScreenId kScreenId{"userBoard"};

  virtual ~UserBoardView() {}

  virtual void Bind(UserSelectionScreen* screen) = 0;
  virtual void Unbind() = 0;

  virtual base::WeakPtr<UserBoardView> GetWeakPtr() = 0;

  virtual void SetPublicSessionDisplayName(const AccountId& account_id,
                                           const std::string& display_name) = 0;
  virtual void SetPublicSessionLocales(const AccountId& account_id,
                                       std::unique_ptr<base::ListValue> locales,
                                       const std::string& default_locale,
                                       bool multiple_recommended_locales) = 0;
  virtual void SetPublicSessionShowFullManagementDisclosure(
      bool show_full_management_disclosure) = 0;
  virtual void ShowBannerMessage(const base::string16& message,
                                 bool is_warning) = 0;
  virtual void ShowUserPodCustomIcon(
      const AccountId& account_id,
      const proximity_auth::ScreenlockBridge::UserPodCustomIconOptions&
          icon) = 0;
  virtual void HideUserPodCustomIcon(const AccountId& account_id) = 0;
  virtual void SetAuthType(const AccountId& account_id,
                           proximity_auth::mojom::AuthType auth_type,
                           const base::string16& initial_value) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_VIEWS_USER_BOARD_VIEW_H_
