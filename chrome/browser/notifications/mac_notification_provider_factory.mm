// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/mac_notification_provider_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/service_sandbox_type.h"
#include "chrome/common/child_process_host_flags.h"
#include "chrome/services/mac_notifications/mac_notification_provider_impl.h"
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

MacNotificationProviderFactory::MacNotificationProviderFactory() = default;

MacNotificationProviderFactory::~MacNotificationProviderFactory() = default;

mojo::Remote<mac_notifications::mojom::MacNotificationProvider>
MacNotificationProviderFactory::LaunchProvider(bool in_process) {
  return in_process ? LaunchInProcessProvider() : LaunchOutOfProcessProvider();
}
