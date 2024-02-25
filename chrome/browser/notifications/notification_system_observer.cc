// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_system_observer.h"

#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_content_browser_client_extensions_part.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "extensions/common/extension.h"

NotificationSystemObserver::NotificationSystemObserver(
    NotificationUIManager* ui_manager)
    : ui_manager_(ui_manager) {
  DCHECK(ui_manager_);
  // base::Unretained(this) is safe here as this object owns
  // |on_app_terminating_subscription_| and the callback won't be invoked
  // after the subscription is destroyed.
  on_app_terminating_subscription_ =
      browser_shutdown::AddAppTerminatingCallback(
          base::BindOnce(&NotificationSystemObserver::OnAppTerminating,
                         base::Unretained(this)));
  for (auto* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    if (extensions::ChromeContentBrowserClientExtensionsPart::
            AreExtensionsDisabledForProfile(profile)) {
      continue;
    }

    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile);
    DCHECK(registry);
    extension_registry_observations_.AddObservation(registry);
  }

  g_browser_process->profile_manager()->AddObserver(this);
}

NotificationSystemObserver::~NotificationSystemObserver() {
  g_browser_process->profile_manager()->RemoveObserver(this);
}

void NotificationSystemObserver::OnAppTerminating() {
  ui_manager_->StartShutdown();
}

void NotificationSystemObserver::OnProfileAdded(Profile* profile) {
  DCHECK(!profile->IsOffTheRecord());

  if (extensions::ChromeContentBrowserClientExtensionsPart::
          AreExtensionsDisabledForProfile(profile)) {
    return;
  }
  auto* registry = extensions::ExtensionRegistry::Get(profile);
  DCHECK(registry);
  // If |this| was created after the profile was created but before the
  // ADDED notification was sent, we may be already observing it. |this| is
  // created lazily so it's not easy to predict construction order.
  if (!extension_registry_observations_.IsObservingSource(registry))
    extension_registry_observations_.AddObservation(registry);
}

void NotificationSystemObserver::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  ui_manager_->CancelAllBySourceOrigin(extension->url());
}

void NotificationSystemObserver::OnShutdown(
    extensions::ExtensionRegistry* registry) {
  extension_registry_observations_.RemoveObservation(registry);
}
