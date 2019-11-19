// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_system_observer.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/notification_service.h"
#include "extensions/common/extension.h"

NotificationSystemObserver::NotificationSystemObserver(
    NotificationUIManager* ui_manager)
    : ui_manager_(ui_manager) {
  DCHECK(ui_manager_);
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_ADDED,
                 content::NotificationService::AllSources());
  for (auto* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    extension_registry_observer_.Add(
        extensions::ExtensionRegistry::Get(profile));
  }
}

NotificationSystemObserver::~NotificationSystemObserver() {
}

void NotificationSystemObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_APP_TERMINATING:
      ui_manager_->StartShutdown();
      break;
    case chrome::NOTIFICATION_PROFILE_ADDED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      DCHECK(!profile->IsOffTheRecord());
      auto* registry = extensions::ExtensionRegistry::Get(profile);
      // If |this| was created after the profile was created but before the
      // ADDED notification was sent, we may be already observing it. |this| is
      // created lazily so it's not easy to predict construction order.
      if (!extension_registry_observer_.IsObserving(registry))
        extension_registry_observer_.Add(registry);
      break;
    }
    default:
      NOTREACHED();
  }
}

void NotificationSystemObserver::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  ui_manager_->CancelAllBySourceOrigin(extension->url());
}

void NotificationSystemObserver::OnShutdown(
    extensions::ExtensionRegistry* registry) {
  extension_registry_observer_.Remove(registry);
}
