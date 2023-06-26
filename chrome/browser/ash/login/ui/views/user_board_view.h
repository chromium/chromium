// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_UI_VIEWS_USER_BOARD_VIEW_H_
#define CHROME_BROWSER_ASH_LOGIN_UI_VIEWS_USER_BOARD_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"

class AccountId;

namespace ash {

class UserSelectionScreen;
enum class SmartLockState;

// TODO(jdufault): Rename UserBoardView to UserSelectionView. See
// crbug.com/672142.

// Interface between user board screen and its representation, either WebUI
// or Views one.
class UserBoardView {
 public:
  virtual ~UserBoardView() {}

  virtual void Bind(UserSelectionScreen* screen) = 0;
  virtual void Unbind() = 0;

  virtual base::WeakPtr<UserBoardView> GetWeakPtr() = 0;

  virtual void SetPublicSessionDisplayName(const AccountId& account_id,
                                           const std::string& display_name) = 0;
  virtual void SetPublicSessionLocales(const AccountId& account_id,
                                       base::Value::List locales,
                                       const std::string& default_locale,
                                       bool multiple_recommended_locales) = 0;
  virtual void SetPublicSessionShowFullManagementDisclosure(
      bool show_full_management_disclosure) = 0;
  virtual void ShowBannerMessage(const std::u16string& message,
                                 bool is_warning) = 0;
  virtual void SetSmartLockState(const AccountId& account_id,
                                 SmartLockState state) = 0;
  virtual void NotifySmartLockAuthResult(const AccountId& account_id,
                                         bool success) = 0;
  virtual void SetAuthType(const AccountId& account_id,
                           proximity_auth::mojom::AuthType auth_type,
                           const std::u16string& initial_value) = 0;

  virtual void SetTpmLockedState(const AccountId& account_id,
                                 bool is_locked,
                                 base::TimeDelta time_left) = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_UI_VIEWS_USER_BOARD_VIEW_H_
