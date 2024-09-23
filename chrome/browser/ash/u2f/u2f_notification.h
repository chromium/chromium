// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_U2F_U2F_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_U2F_U2F_NOTIFICATION_H_

#include <optional>
#include <set>
#include <string>

#include "base/memory/weak_ptr.h"

namespace ash {

// Detects whether the legacy, never-officially-launched built-in U2F feature is
// enabled. If so, shows a notification to tell the user about a security issue.
class U2FNotification {
 public:
  U2FNotification();

  U2FNotification(const U2FNotification&) = delete;
  U2FNotification& operator=(const U2FNotification&) = delete;

  ~U2FNotification();

  // Asynchronously checks whether the legacy implementation is enabled and if
  // so, displays a notification.
  void Check();

 private:
  // Checks status given the current U2F flags.
  void CheckStatus(std::optional<std::set<std::string>> flags);

  // Shows the notification.
  void ShowNotification();

  // Handles clicks on the notification.
  void OnNotificationClick(const std::optional<int> button_index);

  base::WeakPtrFactory<U2FNotification> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_U2F_U2F_NOTIFICATION_H_
