// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_AUTH_FACTORS_POLICY_LOCAL_AUTH_FACTORS_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_ASH_LOGIN_AUTH_FACTORS_POLICY_LOCAL_AUTH_FACTORS_NOTIFICATION_DELEGATE_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace ash {

class LocalAuthFactorsNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  explicit LocalAuthFactorsNotificationDelegate(Profile* profile);
  LocalAuthFactorsNotificationDelegate(
      const LocalAuthFactorsNotificationDelegate&) = delete;
  LocalAuthFactorsNotificationDelegate& operator=(
      const LocalAuthFactorsNotificationDelegate&) = delete;

  // message_center::NotificationDelegate:
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

 protected:
  ~LocalAuthFactorsNotificationDelegate() override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_AUTH_FACTORS_POLICY_LOCAL_AUTH_FACTORS_NOTIFICATION_DELEGATE_H_
