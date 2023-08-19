// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USER_BOARD_VIEW_MOJO_H_
#define CHROME_BROWSER_ASH_LOGIN_USER_BOARD_VIEW_MOJO_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/ui/views/user_board_view.h"

namespace ash {

// UserBoardView implementation that forwards calls to ash via mojo.
class UserBoardViewMojo : public UserBoardView {
 public:
  UserBoardViewMojo();

  UserBoardViewMojo(const UserBoardViewMojo&) = delete;
  UserBoardViewMojo& operator=(const UserBoardViewMojo&) = delete;

  ~UserBoardViewMojo() override;

  // UserBoardView:
  void SetPublicSessionDisplayName(const AccountId& account_id,
                                   const std::string& display_name) override;
  void SetPublicSessionLocales(const AccountId& account_id,
                               base::Value::List locales,
                               const std::string& default_locale,
                               bool multiple_recommended_locales) override;
  void SetPublicSessionShowFullManagementDisclosure(
      bool show_full_management_disclosure) override;
  void ShowBannerMessage(const std::u16string& message,
                         bool is_warning) override;
  void SetSmartLockState(const AccountId& account_id,
                         SmartLockState state) override;
  void NotifySmartLockAuthResult(const AccountId& account_id,
                                 bool success) override;
  void SetAuthType(const AccountId& account_id,
                   proximity_auth::mojom::AuthType auth_type,
                   const std::u16string& initial_value) override;
  void SetTpmLockedState(const AccountId& account_id,
                         bool is_locked,
                         base::TimeDelta time_left) override;
  void Bind(UserSelectionScreen* screen) override {}
  void Unbind() override {}
  base::WeakPtr<UserBoardView> GetWeakPtr() override;

 private:
  base::WeakPtrFactory<UserBoardViewMojo> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USER_BOARD_VIEW_MOJO_H_
