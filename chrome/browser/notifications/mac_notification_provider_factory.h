// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_PROVIDER_FACTORY_H_

#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/services/mac_notifications/public/cpp/mac_notification_metrics.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

class MacNotificationProviderFactory {
 public:
  using ProcessType = mac_notifications::ProcessType;

  explicit MacNotificationProviderFactory(
      ProcessType process_type,
      const web_app::AppId& web_app_id = {});
  MacNotificationProviderFactory(const MacNotificationProviderFactory&) =
      delete;
  MacNotificationProviderFactory& operator=(
      const MacNotificationProviderFactory&) = delete;
  virtual ~MacNotificationProviderFactory();

  // Launches a new provider. Virtual so it can be overridden in tests.
  virtual mojo::Remote<mac_notifications::mojom::MacNotificationProvider>
  LaunchProvider();

  ProcessType process_type() const { return process_type_; }

 private:
  const ProcessType process_type_;
  const web_app::AppId web_app_id_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_PROVIDER_FACTORY_H_
