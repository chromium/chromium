// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_PROVIDER_FACTORY_H_

#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

class MacNotificationProviderFactory {
 public:
  explicit MacNotificationProviderFactory(bool in_process);
  MacNotificationProviderFactory(const MacNotificationProviderFactory&) =
      delete;
  MacNotificationProviderFactory& operator=(
      const MacNotificationProviderFactory&) = delete;
  virtual ~MacNotificationProviderFactory();

  // Launches a new provider. Virtual so it can be overridden in tests.
  virtual mojo::Remote<mac_notifications::mojom::MacNotificationProvider>
  LaunchProvider();

  bool in_process() const { return in_process_; }

 private:
  const bool in_process_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MAC_NOTIFICATION_PROVIDER_FACTORY_H_
