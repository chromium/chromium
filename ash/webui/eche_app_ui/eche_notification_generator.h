// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_NOTIFICATION_GENERATOR_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_NOTIFICATION_GENERATOR_H_

#include <string>

#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {
namespace eche_app {

class LaunchAppHelper;

// Implements the ShowNotification interface to allow WebUI show the native
// notification.
class EcheNotificationGenerator : public mojom::NotificationGenerator {
 public:
  explicit EcheNotificationGenerator(LaunchAppHelper* launch_app_helper);
  ~EcheNotificationGenerator() override;

  EcheNotificationGenerator(const EcheNotificationGenerator&) = delete;
  EcheNotificationGenerator& operator=(const EcheNotificationGenerator&) =
      delete;

  // mojom::NotificationGenerator:
  void ShowNotification(const std::u16string& title,
                        const std::u16string& message,
                        mojom::WebNotificationType type) override;

  void Bind(mojo::PendingReceiver<mojom::NotificationGenerator> receiver);

 private:
  mojo::Receiver<mojom::NotificationGenerator> notification_receiver_{this};
  LaunchAppHelper* launch_app_helper_;
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_NOTIFICATION_GENERATOR_H_
