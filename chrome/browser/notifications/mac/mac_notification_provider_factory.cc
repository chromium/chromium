// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/mac/mac_notification_provider_factory.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/child_process_host_flags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/services/mac_notifications/mac_notification_provider_impl.h"
#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace {

// Launches a new utility process that can display alert notifications.
mojo::Remote<mac_notifications::mojom::MacNotificationProvider>
LaunchOutOfProcessProvider() {
  return content::ServiceProcessHost::Launch<
      mac_notifications::mojom::MacNotificationProvider>(
      content::ServiceProcessHost::Options()
          .WithDisplayName("Notification Service")
          .WithExtraCommandLineSwitches({switches::kMessageLoopTypeUi})
          .WithChildFlags(chrome::kChildProcessHelperAlerts)
          .Pass());
}

// Binds the |receiver| to a new provider on a background task runner.
void BindInProcessProvider(
    mojo::PendingReceiver<mac_notifications::mojom::MacNotificationProvider>
        receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<mac_notifications::MacNotificationProviderImpl>(),
      std::move(receiver));
}

// Launches an in process service that can display banner notifications.
mojo::Remote<mac_notifications::mojom::MacNotificationProvider>
LaunchInProcessProvider() {
  mojo::Remote<mac_notifications::mojom::MacNotificationProvider> remote;
  base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&BindInProcessProvider,
                                remote.BindNewPipeAndPassReceiver()));
  return remote;
}

}  // namespace

MacNotificationProviderFactory::MacNotificationProviderFactory(
    mac_notifications::NotificationStyle notification_style,
    const webapps::AppId& web_app_id)
    : notification_style_(notification_style), web_app_id_(web_app_id) {
  CHECK_NE(
      notification_style_ == mac_notifications::NotificationStyle::kAppShim,
      web_app_id_.empty());
  if (notification_style_ == mac_notifications::NotificationStyle::kAppShim) {
    CHECK(base::FeatureList::IsEnabled(
        features::kAppShimNotificationAttribution));
  }
}

MacNotificationProviderFactory::~MacNotificationProviderFactory() = default;

mojo::Remote<mac_notifications::mojom::MacNotificationProvider>
MacNotificationProviderFactory::LaunchProvider() {
  switch (notification_style_) {
    case mac_notifications::NotificationStyle::kBanner:
      return LaunchInProcessProvider();
    case mac_notifications::NotificationStyle::kAlert:
      return LaunchOutOfProcessProvider();
    case mac_notifications::NotificationStyle::kAppShim:
      return apps::AppShimManager::Get()->LaunchNotificationProvider(
          web_app_id_);
  }
  NOTREACHED_IN_MIGRATION();
}
