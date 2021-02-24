// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/mac_notification_provider_factory.h"

#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_switches.h"

namespace {

// Launches a new utility process that can display alert notifications.
mojo::Remote<mac_notifications::mojom::MacNotificationProvider>
LaunchOutOfProcessProvider() {
  return content::ServiceProcessHost::Launch<
      mac_notifications::mojom::MacNotificationProvider>(
      content::ServiceProcessHost::Options()
          .WithExtraCommandLineSwitches({switches::kMessageLoopTypeUi})
          // TODO(knollr): Set the correct flags so the helper launches via
          // the app which has set the alert notifications style:
          //.WithChildFlags(chrome::kChildProcessHelperAlerts)
          .Pass());
}

}  // namespace

MacNotificationProviderFactory::MacNotificationProviderFactory() = default;
MacNotificationProviderFactory::~MacNotificationProviderFactory() = default;

mojo::Remote<mac_notifications::mojom::MacNotificationProvider>
MacNotificationProviderFactory::LaunchProvider() {
  return LaunchOutOfProcessProvider();
}
