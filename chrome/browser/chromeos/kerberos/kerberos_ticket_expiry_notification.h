// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KERBEROS_KERBEROS_TICKET_EXPIRY_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_KERBEROS_KERBEROS_TICKET_EXPIRY_NOTIFICATION_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace chromeos {
namespace kerberos_ticket_expiry_notification {

using ClickCallback =
    base::RepeatingCallback<void(const std::string& principal_name)>;

// Shows the ticket expiry notification for the given |principal_name|.
// |click_callback| is called when the user clicks on the notification.
void Show(Profile* profile,
          const std::string& principal_name,
          ClickCallback click_callback);

// Closes the ticket expiry notification.
void Close(Profile* profile);

}  // namespace kerberos_ticket_expiry_notification
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KERBEROS_KERBEROS_TICKET_EXPIRY_NOTIFICATION_H_
