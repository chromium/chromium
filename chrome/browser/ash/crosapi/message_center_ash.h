// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_MESSAGE_CENTER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_MESSAGE_CENTER_ASH_H_

#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi message center interface. Lives in ash-chrome on the
// UI thread. Shows notifications in response to mojo IPCs from lacros-chrome.
// Sends reply IPCs when the user interacts with the notifications.
class MessageCenterAsh : public mojom::MessageCenter {
 public:
  MessageCenterAsh();
  MessageCenterAsh(const MessageCenterAsh&) = delete;
  MessageCenterAsh& operator=(const MessageCenterAsh&) = delete;
  ~MessageCenterAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::MessageCenter> receiver);

  // crosapi::mojom::MessageCenter:
  void DisplayNotification(
      mojom::NotificationPtr notification,
      mojo::PendingRemote<mojom::NotificationDelegate> delegate) override;
  void CloseNotification(const std::string& id) override;
  void GetDisplayedNotifications(
      GetDisplayedNotificationsCallback callback) override;

 private:
  mojo::ReceiverSet<mojom::MessageCenter> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_MESSAGE_CENTER_ASH_H_
