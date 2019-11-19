// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_BOARD_VIEW_MOJO_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_BOARD_VIEW_MOJO_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/ui/views/user_board_view.h"

namespace chromeos {

// UserBoardView implementation that forwards calls to ash via mojo.
class UserBoardViewMojo : public UserBoardView {
 public:
  UserBoardViewMojo();
  ~UserBoardViewMojo() override;

  // UserBoardView:
  void SetPublicSessionDisplayName(const AccountId& account_id,
                                   const std::string& display_name) override;
  void SetPublicSessionLocales(const AccountId& account_id,
                               std::unique_ptr<base::ListValue> locales,
                               const std::string& default_locale,
                               bool multiple_recommended_locales) override;
  void SetPublicSessionShowFullManagementDisclosure(
      bool show_full_management_disclosure) override;
  void ShowBannerMessage(const base::string16& message,
                         bool is_warning) override;
  void ShowUserPodCustomIcon(
      const AccountId& account_id,
      const proximity_auth::ScreenlockBridge::UserPodCustomIconOptions& icon)
      override;
  void HideUserPodCustomIcon(const AccountId& account_id) override;
  void SetAuthType(const AccountId& account_id,
                   proximity_auth::mojom::AuthType auth_type,
                   const base::string16& initial_value) override;
  void Bind(UserSelectionScreen* screen) override {}
  void Unbind() override {}
  base::WeakPtr<UserBoardView> GetWeakPtr() override;

 private:
  base::WeakPtrFactory<UserBoardViewMojo> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UserBoardViewMojo);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_BOARD_VIEW_MOJO_H_
