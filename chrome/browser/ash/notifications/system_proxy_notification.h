// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_SYSTEM_PROXY_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_SYSTEM_PROXY_NOTIFICATION_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/system_proxy/system_proxy_service.pb.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

// SystemProxyNotification manages the notification informing the user that
// System-proxy requires user credentials to authenticate to the remote web
// proxy.
class SystemProxyNotification {
 public:
  using OnClickCallback =
      base::OnceCallback<void(const system_proxy::ProtectionSpace&, bool)>;

  SystemProxyNotification(const system_proxy::ProtectionSpace& protection_space,
                          bool show_error,
                          OnClickCallback callback);
  SystemProxyNotification(const SystemProxyNotification&) = delete;
  SystemProxyNotification& operator=(const SystemProxyNotification&) = delete;
  ~SystemProxyNotification();

  void Show();
  void Close();

 private:
  void OnClick();

  const system_proxy::ProtectionSpace protection_space_;
  const bool show_error_;
  OnClickCallback on_click_callback_;

  std::unique_ptr<message_center::Notification> notification_;

  base::WeakPtrFactory<SystemProxyNotification> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_SYSTEM_PROXY_NOTIFICATION_H_
