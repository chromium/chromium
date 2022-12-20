// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_SYSTEM_OBSERVER_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_SYSTEM_OBSERVER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

class NotificationUIManager;

// The ProfileManagerObserver observes system status change and sends
// events to NotificationUIManager. NOTE: NotificationUIManager is deprecated,
// to be replaced by NotificationDisplayService, so this class should go away.
class NotificationSystemObserver : public ProfileManagerObserver,
                                   extensions::ExtensionRegistryObserver {
 public:
  explicit NotificationSystemObserver(NotificationUIManager* ui_manager);
  NotificationSystemObserver(const NotificationSystemObserver&) = delete;
  NotificationSystemObserver& operator=(const NotificationSystemObserver&) =
      delete;
  ~NotificationSystemObserver() override;

 protected:
  void OnAppTerminating();

  // ProfileManagerObserver override.
  void OnProfileAdded(Profile* profile) override;

  // extensions::ExtensionRegistryObserver override.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

 private:
  base::CallbackListSubscription on_app_terminating_subscription_;
  raw_ptr<NotificationUIManager> ui_manager_;

  base::ScopedMultiSourceObservation<extensions::ExtensionRegistry,
                                     extensions::ExtensionRegistryObserver>
      extension_registry_observations_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_SYSTEM_OBSERVER_H_
