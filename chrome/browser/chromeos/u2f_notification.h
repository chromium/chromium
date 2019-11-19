// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_U2F_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_U2F_NOTIFICATION_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"

namespace chromeos {

// Detects whether the legacy, never-officially-launched built-in U2F feature is
// enabled. If so, shows a notification to tell the user about a security issue.
class U2FNotification {
 public:
  U2FNotification();
  ~U2FNotification();

  // Asynchronously checks whether the legacy implementation is enabled and if
  // so, displays a notification.
  void Check();

 private:
  // Checks status given the current U2F flags.
  void CheckStatus(base::Optional<std::set<std::string>> flags);

  // Shows the notification.
  void ShowNotification();

  // Handles clicks on the notification.
  void OnNotificationClick(const base::Optional<int> button_index);

  base::WeakPtrFactory<U2FNotification> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(U2FNotification);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_U2F_NOTIFICATION_H_
